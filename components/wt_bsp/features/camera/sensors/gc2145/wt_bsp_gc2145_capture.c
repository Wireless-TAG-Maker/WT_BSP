/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wt_bsp_camera_port.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_IDF_TARGET_ESP32S31

#include "wt_bsp_gc2145_capture.h"

#include <inttypes.h>

#include "driver/gpio.h"
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
#include "freertos/task.h"
#include "gc2145_sensor.h"
#include "soc/gpio_sig_map.h"
#include "soc/hp_sys_clkrst_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/lcd_cam_reg.h"
#include "soc/soc.h"

#define CAMERA_MAX_BUFFERS 3
#define CAMERA_VGA_APLL_HZ 100000000U
#define CAMERA_720P_APLL_HZ 120000000U
#define CAMERA_DMA_BURST_BYTES 64
#define CAMERA_CORE_DIVIDER_MASK (HP_SYS_CLKRST_REG_LCDCAM_CLK_DIV_NUM_M | \
                                 HP_SYS_CLKRST_REG_LCDCAM_CLK_DIV_NUMERATOR_M | \
                                 HP_SYS_CLKRST_REG_LCDCAM_CLK_DIV_DENOMINATOR_M)

typedef struct {
    uint32_t index;
    int64_t timestamp_us;
} completed_frame_t;

struct wt_bsp_gc2145_capture {
    esp_cam_ctlr_handle_t controller;
    i2c_master_bus_handle_t i2c_bus;
    gc2145_sensor_t sensor;
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
    uint8_t *buffers[CAMERA_MAX_BUFFERS];
    uint32_t buffer_count;
    uint32_t width;
    uint32_t height;
    size_t frame_bytes;
    gc2145_sensor_mode_t mode;
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
};

static const char *TAG = "gc2145_dvp";

/* Full-field VGA timing from the WT9932S31-TINY capture validation. */
static const gc2145_sensor_timing_t s_vga_timing = {
    .pll_mode2 = 0x86,
    .hblank = 304,
    .vblank = 12,
    .anti_flicker_step = 376,
    .exposure_levels = {376, 752, 1128, 1128},
};

