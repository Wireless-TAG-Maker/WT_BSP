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

/* Caller-owned, zero-initialized state. Do not copy an active instance. */
typedef struct {
    i2c_master_dev_handle_t device;
    bool detected;
    bool configured;
} gc2145_sensor_t;

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
 * supplies a running 24 MHz XCLK, and controls the DVP capture peripheral.
 * The driver owns only its I2C device at 7-bit address 0x3c (100 kHz).
 * Init leaves DVP output disabled; call set_stream(true) after capture setup.
 * On init failure, a non-NULL device remains owned until deinit succeeds.
 */
esp_err_t gc2145_sensor_init(gc2145_sensor_t *sensor, i2c_master_bus_handle_t bus);
esp_err_t gc2145_sensor_set_stream(gc2145_sensor_t *sensor, bool enable);
/* Reads hardware now, including live exposure; publishes no partial snapshot. */
esp_err_t gc2145_sensor_readback(gc2145_sensor_t *sensor, gc2145_sensor_readback_t *readback);

/*
 * Stop DVP output and remove the I2C device, leaving the caller's bus intact.
 * On error, retain this state and XCLK, then retry; never discard an active
 * device handle. A zero-initialized/already deinitialized instance is accepted.
 */
esp_err_t gc2145_sensor_deinit(gc2145_sensor_t *sensor);
