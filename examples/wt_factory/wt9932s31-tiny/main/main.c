/**
 * @file main.c
 * @author Wireless-Tag
 * @brief S31 factory diagnostics with optional recording and USB UVC/CDC.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"
#include "factory_console.h"

#include "sdkconfig.h"
#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
#include "avi_writer.h"
#endif

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ==================== [Defines] =========================================== */

#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
#define FACTORY_SEGMENT_BYTES (64U * 1024U * 1024U)
#define FACTORY_SEGMENT_US ((int64_t)CONFIG_WT_FACTORY_SEGMENT_SECONDS * 1000000)
#endif
#define FACTORY_LED_LEVEL 24

/* ==================== [Typedefs] ========================================== */

typedef struct {
    wt_bsp_camera_t camera;
    wt_bsp_camera_format_t format;
    wt_bsp_usb_device_cdc_t cdc;
    wt_bsp_sdmmc_t sdmmc;
    wt_bsp_rgb_t rgb;
    QueueHandle_t status_queue;
    factory_status_t status;
    char directory[FACTORY_PATH_SIZE];
#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
    avi_writer_t writer;
    char partial[FACTORY_PATH_SIZE + 32];
    char completed[FACTORY_PATH_SIZE + 32];
    uint32_t next_file;
#endif
    bool controls_ok;
    bool mounted;
    int led;
} factory_t;

/* ==================== [Static Prototypes] ================================= */

static void factory_button_cb(wt_bsp_button_t button, wt_bsp_button_event_t event, void *user_data);
static bool factory_prepare_storage(factory_t *factory);
static bool factory_close_segment(factory_t *factory);
static esp_err_t factory_consume_frame(const wt_bsp_camera_frame_t *frame, void *user_data);
static void factory_publish_status(factory_t *factory);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "s31_factory";
static factory_t s_factory;

/* ==================== [Macros] ============================================ */
/* ==================== [Global Functions] ================================== */

void app_main(void)
{
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    factory_t *factory = &s_factory;
    factory->rgb = wt_bsp_get_rgb();
    factory->camera = wt_bsp_get_camera();
    factory->sdmmc = wt_bsp_get_sdmmc();
    factory->cdc = wt_bsp_get_usb_device_cdc();
    wt_bsp_button_t button = wt_bsp_get_button();
    factory->led = -1;
#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
    factory->status.recording_enabled = true;
#endif
    factory->status_queue = xQueueCreate(1, sizeof(factory_status_t));
    if (factory->rgb != NULL) {
        wt_bsp_rgb_set_color(factory->rgb, (wt_bsp_rgb_color_t) {.r = FACTORY_LED_LEVEL});
    }
    if (factory->rgb == NULL || factory->camera == NULL || factory->sdmmc == NULL ||
            factory->cdc == NULL || button == NULL || factory->status_queue == NULL) {
        ESP_LOGE(TAG, "Required factory feature or status queue is unavailable");
        return;
    }
    factory->controls_ok = wt_bsp_button_register_event_cb(button, factory_button_cb,
                                                          xTaskGetCurrentTaskHandle()) == ESP_OK;
    factory->status.sd_ok = factory_prepare_storage(factory);
    ret = factory_console_start(factory->cdc, factory->directory, factory->status_queue);
    if (ret != ESP_OK) {
        factory->controls_ok = false;
        ESP_LOGE(TAG, "CDC console failed: %s", esp_err_to_name(ret));
    }
    ret = wt_bsp_camera_prepare(factory->camera, &factory->format);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera preparation failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "J1 enumeration selects UVC+CDC; SD recording %s",
             factory->status.recording_enabled ? "enabled; SW2 toggles recording" : "disabled in menuconfig");
    ESP_LOGI(TAG, "LED: green=healthy, blue=paused, red=error; directory=%s", factory->directory);

    int64_t next_probe = 0;
    int64_t next_card_check = 0;
    int64_t next_log = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        bool usb = wt_bsp_usb_device_cdc_is_mounted(factory->cdc);
        if (usb != factory->status.usb) {
            /* Publish USB mode only after the AVI is closed, so CDC readback
             * never accesses a file while the recorder is still writing it. */
            factory_close_segment(factory);
            factory->status.usb = usb;
            ESP_LOGI(TAG, "Mode -> %s", usb ? "USB UVC+CDC" :
                     (factory->status.recording_enabled ? "SD recorder" : "hardware monitor"));
        }
        uint32_t presses = ulTaskNotifyTake(pdTRUE, 0);
        if (presses != 0) {
            if (!factory->status.recording_enabled) {
                ESP_LOGI(TAG, "SW2 pressed (%" PRIu32 " press events); SD recording disabled", presses);
            } else if (!usb) {
                if (presses & 1U) {
                    factory->status.paused = !factory->status.paused;
                }
                if (factory->status.paused) {
                    factory_close_segment(factory);
                }
                ESP_LOGI(TAG, "Recording %s (%" PRIu32 " press events)",
                         factory->status.paused ? "paused" : "resumed", presses);
            } else {
                ESP_LOGI(TAG, "SW2 ignored in USB mode; offline pause state preserved");
            }
        }

        int64_t now = esp_timer_get_time();
        if (factory->format.fps == 0 && now >= next_probe) {
            wt_bsp_camera_prepare(factory->camera, &factory->format);
            next_probe = now + 5000000;
        }
        if (!usb && factory->format.fps != 0) {
            /* Disabled or paused recording still checks camera frames. The
             * same capture engine is handed to UVC as soon as J1 enumerates. */
            ret = wt_bsp_camera_capture(factory->camera, factory_consume_frame, factory);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE && now >= next_log) {
                ESP_LOGW(TAG, "Local capture failed: %s", esp_err_to_name(ret));
            }
        }
        if (factory->mounted && factory->status.sd_ok && now >= next_card_check) {
            sdmmc_card_t *card = wt_bsp_sdmmc_get_card(factory->sdmmc);
            ret = card != NULL ? sdmmc_get_status(card) : ESP_ERR_INVALID_STATE;
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "SD status failed: %s; restart after checking the card", esp_err_to_name(ret));
                factory->status.sd_ok = false;
                factory_close_segment(factory);
            }
            next_card_check = now + 2000000;
        }
        factory_publish_status(factory);
        if (now >= next_log) {
            ESP_LOGI(TAG, "mode=%s recording_enabled=%d paused=%d camera=%d sd=%d recorded=%" PRIu64
                     " segments=%" PRIu32 " heap=%" PRIu32 "/%" PRIu32,
                     factory->status.usb ? "usb" : (factory->status.recording_enabled ? "sd" : "monitor"),
                     factory->status.recording_enabled, factory->status.paused,
                     factory->status.camera_ok, factory->status.sd_ok, factory->status.recorded,
                     factory->status.segments, factory->status.free_internal, factory->status.free_psram);
            next_log = now + 5000000;
        }
        uint32_t delay_ms = !usb && factory->format.fps != 0 ? 1000 / factory->format.fps : 20;
        if (xTaskDelayUntil(&wake, pdMS_TO_TICKS(delay_ms)) == pdFALSE) {
            /* Slow capture/storage must not starve the idle task. */
            vTaskDelay(1);
            wake = xTaskGetTickCount();
        }
    }
}

