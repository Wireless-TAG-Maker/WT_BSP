/**
 * @file board_tusb_config.h
 * @author Wireless-Tag
 * @brief Sensor-dependent TinyUSB endpoint configuration.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_TUSB_CONFIG_H__
#define __BOARD_TUSB_CONFIG_H__

/* ==================== [Includes] ========================================== */

/* Keep the locked UVC component's descriptors and platform configuration. */
#include "tusb_config.h"

/* ==================== [Defines] =========================================== */

#if !CONFIG_TINYUSB_RHPORT_HS || CONFIG_UVC_SUPPORT_TWO_CAM
#error "The project camera profiles require one High-Speed UVC camera"
#endif

/* The project sensor choice owns the transport. A saved component-level USB
 * choice otherwise survives a GC2145/OV3660 switch in Kconfig. Derive the
 * effective macros here so changing only Camera sensor remains sufficient. */
#undef CFG_TUD_CAM1_VIDEO_STREAMING_BULK
#undef CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE
#undef CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE
#undef UVC_CAM1_BULK_MODE

#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
#define CFG_TUD_CDC 1
#define CFG_TUD_CDC_RX_BUFSIZE 2048
#define CFG_TUD_CDC_TX_BUFSIZE 2048
#define CFG_TUD_CDC_EP_BUFSIZE 512
#endif

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145 || CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
/* A UVC bulk payload may span multiple USB packets. Keep the descriptor's
 * CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE at 512 bytes (HS wMaxPacketSize),
 * but negotiate and stage 16 KiB payloads. The factory composite device uses
 * bulk for both sensors: CDC/SD readback can cause ISO packet loss on OV3660.
 * Besides reducing transfer overhead,
 * this avoids the Linux UVC allocation failure for a one-packet payload. */
#define UVC_CAM1_BULK_MODE
#define CFG_TUD_CAM1_VIDEO_STREAMING_BULK 1
#define CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE 512
#define CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE 16384
#elif CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
/* Preserve OV3660's historically verified isochronous transport. */
#define CFG_TUD_CAM1_VIDEO_STREAMING_BULK 0
#define CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE 1023
#define CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE 1023
#else
#error "Select a project camera sensor"
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

#endif // __BOARD_TUSB_CONFIG_H__
