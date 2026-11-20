/**
 * @file factory_compact_controls.c
 * @author Wireless-Tag
 * @brief Compact RGB, storage and network controls for the P4 factory examples.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* ==================== [Includes] ========================================== */

#include "factory_compact_controls.h"

/* ==================== [Static Prototypes] ================================= */

static lv_obj_t *create_group(lv_obj_t *parent, lv_flex_flow_t flow);
static lv_obj_t *create_channel(lv_obj_t *parent, const char *name, lv_palette_t palette, lv_event_cb_t callback);

/* ==================== [Global Functions] ================================== */

factory_compact_controls_t factory_compact_controls_create(lv_obj_t *screen, bool with_wifi,
                                                           lv_event_cb_t slider_cb, lv_event_cb_t mount_cb)
{
    factory_compact_controls_t controls = {0};
    controls.panel = create_group(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(controls.panel, lv_pct(100));
    lv_obj_set_flex_grow(controls.panel, 1);
    lv_obj_set_style_pad_hor(controls.panel, 10, 0);
    lv_obj_set_style_pad_ver(controls.panel, 4, 0);
    lv_obj_set_style_pad_row(controls.panel, 4, 0);
    lv_obj_set_style_bg_color(controls.panel, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(controls.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(controls.panel, lv_color_white(), 0);

    lv_obj_t *led = create_group(controls.panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(led, lv_pct(100));
    lv_obj_set_flex_grow(led, 1);
    lv_obj_set_style_pad_column(led, 12, 0);

    lv_obj_t *sample = create_group(led, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(sample, 32, lv_pct(100));
    lv_obj_set_flex_align(sample, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sample, 6, 0);
    lv_label_set_text(lv_label_create(sample), "LED");
    controls.swatch = lv_obj_create(sample);
    lv_obj_set_size(controls.swatch, 24, 24);
    lv_obj_set_style_radius(controls.swatch, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(controls.swatch, 1, 0);
    lv_obj_set_style_border_color(controls.swatch, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_bg_color(controls.swatch, lv_color_black(), 0);

    lv_obj_t *channels = create_group(led, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(channels, lv_pct(100));
    lv_obj_set_flex_grow(channels, 1);
    controls.red = create_channel(channels, "R", LV_PALETTE_RED, slider_cb);
    controls.green = create_channel(channels, "G", LV_PALETTE_GREEN, slider_cb);
    controls.blue = create_channel(channels, "B", LV_PALETTE_BLUE, slider_cb);

    lv_obj_t *sd = create_group(controls.panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(sd, lv_pct(100), 48);
    lv_obj_set_style_pad_column(sd, 12, 0);
    lv_obj_t *mount = lv_button_create(sd);
    lv_obj_set_size(mount, 100, 44);
    lv_obj_t *mount_label = lv_label_create(mount);
    lv_label_set_text(mount_label, "Mount SD");
    lv_obj_center(mount_label);
    lv_obj_add_event_cb(mount, mount_cb, LV_EVENT_CLICKED, NULL);
    controls.sd_status = lv_label_create(sd);
    lv_obj_set_flex_grow(controls.sd_status, 1);
    lv_label_set_text(controls.sd_status, "Status: Idle");

    if (with_wifi) {
        lv_obj_t *network = create_group(controls.panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_size(network, lv_pct(100), 44);
        lv_obj_set_flex_align(network, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(network, 2, 0);

        lv_obj_t *line = create_group(network, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(line, 6, 0);
        lv_label_set_text(lv_label_create(line), "Wi-Fi");
        controls.wifi_status = lv_label_create(line);
        lv_obj_set_flex_grow(controls.wifi_status, 1);
        lv_label_set_text(controls.wifi_status, "Disconnected");
        controls.clock = lv_label_create(line);
        lv_label_set_text(controls.clock, "1970-01-01 00:00:00");
        controls.ap_name = lv_label_create(network);
        lv_obj_set_width(controls.ap_name, lv_pct(100));
        lv_obj_set_height(controls.ap_name, lv_font_get_line_height(lv_obj_get_style_text_font(controls.ap_name, 0)));
        lv_label_set_long_mode(controls.ap_name, LV_LABEL_LONG_DOT);
        lv_label_set_text(controls.ap_name, "AP: -");
    }
    return controls;
}

/* ==================== [Static Functions] ================================== */

static lv_obj_t *create_group(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *group = lv_obj_create(parent);
    lv_obj_remove_style_all(group);
    lv_obj_remove_flag(group, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(group, flow);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return group;
}

static lv_obj_t *create_channel(lv_obj_t *parent, const char *name, lv_palette_t palette, lv_event_cb_t callback)
{
    lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_style_pad_right(row, 10, 0);
    lv_obj_t *label = lv_label_create(row);
    lv_obj_set_width(label, 14);
    lv_label_set_text(label, name);

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_height(slider, 14);
    lv_obj_set_flex_grow(slider, 1);
    lv_obj_set_ext_click_area(slider, 12);
    lv_slider_set_range(slider, 0, 255);
    lv_color_t color = lv_palette_main(palette);
    lv_obj_set_style_bg_color(slider, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, color, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, callback, LV_EVENT_VALUE_CHANGED, NULL);
    return slider;
}
