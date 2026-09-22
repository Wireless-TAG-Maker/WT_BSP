/**
 * @file board_usb_descriptors.c
 * @author Wireless-Tag
 * @brief S31 composite UVC camera and CDC ACM descriptors.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "tusb.h"
#include "usb_descriptors.h"
#include <string.h>

/* ==================== [Defines] =========================================== */

#define BOARD_CDC_CONTROL_INTERFACE 2
#define BOARD_CDC_NOTIFICATION_EP 0x82
#define BOARD_CDC_OUT_EP 0x03
#define BOARD_CDC_IN_EP 0x83

#if CFG_TUD_CAM1_VIDEO_STREAMING_BULK
#define BOARD_VIDEO_DESCRIPTOR_LENGTH TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN
#else
#define BOARD_VIDEO_DESCRIPTOR_LENGTH TUD_VIDEO_CAPTURE_DESC_MJPEG_LEN
#endif
#define BOARD_CONFIGURATION_LENGTH (TUD_CONFIG_DESC_LEN + BOARD_VIDEO_DESCRIPTOR_LENGTH + TUD_CDC_DESC_LEN)

/* ==================== [Typedefs] ========================================== */
/* ==================== [Static Prototypes] ================================= */
/* ==================== [Static Variables] ================================== */

static const tusb_desc_device_t s_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = CONFIG_TUSB_VID,
    .idProduct = CONFIG_TUSB_PID,
    .bcdDevice = 0x0101,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const uint8_t s_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 4, 0, BOARD_CONFIGURATION_LENGTH, 0, 500),
#if CFG_TUD_CAM1_VIDEO_STREAMING_BULK
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                           UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT,
                                           UVC_CAM1_FRAME_RATE, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#else
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                      UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT,
                                      UVC_CAM1_FRAME_RATE, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#endif
    TUD_CDC_DESCRIPTOR(BOARD_CDC_CONTROL_INTERFACE, 5, BOARD_CDC_NOTIFICATION_EP, 8,
                       BOARD_CDC_OUT_EP, BOARD_CDC_IN_EP, CFG_TUD_CDC_EP_BUFSIZE),
};

/* The camera adapter sets interface string 4 to the configured product name. */
const char *string_desc_arr[] = {
    (const char[]) {0x09, 0x04},
    CONFIG_TUSB_MANUFACTURER,
    CONFIG_TUSB_PRODUCT,
    CONFIG_TUSB_SERIAL_NUM,
    "Wireless-Tag Camera",
    "Wireless-Tag CDC",
};
static uint16_t s_string[32];

/* ==================== [Macros] ============================================ */
/* ==================== [Global Functions] ================================== */

uint8_t const *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&s_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    return index == 0 ? s_configuration : NULL;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t count;
    if (index == 0) {
        s_string[1] = 0x0409;
        count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        count = strlen(string_desc_arr[index]);
        if (count > 31) {
            count = 31;
        }
        for (size_t i = 0; i < count; i++) {
            s_string[i + 1] = (uint8_t)string_desc_arr[index][i];
        }
    }
    s_string[0] = (TUSB_DESC_STRING << 8) | (2 * count + 2);
    return s_string;
}

/* ==================== [Static Functions] ================================== */
