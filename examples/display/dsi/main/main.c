/**
 * @file main.c
 * @author Wireless-Tag
 * @brief BSP DSI display and LVGL demo example.
 * @version 0.1
 * @date 2026-11-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"

#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if WT_BSP_DSI_ENABLED
#include "lv_demos.h"
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

#if WT_BSP_DSI_ENABLED
#if CONFIG_EXAMPLE_DSI_DEMO_BENCHMARK && LV_VERSION_CHECK(9, 5, 0)
static void benchmark_finished(const lv_demo_benchmark_summary_t *summary);
#endif
#endif

/* ==================== [Static Variables] ================================== */

static const char *TAG = "dsi_example";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing BSP");
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
        return;
    }

#if WT_BSP_DSI_ENABLED
    wt_bsp_dsi_t dsi = wt_bsp_get_dsi();
    if (dsi == NULL) {
        ESP_LOGE(TAG, "Failed to get DSI handle");
        return;
    }

    ESP_LOGI(TAG, "Starting LVGL");
    lv_display_t *display = wt_bsp_dsi_lvgl_start(dsi, NULL);
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start LVGL");
        return;
    }
    if (!wt_bsp_dsi_lvgl_lock(0)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

#if WT_BSP_TOUCH_ENABLED
    wt_bsp_touch_t touch = wt_bsp_get_touch();
    if (touch != NULL && wt_bsp_touch_lvgl_start(touch, display) == NULL) {
        ESP_LOGE(TAG, "Failed to register touch with LVGL");
        wt_bsp_dsi_lvgl_unlock();
        return;
    }
#endif

#if CONFIG_EXAMPLE_DSI_DEMO_BENCHMARK
#if LV_VERSION_CHECK(9, 5, 0)
    lv_demo_benchmark_set_end_cb(benchmark_finished);
#endif
    lv_demo_benchmark();
#else
    lv_demo_widgets();
#endif
    wt_bsp_dsi_lvgl_unlock();

    ret = wt_bsp_dsi_display_on(dsi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display: %s", esp_err_to_name(ret));
        return;
    }
    ret = wt_bsp_dsi_set_brightness(dsi, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
        return;
    }

#if CONFIG_EXAMPLE_DSI_DEMO_BENCHMARK
    ESP_LOGI(TAG, "BENCHMARK_START: official LVGL Benchmark");
#else
    ESP_LOGI(TAG, "DSI widgets example ready");
#endif
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
    ESP_LOGW(TAG, "DSI is disabled in this configuration");
#endif
}

/* ==================== [Static Functions] ================================== */

#if WT_BSP_DSI_ENABLED
#if CONFIG_EXAMPLE_DSI_DEMO_BENCHMARK && LV_VERSION_CHECK(9, 5, 0)
static void benchmark_finished(const lv_demo_benchmark_summary_t *summary)
{
    /* LVGL invokes this callback in its task while the port mutex is held. */
    lv_demo_benchmark_summary_display(summary);
    for (const lv_demo_benchmark_scene_dsc_t *scene = summary->scenes; scene->create_cb; ++scene) {
        if (scene->measurement_cnt > 0) {
            uint32_t count = scene->measurement_cnt;
            ESP_LOGI(TAG, "BENCHMARK_SCENE name=\"%s\" fps=%" PRIu32 " cpu=%" PRIu32
                     " render_ms=%" PRIu32 " flush_ms=%" PRIu32,
                     scene->name, scene->fps_avg / count, scene->cpu_avg_usage / count,
                     scene->render_avg_time / count, scene->flush_avg_time / count);
        }
    }
    if (summary->valid_scene_cnt > 0) {
        ESP_LOGI(TAG, "BENCHMARK_DONE scenes=%" PRId32 " avg_fps=%" PRId32 " avg_cpu=%" PRId32
                 " render_ms=%" PRId32 " flush_ms=%" PRId32,
                 summary->valid_scene_cnt, summary->total_avg_fps / summary->valid_scene_cnt,
                 summary->total_avg_cpu / summary->valid_scene_cnt,
                 summary->total_avg_render_time / summary->valid_scene_cnt,
                 summary->total_avg_flush_time / summary->valid_scene_cnt);
    } else {
        ESP_LOGE(TAG, "Benchmark completed without valid measurements");
    }
}
#endif
#endif
