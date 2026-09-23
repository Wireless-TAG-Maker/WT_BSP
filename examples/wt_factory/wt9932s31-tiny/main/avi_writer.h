/**
 * @file avi_writer.h
 * @author Wireless-Tag
 * @brief Bounded MJPEG AVI recording with exclusive file creation.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __AVI_WRITER_H__
#define __AVI_WRITER_H__

/* ==================== [Includes] ========================================== */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */

/** @brief One compressed frame's position relative to the movi list type. */
typedef struct {
    uint32_t offset; /*!< AVI index offset. */
    uint32_t length; /*!< JPEG bytes, excluding WORD padding. */
} avi_frame_index_t;

/** @brief Single-task writer; initialize to zero and close/abort before reuse. */
typedef struct {
    FILE *file;                 /*!< Owned output stream, or NULL. */
    avi_frame_index_t *index;    /*!< Owned bounded index table. */
    uint32_t capacity;          /*!< Maximum frames in this segment. */
    uint32_t frames;            /*!< Successfully written frames. */
    uint32_t width;             /*!< Video width. */
    uint32_t height;            /*!< Video height. */
    uint32_t nominal_fps;       /*!< Fallback rate for a one-frame segment. */
    uint32_t data_bytes;        /*!< movi chunks including headers and padding. */
    uint32_t largest_frame;     /*!< Largest JPEG in the segment. */
    int64_t first_timestamp_us; /*!< First source frame time. */
    int64_t last_timestamp_us;  /*!< Most recent source frame time. */
    bool failed;               /*!< Sticky write failure; keep the partial file. */
} avi_writer_t;

/* ==================== [Macros] ============================================ */
/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Create a new AVI file; existing files are never overwritten.
 * @param[in,out] writer Zero-initialized or previously closed writer.
 * @param[in] path New file path, usually ending in .part until finalized.
 * @param[in] width Image width, 1..32767.
 * @param[in] height Image height, 1..32767.
 * @param[in] fps Nominal frame rate, 1..1000.
 * @param[in] capacity Maximum frames, 1..100000; bounds the index allocation.
 * @return 0 on success, -1 with errno on invalid arguments, existing file or I/O failure.
 */
int avi_writer_open(avi_writer_t *writer, const char *path, uint32_t width,
                    uint32_t height, uint32_t fps, uint32_t capacity);

/**
 * @brief Append one complete JPEG without encoding it again.
 * @param[in,out] writer Open writer; partial I/O failure makes it unusable.
 * @param[in] jpeg Borrowed JPEG bytes, used synchronously.
 * @param[in] length Complete JPEG length including SOI and EOI markers.
 * @param[in] timestamp_us Monotonic frame time; must increase after the first frame.
 * @return 0 on success, -1 with errno; ENOSPC when the frame index is full.
 */
int avi_writer_append(avi_writer_t *writer, const uint8_t *jpeg, size_t length,
                      int64_t timestamp_us);

/**
 * @brief Write the AVI index/header, flush and close, preserving frame statistics.
 * @param[in,out] writer Open writer. On all paths, file/index ownership is released.
 * @return 0 on success, -1 with errno on I/O failure or invalid state.
 */
int avi_writer_close(avi_writer_t *writer);

/**
 * @brief Release resources after failure, retaining the incomplete file for diagnosis.
 * @param[in,out] writer Writer, including a partially initialized one; NULL is allowed.
 */
void avi_writer_abort(avi_writer_t *writer);

#endif // __AVI_WRITER_H__