static bool IRAM_ATTR camera_get_buffer(esp_cam_ctlr_handle_t handle,
                                        esp_cam_ctlr_trans_t *trans, void *user_data)
{
    (void)handle;
    wt_bsp_gc2145_capture_t *camera = user_data;
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
    wt_bsp_gc2145_capture_t *camera = user_data;
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

static esp_err_t camera_return_buffer(wt_bsp_gc2145_capture_t *camera, uint32_t index)
{
    if (index >= camera->buffer_count || xQueueSend(camera->free_queue, &index, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t camera_wait_buffer(wt_bsp_gc2145_capture_t *camera, completed_frame_t *frame)
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

static esp_err_t camera_change_core_divider(wt_bsp_gc2145_capture_t *camera, bool restore)
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

static esp_err_t camera_create_controller(wt_bsp_gc2145_capture_t *camera,
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

    /* Generate 24 MHz as 120 MHz / 5, or 25 MHz as 100 MHz / 4.
     * Retain our APLL reference through DVP deletion to prevent retuning. */
    const uint32_t apll_hz = camera->mode == GC2145_SENSOR_MODE_720P ?
                            CAMERA_720P_APLL_HZ : CAMERA_VGA_APLL_HZ;
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
    const cam_ctlr_color_t color = camera->mode == GC2145_SENSOR_MODE_720P ?
                                  CAM_CTLR_COLOR_YUV422_UYVY : CAM_CTLR_COLOR_RGB565;
    const esp_cam_ctlr_dvp_config_t config = {
        .ctlr_id = 0,
        .clk_src = CAM_CLK_SRC_APLL,
        .h_res = camera->width,
        .v_res = camera->height,
        .input_data_color_type = color,
        .output_data_color_type = color,
        .cam_data_width = 8,
        .dma_burst_size = CAMERA_DMA_BURST_BYTES,
        .xclk_freq = dvp->xclk_freq_hz,
        .pin = &pins,
        .byte_swap_en = false, /* S31 8-bit DVP cannot swap the sensor's RGB565-BE bytes. */
        .bk_buffer_dis = false,
    };
    ESP_RETURN_ON_ERROR(esp_cam_new_dvp_ctlr(&config, &camera->controller), TAG, "create DVP controller");
    return camera_route_native_inputs(dvp);
}

static esp_err_t camera_init_sensor(wt_bsp_gc2145_capture_t *camera,
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
    ESP_RETURN_ON_ERROR(gc2145_sensor_init(&camera->sensor, camera->i2c_bus, camera->mode),
                        TAG, "initialize GC2145");
    if (camera->mode == GC2145_SENSOR_MODE_STOCK_VGA) {
        ESP_RETURN_ON_ERROR(gc2145_sensor_set_timing(&camera->sensor, &s_vga_timing), TAG, "apply VGA timing");
    }
    gc2145_sensor_readback_t sensor;
    ESP_RETURN_ON_ERROR(gc2145_sensor_readback(&camera->sensor, &sensor), TAG, "read sensor configuration");
    ESP_RETURN_ON_FALSE(sensor.width == camera->width && sensor.height == camera->height,
                        ESP_ERR_INVALID_SIZE, TAG, "unexpected sensor dimensions");
    ESP_LOGI(TAG, "GC2145 PID=0x%04x, DVP %ux%u %s, XCLK=%" PRIu32 " Hz",
             (unsigned)sensor.pid, (unsigned)sensor.width, (unsigned)sensor.height,
             camera->mode == GC2145_SENSOR_MODE_720P ? "UYVY" : "RGB565-BE", dvp->xclk_freq_hz);
    return ESP_OK;
}

static esp_err_t camera_init_buffers(wt_bsp_gc2145_capture_t *camera, uint32_t count)
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

static esp_err_t camera_prepare(wt_bsp_gc2145_capture_t *camera, const wt_bsp_camera_info_t *info)
{
    ESP_RETURN_ON_ERROR(camera_create_controller(camera, &info->dvp), TAG, "configure DVP and XCLK");
    ESP_RETURN_ON_ERROR(camera_init_sensor(camera, &info->dvp), TAG, "configure GC2145 sensor");
    return camera_init_buffers(camera, info->capture_buffer_count);
}

esp_err_t wt_bsp_gc2145_capture_start(wt_bsp_gc2145_capture_t *camera)
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

esp_err_t wt_bsp_gc2145_capture_prepare(wt_bsp_gc2145_capture_t **capture, const wt_bsp_camera_info_t *info)
{
    if (capture == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool vga = info->format.width == WT_BSP_GC2145_VGA_WIDTH &&
                     info->format.height == WT_BSP_GC2145_VGA_HEIGHT &&
                     info->dvp.xclk_freq_hz == WT_BSP_GC2145_VGA_XCLK_HZ;
    const bool hd = info->format.width == WT_BSP_GC2145_720P_WIDTH &&
                    info->format.height == WT_BSP_GC2145_720P_HEIGHT &&
                    info->dvp.xclk_freq_hz == WT_BSP_GC2145_720P_XCLK_HZ;
    if ((!vga && !hd) || info->dvp.sccb_freq_hz != 100000 || info->dvp.reset_gpio >= 0 || info->dvp.pwdn_gpio >= 0 ||
            info->capture_buffer_count < 2 || info->capture_buffer_count > CAMERA_MAX_BUFFERS ||
            info->dequeue_timeout_ms <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*capture != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    wt_bsp_gc2145_capture_t *camera = heap_caps_calloc(1, sizeof(*camera),
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (camera == NULL) {
        return ESP_ERR_NO_MEM;
    }
    camera->mode = hd ? GC2145_SENSOR_MODE_720P : GC2145_SENSOR_MODE_STOCK_VGA;
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
        esp_err_t cleanup = wt_bsp_gc2145_capture_stop(capture);
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "failed to clean up DVP after %s: %s", esp_err_to_name(ret), esp_err_to_name(cleanup));
        }
    }
    return ret;
}

esp_err_t wt_bsp_gc2145_capture_stop(wt_bsp_gc2145_capture_t **capture)
{
    if (capture == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    wt_bsp_gc2145_capture_t *camera = *capture;
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

esp_err_t wt_bsp_gc2145_capture_acquire(wt_bsp_gc2145_capture_t *capture, wt_bsp_gc2145_frame_t *frame)
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
    *frame = (wt_bsp_gc2145_frame_t) {
        .data = buffer,
        .size = capture->frame_bytes,
        .timestamp_us = completed.timestamp_us,
        .index = completed.index,
    };
    return ESP_OK;
}

void wt_bsp_gc2145_capture_release(wt_bsp_gc2145_capture_t *capture, const wt_bsp_gc2145_frame_t *frame)
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

#endif
