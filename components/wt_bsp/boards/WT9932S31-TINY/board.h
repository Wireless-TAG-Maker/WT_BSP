/**
 * @file board.h
 * @author Wireless-Tag
 * @brief WT9932S31-TINY private board interface and schematic pinout.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __BOARD_H__
#define __BOARD_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

#define BOARD_BUTTON_GPIO_NUM 61
#define BOARD_RGB_GPIO_NUM 19

/* SD socket uses the dedicated slot 0 pins. Card power is external 3.3 V;
 * on-chip LDO 1 supplies the SD pad circuitry at 1.8 V. */
#define BOARD_SDMMC_SLOT 0
#define BOARD_SDMMC_LDO_CHANNEL 1
#define BOARD_SDMMC_CLK_GPIO 24
#define BOARD_SDMMC_CMD_GPIO 25
#define BOARD_SDMMC_D0_GPIO 20
#define BOARD_SDMMC_D1_GPIO 21
#define BOARD_SDMMC_D2_GPIO 22
#define BOARD_SDMMC_D3_GPIO 23

/* Both supported modules use the FPC camera connector. RESET/PWDN are
 * biased on the board and are not connected to a controllable GPIO. */
#define BOARD_DVP_SCCB_PORT 0
#define BOARD_DVP_SCCB_FREQ_HZ 100000
#define BOARD_DVP_SCCB_SCL_GPIO 1
#define BOARD_DVP_SCCB_SDA_GPIO 0
#define BOARD_DVP_RESET_GPIO -1
#define BOARD_DVP_PWDN_GPIO -1
#define BOARD_DVP_XCLK_FREQ_HZ 20000000
#define BOARD_DVP_XCLK_GPIO 55
#define BOARD_DVP_PCLK_GPIO 54
#define BOARD_DVP_VSYNC_GPIO 56
#define BOARD_DVP_DE_GPIO 57
#define BOARD_DVP_D0_GPIO 46
#define BOARD_DVP_D1_GPIO 47
#define BOARD_DVP_D2_GPIO 48
#define BOARD_DVP_D3_GPIO 49
#define BOARD_DVP_D4_GPIO 50
#define BOARD_DVP_D5_GPIO 51
#define BOARD_DVP_D6_GPIO 52
#define BOARD_DVP_D7_GPIO 53

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/** @brief Get this board's static operation table; ownership stays with BSP. */
wt_bsp_interface_t *board_get_bsp_interface(void);

#if WT_BSP_USB_DEVICE_UVC_ENABLED
/** @brief Initialize board-owned DVP/JPEG/UVC resources; return ESP_OK or an initialization error. */
esp_err_t board_usb_device_uvc_init(void);
/** @brief Stop UVC before releasing video resources; return ESP_OK or the cleanup error. */
esp_err_t board_usb_device_uvc_deinit(void);
#endif

#if WT_BSP_CAMERA_ENABLED
/** @brief Prepare the board's shared sensor and return its selected JPEG format. */
esp_err_t board_camera_prepare(wt_bsp_camera_format_t *format);
/** @brief Consume one local JPEG while the USB host does not own capture. */
esp_err_t board_camera_capture(wt_bsp_camera_frame_cb_t callback, void *user_data);
/** @brief Read health shared by local and UVC capture paths. */
esp_err_t board_camera_get_status(wt_bsp_camera_status_t *status);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __BOARD_H__
