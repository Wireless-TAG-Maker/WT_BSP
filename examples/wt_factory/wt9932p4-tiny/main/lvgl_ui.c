/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"
#include "src/misc/cache/lv_cache.h"
#include <stdatomic.h>
#include <stdio.h>
#include "esp_err.h"

extern void example_set_led_color(uint8_t r, uint8_t g, uint8_t b);
extern esp_err_t example_sdcard_mount(void);
extern uint64_t example_sdcard_get_capacity(void);

static lv_obj_t *cam_display;
static lv_obj_t *cam_msg;
static lv_obj_t *bottom_row;
static atomic_bool s_camera_fullscreen;

bool camera_is_fullscreen(void)
{
    return atomic_load(&s_camera_fullscreen);
}

static void cam_click_event_cb(lv_event_t * e)
{
    bool fullscreen = !camera_is_fullscreen();
    atomic_store(&s_camera_fullscreen, fullscreen);
    if (fullscreen) {
        lv_obj_add_flag(bottom_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(cam_display, 480, 640);
    } else {
        lv_obj_remove_flag(bottom_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(cam_display, 480, 384);
    }
    // Remove software rotation and scale, we will use PPA hardware in main.c
}

static lv_image_dsc_t cam_img_dsc = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB888,
        .flags = 0,
        .w = 480,
        .h = 384,
        .stride = 480 * 3,
    },
    .data_size = 480 * 384 * 3,
    .data = NULL
};
 
void update_camera_frame(uint8_t *buf, uint32_t width, uint32_t height)
{
    if (cam_msg != NULL && !lv_obj_has_flag(cam_msg, LV_OBJ_FLAG_HIDDEN)) {
        // Hide the "Initializing..." message on the first frame
        lv_obj_add_flag(cam_msg, LV_OBJ_FLAG_HIDDEN);
    }

    cam_img_dsc.header.w = width;
    cam_img_dsc.header.h = height;
    cam_img_dsc.header.stride = width * 3;
    cam_img_dsc.data_size = width * height * 3;
    cam_img_dsc.data = buf;
    
    // Set the source and invalidate to force a redraw
    lv_image_set_src(cam_display, &cam_img_dsc);
    lv_obj_invalidate(cam_display);
}

void set_camera_error(const char *msg)
{
    if (cam_msg) {
        lv_label_set_text(cam_msg, msg);
        lv_obj_remove_flag(cam_msg, LV_OBJ_FLAG_HIDDEN);
    }
}


static lv_obj_t *slider_r;
static lv_obj_t *slider_g;
static lv_obj_t *slider_b;
static lv_obj_t *led_color_rect;
static lv_obj_t *label_sd_info;
static bool s_updating_led_ui;

void update_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (slider_r == NULL || slider_g == NULL || slider_b == NULL || led_color_rect == NULL) {
        return;
    }

    s_updating_led_ui = true;
    lv_slider_set_value(slider_r, r, LV_ANIM_OFF);
    lv_slider_set_value(slider_g, g, LV_ANIM_OFF);
    lv_slider_set_value(slider_b, b, LV_ANIM_OFF);
    lv_obj_invalidate(slider_r);
    lv_obj_invalidate(slider_g);
    lv_obj_invalidate(slider_b);
    lv_obj_set_style_bg_color(led_color_rect, lv_color_make(r, g, b), 0);
    lv_obj_invalidate(led_color_rect);
    s_updating_led_ui = false;
}

static void slider_event_cb(lv_event_t * e)
{
    if (s_updating_led_ui) {
        return;
    }

    uint8_t r = lv_slider_get_value(slider_r);
    uint8_t g = lv_slider_get_value(slider_g);
    uint8_t b = lv_slider_get_value(slider_b);

    lv_obj_set_style_bg_color(led_color_rect, lv_color_make(r, g, b), 0);
    example_set_led_color(r, g, b);
}

