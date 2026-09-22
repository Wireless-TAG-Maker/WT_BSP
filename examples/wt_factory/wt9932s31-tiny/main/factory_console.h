/**
 * @file factory_console.h
 * @author Wireless-Tag
 * @brief Factory CDC status and recorded-video readback.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __FACTORY_CONSOLE_H__
#define __FACTORY_CONSOLE_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* ==================== [Defines] =========================================== */
#define FACTORY_PATH_SIZE 192

/* ==================== [Typedefs] ========================================== */

/** @brief Consistent application snapshot, transferred through a one-item queue. */
typedef struct {
    bool usb;               /*!< USB mode selected. */
    bool recording_enabled; /*!< SD recording compiled in via menuconfig. */
    bool paused;            /*!< Preserved offline pause state. */
    bool camera_ok;         /*!< Sensor/capture health. */
    bool sd_ok;             /*!< Card mounted and checked I/O healthy. */
    uint64_t recorded;      /*!< JPEG frames appended during this boot. */
    uint32_t segments;      /*!< Successfully finalized AVI files. */
    uint32_t free_internal; /*!< Free internal heap for factory diagnostics. */
    uint32_t free_psram;    /*!< Free external heap for factory diagnostics. */
    char file[32];          /*!< Current or most recently finalized AVI filename. */
} factory_status_t;

/* ==================== [Macros] ============================================ */
/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Start bounded CDC commands: status, ping, list, get <recorded AVI name>.
 * @param[in] cdc Board-owned CDC handle, kept alive for the task's lifetime.
 * @param[in] directory Recording directory; copied into task-owned storage.
 * @param[in] status One-item factory_status_t queue, owned by the application.
 * @return ESP_OK or ESP_ERR_NO_MEM/ESP_ERR_INVALID_ARG. Task lives until reset.
 */
esp_err_t factory_console_start(wt_bsp_usb_device_cdc_t cdc, const char *directory,
                                QueueHandle_t status);

#endif // __FACTORY_CONSOLE_H__
