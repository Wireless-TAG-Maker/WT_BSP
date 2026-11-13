/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef enum {
    GC2145_SENSOR_MODE_STOCK_VGA = 0,
    GC2145_SENSOR_MODE_SVGA_CROP,
    GC2145_SENSOR_MODE_720P,
} gc2145_sensor_mode_t;

/* Caller-owned, zero-initialized state. Do not copy an active instance. */
typedef struct {
    i2c_master_dev_handle_t device;
    bool detected;
    bool configured;
    bool streaming;
    gc2145_sensor_mode_t mode;
} gc2145_sensor_t;

/* Timing changes do not alter the VGA readout window, sampling pattern or DVP format. */
typedef struct {
    uint8_t pll_mode2;
    uint16_t hblank;
    uint16_t vblank;
    uint16_t anti_flicker_step;
    uint16_t exposure_levels[4];
} gc2145_sensor_timing_t;

typedef struct {
    uint16_t pid;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    uint16_t raw_width;
    uint16_t raw_height;
    uint16_t hblank;
    uint16_t vblank;
    uint16_t exposure;
    uint8_t pll_mode1;
    uint8_t pll_mode2;
    uint8_t clock_mode;
    uint8_t clock_divider;
    uint8_t scalar_mode;
    uint8_t analog_mode;
    uint8_t output_format;
    uint8_t sync_mode;
    uint8_t output_enable;
    uint8_t subsample;
    uint8_t subsample_mode;
    /* Page 0 registers 0x9b..0xa2: four row-pattern bytes, then four column bytes. */
    uint8_t subsample_pattern[8];
    uint8_t crop_enable;
    uint16_t sh_delay;
    uint8_t aec_enabled;
    uint16_t anti_flicker_step;
    uint16_t exposure_levels[4];
    /* Raw page 1 register 0x3c, including its minimum-exposure field. */
    uint8_t max_exp_level;
} gc2145_sensor_readback_t;

/*
 * All calls must be serialized by the caller. The caller owns the I2C bus,
 * supplies a running XCLK, and controls the DVP capture peripheral. The default
 * presets use 20 MHz; the caller must match XCLK to any explicit timing override.
 * The driver owns only its I2C device at 7-bit address 0x3c (100 kHz).
 * Init leaves DVP output disabled; call set_stream(true) after capture setup.
 * STOCK_VGA preserves the upstream VGA preset. SVGA_CROP preserves the SVGA
 * timing/ISP preset and selects its central 640x480 pixels at (80, 60), retaining
 * 80% of its width and height. Neither mode needs host-side VGA scaling.
 * The 720P mode requires a 24 MHz XCLK and outputs a centered 1280x720 UYVY window.
 * On init failure, a non-NULL device remains owned until deinit succeeds.
 */
esp_err_t gc2145_sensor_init(gc2145_sensor_t *sensor, i2c_master_bus_handle_t bus,
                             gc2145_sensor_mode_t mode);
esp_err_t gc2145_sensor_set_stream(gc2145_sensor_t *sensor, bool enable);
/*
 * Override only timing/AEC registers of a configured, stopped STOCK_VGA sensor.
 * PLL mode 2 is restricted to 0x84..0x86; HB is 12-bit, VB is 13-bit, and the
 * anti-flicker step and exposure levels must be nonzero 13-bit values.
 * Failure after touching hardware invalidates configuration; deinit/reinit is
 * then required. The device handle remains owned and can always be cleaned up.
 */
esp_err_t gc2145_sensor_set_timing(gc2145_sensor_t *sensor, const gc2145_sensor_timing_t *timing);
/*
 * May be called while streaming, serialized with all other sensor operations.
 * Enabling AEC ignores lines. Disabling AEC sets a manual exposure of 1..8191
 * sensor rows. Failure can leave AEC disabled or a partial manual exposure; the
 * caller must stop/reinitialize before relying on exposure or frame timing.
 */
esp_err_t gc2145_sensor_set_exposure(gc2145_sensor_t *sensor, bool enabled, uint16_t lines);
/* Reads hardware now, including live exposure; publishes no partial snapshot. */
esp_err_t gc2145_sensor_readback(gc2145_sensor_t *sensor, gc2145_sensor_readback_t *readback);

/*
 * Stop DVP output and remove the I2C device, leaving the caller's bus intact.
 * On error, retain this state and XCLK, then retry; never discard an active
 * device handle. A zero-initialized/already deinitialized instance is accepted.
 */
esp_err_t gc2145_sensor_deinit(gc2145_sensor_t *sensor);
