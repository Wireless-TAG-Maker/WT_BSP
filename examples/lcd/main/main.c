/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "wt_bsp.h"

static const char *TAG = "lcd_example";

#define EXAMPLE_LVGL_DRAW_BUF_LINES    200
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    wt_bsp_lcd_t panel_handle = (wt_bsp_lcd_t)lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

static void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_port_task(void *arg)
{
    while (1) {
        uint32_t time_till_next_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
    }
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

extern void example_lvgl_demo_ui(lv_display_t *disp);

void app_main(void)
{
    ESP_ERROR_CHECK(wt_bsp_init());
    wt_bsp_lcd_t lcd_handle = wt_bsp_get_lcd();
    if (lcd_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get LCD handle");
        return;
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    
    uint32_t h_res = 0;
    uint32_t v_res = 0;
#if CONFIG_WT_BSP_LCD_ILI9881C
    h_res = 800;
    v_res = 1280;
#elif CONFIG_WT_BSP_LCD_EK79007
    h_res = 1024;
    v_res = 600;
#endif

    lv_display_t *display = lv_display_create(h_res, v_res);
    lv_display_set_user_data(display, lcd_handle);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);

    size_t draw_buffer_sz = h_res * EXAMPLE_LVGL_DRAW_BUF_LINES * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(lcd_handle, &cbs, display));

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreate(example_lvgl_port_task, "LVGL", 8192, NULL, 5, NULL);

    example_lvgl_demo_ui(display);
}
