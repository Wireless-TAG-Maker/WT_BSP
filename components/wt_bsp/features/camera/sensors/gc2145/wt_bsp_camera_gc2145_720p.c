/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wt_bsp_camera_interface.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_720P

#include <inttypes.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/jpeg_encode.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_check.h"
#include "esp_clk_tree.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_private/esp_clk_tree_common.h"
#include "esp_private/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "gc2145_sensor.h"
#include "soc/gpio_sig_map.h"
#include "soc/hp_sys_clkrst_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/lcd_cam_reg.h"
#include "soc/soc.h"

#define GC2145_720P_WIDTH 1280
#define GC2145_720P_HEIGHT 720
#define GC2145_720P_XCLK_HZ 24000000U
#define CAMERA_BUFFER_COUNT 2
#define CAMERA_720P_APLL_HZ 120000000U
#define CAMERA_DMA_BURST_BYTES 64
#define CAMERA_CORE_DIVIDER_MASK (HP_SYS_CLKRST_REG_LCDCAM_CLK_DIV_NUM_M | \
                                 HP_SYS_CLKRST_REG_LCDCAM_CLK_DIV_NUMERATOR_M | \
                                 HP_SYS_CLKRST_REG_LCDCAM_CLK_DIV_DENOMINATOR_M)

typedef struct {
    uint32_t index;
    int64_t timestamp_us;
} completed_frame_t;

typedef struct {
    esp_cam_ctlr_handle_t controller;
    i2c_master_bus_handle_t i2c_bus;
    gc2145_sensor_t sensor;
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
    uint8_t *buffers[CAMERA_BUFFER_COUNT];
    uint32_t buffer_count;
    uint32_t width;
    uint32_t height;
    size_t frame_bytes;
    uint32_t startup_discard;
    uint32_t saved_core_divider;
    uint32_t held_index;
    TickType_t wait_ticks;
    bool owns_core_divider;
    bool owns_apll;
    bool enabled;
    bool streaming;
    bool running;
    bool frame_held;
    volatile bool stopping;
    volatile uint32_t completed;
    volatile uint32_t recycled;
    volatile uint32_t backup_requests;
    volatile uint32_t frame_errors;
#if CONFIG_PM_ENABLE
    esp_pm_lock_handle_t no_sleep_lock;
    bool no_sleep_lock_held;
#endif
} gc2145_capture_t;

typedef struct {
    uint8_t *data;
    size_t size;
    int64_t timestamp_us;
    uint32_t index;
} gc2145_frame_t;

static const char *TAG = "gc2145_720p";

static esp_err_t camera_capture_stop(gc2145_capture_t **capture);

/* DVP DMA and frame ownership */
static bool IRAM_ATTR camera_get_buffer(esp_cam_ctlr_handle_t handle,
                                        esp_cam_ctlr_trans_t *trans, void *user_data)
{
    (void)handle;
    gc2145_capture_t *camera = user_data;
    trans->buffer = NULL;
    trans->buflen = 0;
    if (camera->stopping) {
        return false;
    }

    uint32_t index;
    const bool in_isr = xPortInIsrContext();
    BaseType_t available = in_isr ? xQueueReceiveFromISR(camera->free_queue, &index, NULL) :
                          xQueueReceive(camera->free_queue, &index, 0);
    if (available != pdTRUE) {
        /* Only recycle a queued frame. A borrowed buffer is on neither queue,
         * and IDF requests the next buffer before publishing the current one. */
        completed_frame_t old_frame;
        available = in_isr ? xQueueReceiveFromISR(camera->ready_queue, &old_frame, NULL) :
                    xQueueReceive(camera->ready_queue, &old_frame, 0);
        if (available == pdTRUE) {
            index = old_frame.index;
            camera->recycled++;
        }
    }
    if (available == pdTRUE && index < camera->buffer_count) {
        trans->buffer = camera->buffers[index];
        trans->buflen = camera->frame_bytes;
    } else {
        /* IDF will receive into its private backup buffer. */
        camera->backup_requests++;
    }
    return false;
}