static void sd_mount_event_cb(lv_event_t * e)
{
    esp_err_t ret = example_sdcard_mount();
    if (ret == ESP_OK) {
        uint64_t capacity = example_sdcard_get_capacity();
        // Calculate GB and fractional part using integer arithmetic to avoid %f formatting issues
        uint32_t capacity_gb_int = (uint32_t)(capacity / (1024 * 1024 * 1024));
        uint32_t capacity_gb_frac = (uint32_t)(((capacity % (1024 * 1024 * 1024)) * 100) / (1024 * 1024 * 1024));
        
        lv_label_set_text_fmt(label_sd_info, "Success\nSize: %"PRIu32".%02"PRIu32" GB", capacity_gb_int, capacity_gb_frac);
        lv_obj_set_style_text_color(label_sd_info, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text_fmt(label_sd_info, "Failed\nErr: %s", esp_err_to_name(ret));
        lv_obj_set_style_text_color(label_sd_info, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

void lvgl_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_row(scr, 2, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* --- 1. Camera Section (Top 60%) --- */
    cam_display = lv_image_create(scr);
    lv_obj_set_size(cam_display, 480, 384);
    lv_obj_set_style_bg_color(cam_display, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(cam_display, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cam_display, 0, 0);
    lv_obj_set_style_border_width(cam_display, 0, 0);
    lv_image_set_inner_align(cam_display, LV_IMAGE_ALIGN_CENTER);
    lv_obj_remove_flag(cam_display, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cam_display, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cam_display, cam_click_event_cb, LV_EVENT_CLICKED, NULL);
    
    cam_msg = lv_label_create(cam_display);
    lv_label_set_text(cam_msg, "Camera Initializing...");
    lv_obj_set_style_text_color(cam_msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cam_msg);

    /* --- Bottom Layout (Remaining 40%) --- */
    bottom_row = lv_obj_create(scr);
    lv_obj_set_size(bottom_row, 480, 254);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(bottom_row, 0, 0);
    lv_obj_set_style_pad_column(bottom_row, 2, 0);
    lv_obj_set_style_bg_opa(bottom_row, 0, 0);
    lv_obj_set_style_border_width(bottom_row, 0, 0);
    lv_obj_remove_flag(bottom_row, LV_OBJ_FLAG_SCROLLABLE);

    /* --- 2. RGB LED Section --- */
    lv_obj_t *rgb_cont = lv_obj_create(bottom_row);
    lv_obj_set_height(rgb_cont, 254);
    lv_obj_set_flex_grow(rgb_cont, 1);
    lv_obj_set_flex_flow(rgb_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rgb_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(rgb_cont, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(rgb_cont, 0, 0);
    lv_obj_set_style_border_width(rgb_cont, 0, 0);
    lv_obj_set_style_pad_row(rgb_cont, 10, 0);
    lv_obj_remove_flag(rgb_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_rgb = lv_label_create(rgb_cont);
    lv_label_set_text(title_rgb, "LED Control");
    lv_obj_set_style_text_color(title_rgb, lv_color_hex(0xFFFFFF), 0);

    led_color_rect = lv_obj_create(rgb_cont);
    lv_obj_set_size(led_color_rect, 30, 30);
    lv_obj_set_style_bg_color(led_color_rect, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_radius(led_color_rect, 15, 0);
    lv_obj_remove_flag(led_color_rect, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *slider_sub_cont = lv_obj_create(rgb_cont);
    lv_obj_set_size(slider_sub_cont, lv_pct(100), 160);
    lv_obj_set_flex_flow(slider_sub_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(slider_sub_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(slider_sub_cont, 0, 0);
    lv_obj_set_style_border_width(slider_sub_cont, 0, 0);
    lv_obj_set_style_pad_all(slider_sub_cont, 0, 0);
    lv_obj_remove_flag(slider_sub_cont, LV_OBJ_FLAG_SCROLLABLE);

    slider_r = lv_slider_create(slider_sub_cont);
    lv_obj_set_size(slider_r, lv_pct(90), 17);
    lv_slider_set_range(slider_r, 0, 255);
    lv_obj_set_style_bg_color(slider_r, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider_r, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    slider_g = lv_slider_create(slider_sub_cont);
    lv_obj_set_size(slider_g, lv_pct(90), 17);
    lv_slider_set_range(slider_g, 0, 255);
    lv_obj_set_style_bg_color(slider_g, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider_g, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    slider_b = lv_slider_create(slider_sub_cont);
    lv_obj_set_size(slider_b, lv_pct(90), 17);
    lv_slider_set_range(slider_b, 0, 255);
    lv_obj_set_style_bg_color(slider_b, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider_b, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* --- 3. SD Card Section --- */
    lv_obj_t *sd_cont = lv_obj_create(bottom_row);
    lv_obj_set_size(sd_cont, LV_SIZE_CONTENT, 254);
    lv_obj_set_flex_flow(sd_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sd_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(sd_cont, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(sd_cont, 0, 0);
    lv_obj_set_style_border_width(sd_cont, 0, 0);
    lv_obj_set_style_pad_hor(sd_cont, 10, 0);
    lv_obj_set_style_pad_row(sd_cont, 20, 0);
    lv_obj_remove_flag(sd_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_sd = lv_button_create(sd_cont);
    lv_obj_t *label_btn = lv_label_create(btn_sd);
    lv_label_set_text(label_btn, "SD Card Mount");
    lv_obj_add_event_cb(btn_sd, sd_mount_event_cb, LV_EVENT_CLICKED, NULL);

    label_sd_info = lv_label_create(sd_cont);
    lv_label_set_text(label_sd_info, "Status: Idle");
    lv_obj_set_style_text_color(label_sd_info, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_long_mode(label_sd_info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_sd_info, 120);
    lv_obj_set_style_text_align(label_sd_info, LV_TEXT_ALIGN_CENTER, 0);
}