/* ==================== [Static Functions] ================================== */

static void factory_button_cb(wt_bsp_button_t button, wt_bsp_button_event_t event, void *user_data)
{
    (void)button;
    if (event == WT_BSP_BUTTON_EVENT_PRESS) {
        xTaskNotifyGive((TaskHandle_t)user_data);
    }
}

static bool factory_prepare_storage(factory_t *factory)
{
    const char *mount = wt_bsp_sdmmc_get_mount_point(factory->sdmmc);
    if (snprintf(factory->directory, sizeof(factory->directory), "%s/recordings", mount) >=
            sizeof(factory->directory)) {
        ESP_LOGE(TAG, "Recording directory path is too long");
        return false;
    }
    esp_err_t ret = wt_bsp_sdmmc_mount(factory->sdmmc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s; no formatting is performed", esp_err_to_name(ret));
        return false;
    }
    factory->mounted = true;
#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
    if (mkdir(factory->directory, 0777) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Cannot create recording directory: errno=%d", errno);
        return false;
    }
    DIR *directory = opendir(factory->directory);
    if (directory == NULL) {
        ESP_LOGE(TAG, "Cannot scan recording directory: errno=%d", errno);
        return false;
    }
    factory->next_file = 1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        unsigned number;
        int end = 0;
        if (sscanf(entry->d_name, "rec_%6u.avi%n", &number, &end) == 1 && end == 14 &&
                (entry->d_name[end] == '\0' || strcmp(entry->d_name + end, ".part") == 0) &&
                number >= factory->next_file) {
            factory->next_file = number + 1;
        }
    }
    closedir(directory);
    ESP_LOGI(TAG, "SD ready; next recording=%06" PRIu32, factory->next_file);
#else
    ESP_LOGI(TAG, "SD ready; recording disabled, no recording directory or files will be created");
#endif
    return true;
}