static bool IRAM_ATTR camera_frame_done(esp_cam_ctlr_handle_t handle,
                                        esp_cam_ctlr_trans_t *trans, void *user_data)
{
    (void)handle;
    gc2145_capture_t *camera = user_data;
    if (camera->stopping) {
        return false;
    }

    BaseType_t woken = pdFALSE;
    for (uint32_t index = 0; index < camera->buffer_count; index++) {
        if (trans->buffer == camera->buffers[index]) {
            camera->completed++;
            const completed_frame_t frame = {
                .index = index,
                .timestamp_us = esp_timer_get_time(),
            };
            /* For raw capture, IDF reports the configured extent. A matching
             * length does not prove the DMA saw every pixel of this frame. */
            if (trans->received_size != camera->frame_bytes ||
                    xQueueSendFromISR(camera->ready_queue, &frame, &woken) != pdTRUE) {
                camera->frame_errors++;
                xQueueSendFromISR(camera->free_queue, &index, &woken);
            }
            return woken == pdTRUE;
        }
    }
    camera->frame_errors++;
    return false;
}

static esp_err_t camera_return_buffer(gc2145_capture_t *camera, uint32_t index)
{
    if (index >= camera->buffer_count || xQueueSend(camera->free_queue, &index, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t camera_wait_buffer(gc2145_capture_t *camera, completed_frame_t *frame)
{
    for (;;) {
        if (xQueueReceive(camera->ready_queue, frame, camera->wait_ticks) != pdTRUE) {
            ESP_LOGW(TAG, "DVP timeout: irq=0x%" PRIx32 " frames=%" PRIu32
                     " recycled=%" PRIu32 " backup=%" PRIu32 " errors=%" PRIu32,
                     REG_READ(LCDCAM_LC_DMA_INT_RAW_REG), camera->completed,
                     camera->recycled, camera->backup_requests, camera->frame_errors);
            return ESP_ERR_TIMEOUT;
        }
        if (camera->startup_discard == 0) {
            return ESP_OK;
        }
        camera->startup_discard--;
        ESP_RETURN_ON_ERROR(camera_return_buffer(camera, frame->index), TAG, "discard startup frame");
    }
}

static esp_err_t camera_change_core_divider(gc2145_capture_t *camera, bool restore)
{
    if (restore && !camera->owns_core_divider) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    /* Both LCD_CAM consumers must be idle. A cold start can have the APB gate
     * closed, so inspect LCD_CAM peripheral registers only when it is open. */
    PERIPH_RCC_ATOMIC() {
        const uint32_t core = REG_READ(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG);
        bool idle = !(REG_READ(HP_SYS_CLKRST_LCDCAM_LCD_CTRL0_REG) & HP_SYS_CLKRST_REG_LCD_CLK_EN) &&
                    !(REG_READ(HP_SYS_CLKRST_LCDCAM_CAM_CTRL0_REG) & HP_SYS_CLKRST_REG_CAM_CLK_EN);
        if (idle && (REG_READ(HP_SYS_CLKRST_LCDCAM_CTRL0_REG) & HP_SYS_CLKRST_REG_LCDCAM_APB_CLK_EN)) {
            idle = !(REG_READ(LCDCAM_CAM_CTRL1_REG) & LCDCAM_CAM_START) &&
                   !(REG_READ(LCDCAM_LCD_USER_REG) & LCDCAM_LCD_START);
        }
        if (!idle || (core & HP_SYS_CLKRST_REG_LCDCAM_CLK_SRC_SEL_M) !=
                (1U << HP_SYS_CLKRST_REG_LCDCAM_CLK_SRC_SEL_S)) {
            ret = ESP_ERR_INVALID_STATE;
        } else {
            if (!restore) {
                camera->saved_core_divider = core & CAMERA_CORE_DIVIDER_MASK;
                camera->owns_core_divider = true;
            }
            const uint32_t target = restore ? camera->saved_core_divider : 0;
            const uint32_t gated = core & ~HP_SYS_CLKRST_REG_LCDCAM_CLK_EN;
            REG_WRITE(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG, gated);
            if (REG_READ(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG) != gated) {
                ret = ESP_FAIL;
            } else {
                const uint32_t changed = (gated & ~CAMERA_CORE_DIVIDER_MASK) | target;
                REG_WRITE(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG, changed);
                if (REG_READ(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG) != changed) {
                    ret = ESP_FAIL;
                }
            }

            /* Restore the gate state observed on entry, including after delete. */
            const uint32_t current = REG_READ(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG);
            const uint32_t final = (current & ~HP_SYS_CLKRST_REG_LCDCAM_CLK_EN) |
                                   (core & HP_SYS_CLKRST_REG_LCDCAM_CLK_EN);
            REG_WRITE(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG, final);
            if (REG_READ(HP_SYS_CLKRST_LCDCAM_LCDCAM_CTRL0_REG) !=
                    ((core & ~CAMERA_CORE_DIVIDER_MASK) | target)) {
                ret = ESP_FAIL;
            }
            if (restore && ret == ESP_OK) {
                camera->owns_core_divider = false;
            }
        }
    }
    return ret;
}

static esp_err_t camera_route_native_inputs(const wt_bsp_camera_dvp_info_t *dvp)
{
    static const struct {
        gpio_num_t pin;
        int function;
        uint32_t signal;
    } inputs[] = {
        {GPIO_NUM_46, FUNC_GPIO46_CAM_DATA0_IN_PAD, CAM_DATA0_IN_PAD_IN_IDX},
        {GPIO_NUM_47, FUNC_GPIO47_CAM_DATA1_IN_PAD, CAM_DATA1_IN_PAD_IN_IDX},
        {GPIO_NUM_48, FUNC_GPIO48_CAM_DATA2_IN_PAD, CAM_DATA2_IN_PAD_IN_IDX},
        {GPIO_NUM_49, FUNC_GPIO49_CAM_DATA3_IN_PAD, CAM_DATA3_IN_PAD_IN_IDX},
        {GPIO_NUM_50, FUNC_GPIO50_CAM_DATA4_IN_PAD, CAM_DATA4_IN_PAD_IN_IDX},
        {GPIO_NUM_51, FUNC_GPIO51_CAM_DATA5_IN_PAD, CAM_DATA5_IN_PAD_IN_IDX},
        {GPIO_NUM_52, FUNC_GPIO52_CAM_DATA6_IN_PAD, CAM_DATA6_IN_PAD_IN_IDX},
        {GPIO_NUM_53, FUNC_GPIO53_CAM_DATA7_IN_PAD, CAM_DATA7_IN_PAD_IN_IDX},
        {GPIO_NUM_54, FUNC_MTDO_CAM_PCLK_PAD, CAM_PCLK_PAD_IN_IDX},
    };

    /* Keep the driver's VSYNC inversion and DE matrix routing. GPIO57's
     * native alternate function is HSYNC, which is not used in DE mode. */
    for (size_t i = 0; i < 8; i++) {
        ESP_RETURN_ON_FALSE(dvp->data_gpio[i] == (int)inputs[i].pin,
                            ESP_ERR_NOT_SUPPORTED, TAG, "unsupported native DVP data pin");
    }
    ESP_RETURN_ON_FALSE(dvp->pclk_gpio == (int)inputs[8].pin,
                        ESP_ERR_NOT_SUPPORTED, TAG, "unsupported native DVP clock pin");
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        ESP_RETURN_ON_ERROR(gpio_iomux_input(inputs[i].pin, inputs[i].function, inputs[i].signal),
                            TAG, "route native DVP input");
    }
    return ESP_OK;
}

static esp_err_t camera_create_controller(gc2145_capture_t *camera,
                                          const wt_bsp_camera_dvp_info_t *dvp)
{
    /* XCLK remains live through sensor teardown as well as stream setup. */
#if CONFIG_PM_ENABLE
    ESP_RETURN_ON_ERROR(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "gc2145_xclk",
                                          &camera->no_sleep_lock), TAG, "create XCLK sleep lock");
    ESP_RETURN_ON_ERROR(esp_pm_lock_acquire(camera->no_sleep_lock), TAG, "acquire XCLK sleep lock");
    camera->no_sleep_lock_held = true;
#endif
    ESP_RETURN_ON_ERROR(camera_change_core_divider(camera, false), TAG, "configure LCD_CAM core clock");

    /* Generate 24 MHz as 120 MHz / 5.
     * Retain our APLL reference through DVP deletion to prevent retuning. */
    const uint32_t apll_hz = CAMERA_720P_APLL_HZ;
    ESP_RETURN_ON_ERROR(esp_clk_tree_enable_src(SOC_MOD_CLK_APLL, true), TAG, "acquire camera APLL");
    camera->owns_apll = true;
    uint32_t actual_hz = 0;
    esp_err_t ret = esp_clk_tree_src_set_freq_hz(SOC_MOD_CLK_APLL, apll_hz, &actual_hz);
    if ((ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) || actual_hz != apll_hz) {
        ESP_LOGE(TAG, "camera APLL unavailable: requested %u Hz, got %" PRIu32 " Hz (%s)",
                 apll_hz, actual_hz, esp_err_to_name(ret));
        return ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
    }

    esp_cam_ctlr_dvp_pin_config_t pins = {
        .data_width = CAM_CTLR_DATA_WIDTH_8,
        .vsync_io = dvp->vsync_gpio,
        .de_io = dvp->de_gpio,
        .pclk_io = dvp->pclk_gpio,
        .xclk_io = dvp->xclk_gpio,
    };
    for (size_t i = 0; i < 8; i++) {
        pins.data_io[i] = dvp->data_gpio[i];
    }
    const esp_cam_ctlr_dvp_config_t config = {
        .ctlr_id = 0,
        .clk_src = CAM_CLK_SRC_APLL,
        .h_res = camera->width,
        .v_res = camera->height,
        .input_data_color_type = CAM_CTLR_COLOR_YUV422_UYVY,
        .output_data_color_type = CAM_CTLR_COLOR_YUV422_UYVY,
        .cam_data_width = 8,
        .dma_burst_size = CAMERA_DMA_BURST_BYTES,
        .xclk_freq = dvp->xclk_freq_hz,
        .pin = &pins,
        .byte_swap_en = false,
        .bk_buffer_dis = false,
    };
    ESP_RETURN_ON_ERROR(esp_cam_new_dvp_ctlr(&config, &camera->controller), TAG, "create DVP controller");
    return camera_route_native_inputs(dvp);
}

static esp_err_t camera_init_sensor(gc2145_capture_t *camera,
                                    const wt_bsp_camera_dvp_info_t *dvp)
{
    const i2c_master_bus_config_t bus = {
        .i2c_port = dvp->sccb_port,
        .sda_io_num = dvp->sccb_sda_gpio,
        .scl_io_num = dvp->sccb_scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus, &camera->i2c_bus), TAG, "create SCCB bus");
    /* The controller has already enabled XCLK. */
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gc2145_sensor_init(&camera->sensor, camera->i2c_bus),
                        TAG, "initialize GC2145");
    gc2145_sensor_readback_t sensor;
    ESP_RETURN_ON_ERROR(gc2145_sensor_readback(&camera->sensor, &sensor), TAG, "read sensor configuration");
    ESP_RETURN_ON_FALSE(sensor.width == camera->width && sensor.height == camera->height,
                        ESP_ERR_INVALID_SIZE, TAG, "unexpected sensor dimensions");
    ESP_LOGI(TAG, "GC2145 PID=0x%04x, DVP %ux%u UYVY, XCLK=%" PRIu32 " Hz",
             (unsigned)sensor.pid, (unsigned)sensor.width, (unsigned)sensor.height, dvp->xclk_freq_hz);
    return ESP_OK;
}

