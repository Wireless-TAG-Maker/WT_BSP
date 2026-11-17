/**
 * @file camera_uvc_usb_descriptors.c
 * @author Wireless-Tag
 * @brief S31 camera USB descriptors for standalone 720p and composite profiles.
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

#define CAMERA_UVC_CDC_CONTROL_INTERFACE 2
#define CAMERA_UVC_CDC_NOTIFICATION_EP 0x82
#define CAMERA_UVC_CDC_OUT_EP 0x03
#define CAMERA_UVC_CDC_IN_EP 0x83

#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
#define CAMERA_UVC_INTERFACE_COUNT 4
#define CAMERA_UVC_CDC_DESCRIPTOR_LENGTH TUD_CDC_DESC_LEN
#define CAMERA_UVC_DEVICE_RELEASE 0x0101
#else
#define CAMERA_UVC_INTERFACE_COUNT ITF_NUM_TOTAL
#define CAMERA_UVC_CDC_DESCRIPTOR_LENGTH 0
#define CAMERA_UVC_DEVICE_RELEASE 0x0100
#endif

#if CFG_TUD_CAM1_VIDEO_STREAMING_BULK
#define CAMERA_UVC_VIDEO_DESCRIPTOR_LENGTH TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN
#else
#define CAMERA_UVC_FRAME_DESCRIPTOR_LENGTH (TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_DISC_LEN + 4)
#define CAMERA_UVC_VIDEO_DESCRIPTOR_LENGTH \
    (TUD_VIDEO_CAPTURE_DESC_MJPEG_LEN - TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_LEN + CAMERA_UVC_FRAME_DESCRIPTOR_LENGTH)
#define CAMERA_UVC_FRAME_INTERVAL (10000000 / UVC_CAM1_FRAME_RATE)
#define CAMERA_UVC_RAW_FRAME_BYTES (UVC_CAM1_FRAME_WIDTH * UVC_CAM1_FRAME_HEIGHT * 2)
#endif
#define CAMERA_UVC_CONFIGURATION_LENGTH \
    (TUD_CONFIG_DESC_LEN + CAMERA_UVC_VIDEO_DESCRIPTOR_LENGTH + CAMERA_UVC_CDC_DESCRIPTOR_LENGTH)

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
    .bcdDevice = CAMERA_UVC_DEVICE_RELEASE,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const uint8_t s_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, CAMERA_UVC_INTERFACE_COUNT, 0, CAMERA_UVC_CONFIGURATION_LENGTH, 0, 500),
#if CFG_TUD_CAM1_VIDEO_STREAMING_BULK
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                           UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT,
                                           UVC_CAM1_FRAME_RATE, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#else
    TUD_VIDEO_DESC_IAD(ITF_NUM_VIDEO_CONTROL, 2, 4),
    TUD_VIDEO_DESC_STD_VC(ITF_NUM_VIDEO_CONTROL, 0, 4),
    TUD_VIDEO_DESC_CS_VC(0x0150, TUD_VIDEO_DESC_CAMERA_TERM_LEN + TUD_VIDEO_DESC_OUTPUT_TERM_LEN,
                         UVC_CLOCK_FREQUENCY, ITF_NUM_VIDEO_STREAMING),
    TUD_VIDEO_DESC_CAMERA_TERM(UVC_ENTITY_CAP_INPUT_TERMINAL, 0, 0, 0, 0, 0, 0),
    TUD_VIDEO_DESC_OUTPUT_TERM(UVC_ENTITY_CAP_OUTPUT_TERMINAL, VIDEO_TT_STREAMING, 0,
                               UVC_ENTITY_CAP_INPUT_TERMINAL, 0),
    TUD_VIDEO_DESC_STD_VS(ITF_NUM_VIDEO_STREAMING, 0, 0, 4),
    TUD_VIDEO_DESC_CS_VS_INPUT(1, TUD_VIDEO_DESC_CS_VS_FMT_MJPEG_LEN + CAMERA_UVC_FRAME_DESCRIPTOR_LENGTH +
                               TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING_LEN, EPNUM_CAM1_VIDEO_IN, 0,
                               UVC_ENTITY_CAP_OUTPUT_TERMINAL, 0, 0, 0, 0),
    TUD_VIDEO_DESC_CS_VS_FMT_MJPEG(1, 1, 0, 1, 0, 0, 0, 0),
    /* Advertise one discrete interval so the host cannot select 15 fps. */
    CAMERA_UVC_FRAME_DESCRIPTOR_LENGTH, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_FRAME_MJPEG,
    1, 0, U16_TO_U8S_LE(UVC_CAM1_FRAME_WIDTH), U16_TO_U8S_LE(UVC_CAM1_FRAME_HEIGHT),
    U32_TO_U8S_LE(CAMERA_UVC_RAW_FRAME_BYTES * 8),
    U32_TO_U8S_LE(CAMERA_UVC_RAW_FRAME_BYTES * 8 * UVC_CAM1_FRAME_RATE),
    U32_TO_U8S_LE(CAMERA_UVC_RAW_FRAME_BYTES), U32_TO_U8S_LE(CAMERA_UVC_FRAME_INTERVAL),
    1, U32_TO_U8S_LE(CAMERA_UVC_FRAME_INTERVAL),
    TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING(VIDEO_COLOR_PRIMARIES_BT709, VIDEO_COLOR_XFER_CH_BT709,
                                        VIDEO_COLOR_COEF_SMPTE170M),
    TUD_VIDEO_DESC_STD_VS(ITF_NUM_VIDEO_STREAMING, 1, 1, 4),
    TUD_VIDEO_DESC_EP_ISO(EPNUM_CAM1_VIDEO_IN, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE, 1),
#endif
#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
    TUD_CDC_DESCRIPTOR(CAMERA_UVC_CDC_CONTROL_INTERFACE, 5, CAMERA_UVC_CDC_NOTIFICATION_EP, 8,
                       CAMERA_UVC_CDC_OUT_EP, CAMERA_UVC_CDC_IN_EP, CFG_TUD_CDC_EP_BUFSIZE),
#endif
};
_Static_assert(sizeof(s_configuration) == CAMERA_UVC_CONFIGURATION_LENGTH, "USB configuration length mismatch");

/* The application owns the product and interface names in this profile. */
const char *string_desc_arr[] = {
    (const char[]) {0x09, 0x04},
    CONFIG_TUSB_MANUFACTURER,
    CONFIG_TUSB_PRODUCT,
    CONFIG_TUSB_SERIAL_NUM,
#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
    CONFIG_TUSB_PRODUCT,
    "Wireless-Tag CDC",
#else
    "UVC CAM1",
#endif
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
