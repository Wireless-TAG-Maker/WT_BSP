/**
 * @file board_tinyusb_video.c
 * @author Wireless-Tag
 * @brief Terminate packet-aligned UVC bulk payloads on WT9932S31-TINY.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

/* Compile the managed driver once, with its original callback renamed. This
 * keeps its private stream state accessible without copying the driver or
 * changing files maintained by the component manager. CMake substitutes this
 * translation unit for S31 bulk profiles, including the factory composite. */
#define videod_xfer_cb board_tinyusb_video_xfer_cb
#include "class/video/video_device.c"
#undef videod_xfer_cb

/* ==================== [Defines] =========================================== */

#if TUSB_VERSION_MAJOR != 0 || TUSB_VERSION_MINOR != 19
#error "Review the S31 UVC bulk completion adapter when upgrading TinyUSB"
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

bool videod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                   uint32_t xferred_bytes)
{
    if (result == XFER_RESULT_SUCCESS && xferred_bytes != 0) {
        for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO_STREAMING; i++) {
            videod_streaming_interface_t *stream = &_videod_streaming_itf[i];
            uint16_t ep_offset = stream->desc.ep[0];
            if (ep_offset == 0) {
                continue;
            }
            const uint8_t *descriptor = _videod_itf[stream->index_vc].beg;
            const tusb_desc_endpoint_t *endpoint =
                (const tusb_desc_endpoint_t *)(descriptor + ep_offset);
            if (endpoint->bEndpointAddress != ep_addr) {
                continue;
            }

            uint16_t packet_size = tu_edpt_packet_size(endpoint);
            if (endpoint->bmAttributes.xfer == TUSB_XFER_BULK &&
                stream->buffer != NULL && stream->offset == stream->bufsize &&
                xferred_bytes < stream->max_payload_transfer_size &&
                packet_size != 0 && xferred_bytes % packet_size == 0) {
                /* A payload below dwMaxPayloadTransferSize must end with a
                 * short USB packet. Otherwise the host appends bytes from the
                 * next frame, even though this payload carries EOF. Keep the
                 * frame busy until the ZLP completes; its zero-byte callback
                 * then reaches the original driver's completion path. */
                TU_VERIFY(usbd_edpt_claim(rhport, ep_addr));
                return usbd_edpt_xfer(rhport, ep_addr, NULL, 0);
            }
            break;
        }
    }

    return board_tinyusb_video_xfer_cb(rhport, ep_addr, result, xferred_bytes);
}

/* ==================== [Static Functions] ================================== */