static esp_err_t camera_init_buffers(gc2145_capture_t *camera, uint32_t count)
{
    camera->free_queue = xQueueCreateWithCaps(count, sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    camera->ready_queue = xQueueCreateWithCaps(count, sizeof(completed_frame_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (camera->free_queue == NULL || camera->ready_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    for (uint32_t i = 0; i < count; i++) {
        camera->buffers[i] = esp_cam_ctlr_alloc_buffer(camera->controller, camera->frame_bytes,
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (camera->buffers[i] == NULL) {
            return ESP_ERR_NO_MEM;
        }
        camera->buffer_count++;
        ESP_RETURN_ON_ERROR(camera_return_buffer(camera, i), TAG, "queue DVP buffer");
    }

    const esp_cam_ctlr_evt_cbs_t callbacks = {
        .on_get_new_trans = camera_get_buffer,
        .on_trans_finished = camera_frame_done,
    };
    return esp_cam_ctlr_register_event_callbacks(camera->controller, &callbacks, camera);
}

static esp_err_t camera_prepare(gc2145_capture_t *camera, const wt_bsp_camera_info_t *info)
{
    ESP_RETURN_ON_ERROR(camera_create_controller(camera, &info->dvp), TAG, "configure DVP and XCLK");
    ESP_RETURN_ON_ERROR(camera_init_sensor(camera, &info->dvp), TAG, "configure GC2145 sensor");
    return camera_init_buffers(camera, info->capture_buffer_count);
}

static esp_err_t camera_capture_start(gc2145_capture_t *camera)
{
    if (camera == NULL || camera->stopping || camera->controller == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (camera->running) {
        return ESP_OK;
    }
    if (camera->enabled) {
        /* A partial start needs stop() before the next attempt. */
        return ESP_ERR_INVALID_STATE;
    }
    /* W1C only the camera overflow flag. Other LCD_CAM interrupt flags are shared. */
    REG_WRITE(LCDCAM_LC_DMA_INT_CLR_REG, LCDCAM_CAM_OVERFLOW_INT_CLR);
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_enable(camera->controller), TAG, "enable DVP");
    camera->enabled = true;
    /* IDF changes its state before starting DMA. Cleanup must stop it on error too. */
    camera->streaming = true;
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_start(camera->controller), TAG, "start DVP");
    ESP_RETURN_ON_ERROR(gc2145_sensor_set_stream(&camera->sensor, true), TAG, "start GC2145 output");

    completed_frame_t first_frame;
    ESP_RETURN_ON_ERROR(camera_wait_buffer(camera, &first_frame), TAG, "prime DVP capture");
    ESP_RETURN_ON_ERROR(camera_return_buffer(camera, first_frame.index), TAG, "return priming frame");
    camera->running = true;
    return ESP_OK;
}

static esp_err_t camera_capture_prepare(gc2145_capture_t **capture, const wt_bsp_camera_info_t *info)
{
    if (capture == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (info->format.width != GC2145_720P_WIDTH || info->format.height != GC2145_720P_HEIGHT ||
            info->dvp.xclk_freq_hz != GC2145_720P_XCLK_HZ || info->dvp.sccb_freq_hz != 100000 ||
            info->dvp.reset_gpio >= 0 || info->dvp.pwdn_gpio >= 0 ||
            info->capture_buffer_count != CAMERA_BUFFER_COUNT ||
            info->dequeue_timeout_ms <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*capture != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    gc2145_capture_t *camera = heap_caps_calloc(1, sizeof(*camera),
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (camera == NULL) {
        return ESP_ERR_NO_MEM;
    }
    camera->width = info->format.width;
    camera->height = info->format.height;
    camera->frame_bytes = (size_t)camera->width * camera->height * 2;
    camera->wait_ticks = pdMS_TO_TICKS(info->dequeue_timeout_ms);
    if (camera->wait_ticks == 0) {
        camera->wait_ticks = 1;
    }
    camera->startup_discard = info->startup_frames_to_discard;
    *capture = camera;

    esp_err_t ret = camera_prepare(camera, info);
    if (ret != ESP_OK) {
        esp_err_t cleanup = camera_capture_stop(capture);
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "failed to clean up DVP after %s: %s", esp_err_to_name(ret), esp_err_to_name(cleanup));
        }
    }
    return ret;
}

static esp_err_t camera_capture_stop(gc2145_capture_t **capture)
{
    if (capture == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    gc2145_capture_t *camera = *capture;
    if (camera == NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(!camera->frame_held, ESP_ERR_INVALID_STATE, TAG, "DVP frame is still borrowed");

    camera->stopping = true;
    /* Keep XCLK running until the sensor has stopped output and released SCCB. */
    ESP_RETURN_ON_ERROR(gc2145_sensor_deinit(&camera->sensor), TAG, "stop/remove sensor");
    if (camera->streaming) {
        ESP_RETURN_ON_ERROR(esp_cam_ctlr_stop(camera->controller), TAG, "stop DVP");
        camera->streaming = false;
    }
    if (camera->enabled) {
        ESP_RETURN_ON_ERROR(esp_cam_ctlr_disable(camera->controller), TAG, "disable DVP");
        camera->enabled = false;
    }
    if (camera->controller != NULL) {
        /* Controller deletion synchronizes the DMA ISR before queues/buffers are freed. */
        ESP_RETURN_ON_ERROR(esp_cam_ctlr_del(camera->controller), TAG, "delete DVP");
        camera->controller = NULL;
    }
    ESP_RETURN_ON_ERROR(camera_change_core_divider(camera, true), TAG, "restore LCD_CAM core clock");
    if (camera->owns_apll) {
        ESP_RETURN_ON_ERROR(esp_clk_tree_enable_src(SOC_MOD_CLK_APLL, false), TAG, "release camera APLL");
        camera->owns_apll = false;
    }
#if CONFIG_PM_ENABLE
    if (camera->no_sleep_lock_held) {
        ESP_RETURN_ON_ERROR(esp_pm_lock_release(camera->no_sleep_lock), TAG, "release XCLK sleep lock");
        camera->no_sleep_lock_held = false;
    }
    if (camera->no_sleep_lock != NULL) {
        ESP_RETURN_ON_ERROR(esp_pm_lock_delete(camera->no_sleep_lock), TAG, "delete XCLK sleep lock");
        camera->no_sleep_lock = NULL;
    }
#endif
    if (camera->i2c_bus != NULL) {
        ESP_RETURN_ON_ERROR(i2c_del_master_bus(camera->i2c_bus), TAG, "delete SCCB bus");
        camera->i2c_bus = NULL;
    }

    for (uint32_t i = 0; i < camera->buffer_count; i++) {
        heap_caps_free(camera->buffers[i]);
    }
    if (camera->free_queue != NULL) {
        vQueueDeleteWithCaps(camera->free_queue);
    }
    if (camera->ready_queue != NULL) {
        vQueueDeleteWithCaps(camera->ready_queue);
    }
    heap_caps_free(camera);
    *capture = NULL;
    return ESP_OK;
}

static esp_err_t camera_capture_acquire(gc2145_capture_t *capture, gc2145_frame_t *frame)
{
    if (capture == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!capture->running || capture->stopping || capture->frame_held) {
        return ESP_ERR_INVALID_STATE;
    }

    completed_frame_t completed;
    ESP_RETURN_ON_ERROR(camera_wait_buffer(capture, &completed), TAG, "wait for frame");
    uint8_t *buffer = capture->buffers[completed.index];
    esp_err_t ret = esp_cache_msync(buffer, capture->frame_bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    if (ret != ESP_OK) {
        ESP_RETURN_ON_ERROR(camera_return_buffer(capture, completed.index), TAG, "return failed frame");
        return ret;
    }

    capture->held_index = completed.index;
    capture->frame_held = true;
    *frame = (gc2145_frame_t) {
        .data = buffer,
        .size = capture->frame_bytes,
        .timestamp_us = completed.timestamp_us,
        .index = completed.index,
    };
    return ESP_OK;
}

static void camera_capture_release(gc2145_capture_t *capture, const gc2145_frame_t *frame)
{
    if (capture == NULL || frame == NULL || !capture->frame_held ||
            capture->held_index != frame->index) {
        return;
    }
    const esp_err_t ret = camera_return_buffer(capture, frame->index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "could not return DVP buffer: %s", esp_err_to_name(ret));
    }
    capture->frame_held = false;
}

/* Camera interface and hardware JPEG encoding */
typedef struct {
    SemaphoreHandle_t mutex;
    gc2145_capture_t *capture;
    jpeg_encoder_handle_t encoder;
    uint8_t *jpeg;
    size_t jpeg_capacity;
    bool prepared;
    bool streaming;
    bool frame_borrowed;
    wt_bsp_camera_status_t health;
} gc2145_720p_context_t;

static esp_err_t gc2145_720p_prepare_locked(wt_bsp_camera_t camera)
{
    gc2145_720p_context_t *ctx = camera->backend;
    const wt_bsp_camera_info_t *info = &camera->info;
    if (ctx->jpeg == NULL) {
        const jpeg_encode_memory_alloc_cfg_t memory = {
            .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER,
        };
        ctx->jpeg = jpeg_alloc_encoder_mem(info->jpeg_buffer_capacity, &memory, &ctx->jpeg_capacity);
        ESP_RETURN_ON_FALSE(ctx->jpeg, ESP_ERR_NO_MEM, TAG, "allocate JPEG buffer");
    }
    if (ctx->encoder == NULL) {
        const jpeg_encode_engine_cfg_t config = {
            .timeout_ms = info->jpeg_encode_timeout_ms,
        };
        ESP_RETURN_ON_ERROR(jpeg_new_encoder_engine(&config, &ctx->encoder), TAG, "create JPEG engine");
    }
    if (!ctx->prepared) {
        ESP_RETURN_ON_ERROR(camera_capture_stop(&ctx->capture), TAG, "finish DVP cleanup");
        ESP_RETURN_ON_ERROR(camera_capture_prepare(&ctx->capture, info), TAG, "prepare DVP capture");
        ctx->prepared = true;
    }
    return ESP_OK;
}

static esp_err_t gc2145_720p_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format)
{
    gc2145_720p_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    esp_err_t ret = ctx->frame_borrowed ? ESP_ERR_INVALID_STATE : gc2145_720p_prepare_locked(camera);
    if (ret == ESP_OK) {
        *format = camera->info.format;
    }
    if (!ctx->frame_borrowed) {
        ctx->health.ready = ret == ESP_OK;
        ctx->health.last_error = ret;
    }
    xSemaphoreGiveRecursive(ctx->mutex);
    return ret;
}

static esp_err_t gc2145_720p_encode(const wt_bsp_camera_info_t *info, gc2145_720p_context_t *ctx,
                                      const gc2145_frame_t *raw, uint32_t *jpeg_size)
{
    esp_err_t ret = esp_cache_msync(ctx->jpeg, ctx->jpeg_capacity,
                                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    if (ret == ESP_OK) {
        const jpeg_encode_cfg_t config = {
            .height = info->format.height,
            .width = info->format.width,
            .src_type = JPEG_ENCODE_IN_FORMAT_YUV422,
            .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
            .image_quality = info->jpeg_quality,
        };
        ret = jpeg_encoder_process(ctx->encoder, &config, raw->data, raw->size,
                                   ctx->jpeg, ctx->jpeg_capacity, jpeg_size);
    }
    /* JPEG DMA borrows the captured UYVY buffer until encoding returns. */
    camera_capture_release(ctx->capture, raw);
    ESP_RETURN_ON_ERROR(ret, TAG, "encode JPEG");
    if (*jpeg_size < 4 || *jpeg_size > ctx->jpeg_capacity ||
            ctx->jpeg[0] != 0xff || ctx->jpeg[1] != 0xd8 ||
            ctx->jpeg[*jpeg_size - 2] != 0xff || ctx->jpeg[*jpeg_size - 1] != 0xd9) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t gc2145_720p_capture(wt_bsp_camera_t camera, wt_bsp_camera_frame_cb_t callback,
                                   void *user_data)
{
    gc2145_720p_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    if (ctx->frame_borrowed) {
        xSemaphoreGiveRecursive(ctx->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = gc2145_720p_prepare_locked(camera);
    if (ret == ESP_OK && !ctx->streaming) {
        ret = camera_capture_start(ctx->capture);
        ctx->streaming = ret == ESP_OK;
    }
    gc2145_frame_t raw;
    if (ret == ESP_OK) {
        ret = camera_capture_acquire(ctx->capture, &raw);
    }
    if (ret != ESP_OK) {
        /* Rebuild the DMA state after a lost capture on the next request. */
        const esp_err_t cleanup = camera_capture_stop(&ctx->capture);
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "Capture cleanup failed: %s", esp_err_to_name(cleanup));
        }
        ctx->prepared = false;
        ctx->streaming = false;
        ctx->health.ready = false;
        ctx->health.last_error = ret;
        xSemaphoreGiveRecursive(ctx->mutex);
        return ret;
    }

    uint32_t jpeg_size = 0;
    ret = gc2145_720p_encode(&camera->info, ctx, &raw, &jpeg_size);
    ctx->health.ready = true;
    ctx->health.last_error = ret;
    if (ret == ESP_OK) {
        const wt_bsp_camera_frame_t frame = {
            .data = ctx->jpeg,
            .length = jpeg_size,
            .width = camera->info.format.width,
            .height = camera->info.format.height,
            .timestamp_us = raw.timestamp_us,
        };
        ctx->health.frames++;
        ctx->health.last_frame_us = raw.timestamp_us;
        ctx->frame_borrowed = true;
        ret = callback(&frame, user_data);
        ctx->frame_borrowed = false;
    }
    xSemaphoreGiveRecursive(ctx->mutex);
    return ret;
}

static esp_err_t gc2145_720p_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status)
{
    gc2145_720p_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    *status = ctx->health;
    xSemaphoreGiveRecursive(ctx->mutex);
    return ESP_OK;
}

static esp_err_t gc2145_720p_deinit(wt_bsp_camera_t camera)
{
    gc2145_720p_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    if (ctx->frame_borrowed) {
        xSemaphoreGiveRecursive(ctx->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = camera_capture_stop(&ctx->capture);
    ctx->health.ready = false;
    ctx->prepared = false;
    ctx->streaming = false;
    if (ret == ESP_OK && ctx->encoder != NULL) {
        ret = jpeg_del_encoder_engine(ctx->encoder);
        if (ret == ESP_OK) {
            ctx->encoder = NULL;
        }
    }
    ctx->health.last_error = ret;
    xSemaphoreGiveRecursive(ctx->mutex);
    if (ret == ESP_OK) {
        free(ctx->jpeg);
        vSemaphoreDelete(ctx->mutex);
        free(ctx);
        camera->backend = NULL;
    }
    return ret;
}

static esp_err_t gc2145_720p_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info,
                                    const wt_bsp_camera_ops_t **ops)
{
    static const wt_bsp_camera_ops_t driver_ops = {
        .deinit = gc2145_720p_deinit,
        .prepare = gc2145_720p_prepare,
        .capture = gc2145_720p_capture,
        .get_status = gc2145_720p_get_status,
    };

    if (info->format.width != GC2145_720P_WIDTH || info->format.height != GC2145_720P_HEIGHT ||
            info->format.fps != 30 || info->dvp.xclk_freq_hz != GC2145_720P_XCLK_HZ ||
            info->capture_buffer_count != CAMERA_BUFFER_COUNT ||
            info->property_count != 0 || info->dequeue_timeout_ms <= 0 ||
            info->jpeg_buffer_capacity == 0 || info->jpeg_buffer_capacity > UINT32_MAX ||
            info->jpeg_quality < 1 || info->jpeg_quality > 100 || info->jpeg_encode_timeout_ms <= 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    gc2145_720p_context_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->mutex = xSemaphoreCreateRecursiveMutex();
    if (ctx->mutex == NULL) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }
    ctx->health.last_error = ESP_ERR_INVALID_STATE;
    camera->backend = ctx;
    *ops = &driver_ops;
    return ESP_OK;
}

WT_BSP_CAMERA_DRIVER_REGISTER(gc2145_720p, "gc2145_720p", gc2145_720p_init);

#endif