static bool factory_close_segment(factory_t *factory)
{
#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
    if (factory->writer.file == NULL) {
        return true;
    }
    uint32_t frames = factory->writer.frames;
    uint32_t bytes = factory->writer.data_bytes;
    if (avi_writer_close(&factory->writer) != 0) {
        ESP_LOGE(TAG, "AVI finalization failed: errno=%d; retained %s", errno, factory->partial);
        factory->status.sd_ok = false;
        return false;
    }
    /* FAT rename rejects an existing destination. Check explicitly for hosts
     * that provide POSIX replace semantics, and never reuse a scanned number. */
    struct stat existing;
    if (stat(factory->completed, &existing) == 0 || errno != ENOENT ||
            rename(factory->partial, factory->completed) != 0) {
        ESP_LOGE(TAG, "AVI rename failed: errno=%d; retained %s", errno, factory->partial);
        factory->status.sd_ok = false;
        return false;
    }
    factory->status.segments++;
    ESP_LOGI(TAG, "Saved %s frames=%" PRIu32 " bytes=%" PRIu32, factory->completed, frames, bytes);
#else
    (void)factory;
#endif
    return true;
}

static esp_err_t factory_consume_frame(const wt_bsp_camera_frame_t *frame, void *user_data)
{
#if CONFIG_WT_FACTORY_ENABLE_SD_RECORDING
    factory_t *factory = user_data;
    if (factory->status.paused || !factory->status.sd_ok ||
            wt_bsp_usb_device_cdc_is_mounted(factory->cdc)) {
        return ESP_OK;
    }
    if (factory->writer.file != NULL &&
            (frame->timestamp_us - factory->writer.first_timestamp_us >= FACTORY_SEGMENT_US ||
             factory->writer.data_bytes + frame->length + 32 > FACTORY_SEGMENT_BYTES ||
             factory->writer.frames >= factory->writer.capacity)) {
        if (!factory_close_segment(factory)) {
            return ESP_FAIL;
        }
    }
    if (factory->writer.file == NULL) {
        if (factory->next_file > 999999) {
            ESP_LOGE(TAG, "Recording file numbers exhausted; existing files are preserved");
            factory->status.sd_ok = false;
            return ESP_FAIL;
        }
        snprintf(factory->status.file, sizeof(factory->status.file), "rec_%06" PRIu32 ".avi",
                 factory->next_file++);
        snprintf(factory->completed, sizeof(factory->completed), "%s/%s", factory->directory,
                 factory->status.file);
        snprintf(factory->partial, sizeof(factory->partial), "%s/%.14s.part", factory->directory,
                 factory->status.file);
        uint32_t capacity = (CONFIG_WT_FACTORY_SEGMENT_SECONDS + 2) * factory->format.fps;
        if (avi_writer_open(&factory->writer, factory->partial, frame->width, frame->height,
                            factory->format.fps, capacity) != 0) {
            ESP_LOGE(TAG, "AVI creation failed: errno=%d", errno);
            factory->status.sd_ok = false;
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Recording %s", factory->partial);
    }
    if (avi_writer_append(&factory->writer, frame->data, frame->length, frame->timestamp_us) != 0) {
        ESP_LOGE(TAG, "AVI write failed: errno=%d; retained %s", errno, factory->partial);
        avi_writer_abort(&factory->writer);
        factory->status.sd_ok = false;
        return ESP_FAIL;
    }
    factory->status.recorded++;
#else
    (void)frame;
    (void)user_data;
#endif
    return ESP_OK;
}

static void factory_publish_status(factory_t *factory)
{
    wt_bsp_camera_status_t camera = {0};
    esp_err_t ret = wt_bsp_camera_get_status(factory->camera, &camera);
    factory->status.camera_ok = ret == ESP_OK && camera.ready && camera.last_error == ESP_OK;
    factory->status.free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    factory->status.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int led = !factory->controls_ok || !factory->status.sd_ok || !factory->status.camera_ok ? 0 :
              (!factory->status.usb && factory->status.paused ? 2 : 1);
    if (led != factory->led) {
        wt_bsp_rgb_color_t color = {
            .r = led == 0 ? FACTORY_LED_LEVEL : 0,
            .g = led == 1 ? FACTORY_LED_LEVEL : 0,
            .b = led == 2 ? FACTORY_LED_LEVEL : 0,
        };
        ret = wt_bsp_rgb_set_color(factory->rgb, color);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "RGB update failed: %s", esp_err_to_name(ret));
        } else {
            factory->led = led;
            ESP_LOGI(TAG, "LED -> %s", led == 0 ? "red" : led == 1 ? "green" : "blue");
        }
    }
    xQueueOverwrite(factory->status_queue, &factory->status);
}
