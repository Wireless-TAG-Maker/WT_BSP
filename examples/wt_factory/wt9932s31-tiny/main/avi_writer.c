/**
 * @file avi_writer.c
 * @author Wireless-Tag
 * @brief Portable RIFF/AVI MJPEG writer with a bounded frame index.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "avi_writer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ==================== [Defines] =========================================== */

#define AVI_HEADER_SIZE 224U
#define AVI_MOVI_TYPE_OFFSET 220U
#define AVI_INDEX_ENTRY_SIZE 16U

/* ==================== [Typedefs] ========================================== */
/* ==================== [Static Prototypes] ================================= */

static int avi_write_header(avi_writer_t *writer, bool final);
static void avi_u32(uint8_t *p, uint32_t value);
static void avi_u16(uint8_t *p, uint16_t value);

/* ==================== [Static Variables] ================================== */
/* ==================== [Macros] ============================================ */
/* ==================== [Global Functions] ================================== */

int avi_writer_open(avi_writer_t *writer, const char *path, uint32_t width,
                    uint32_t height, uint32_t fps, uint32_t capacity)
{
    if (writer == NULL || path == NULL || writer->file != NULL || writer->index != NULL ||
            width == 0 || width > 32767 || height == 0 || height > 32767 ||
            fps == 0 || fps > 1000 || capacity == 0 || capacity > 100000) {
        errno = EINVAL;
        return -1;
    }
    avi_frame_index_t *index = calloc(capacity, sizeof(*index));
    if (index == NULL) {
        errno = ENOMEM;
        return -1;
    }
    int fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd < 0) {
        free(index);
        return -1;
    }
    FILE *file = fdopen(fd, "w+b");
    if (file == NULL) {
        int saved_errno = errno;
        close(fd);
        free(index);
        errno = saved_errno;
        return -1;
    }
    *writer = (avi_writer_t) {
        .file = file,
        .index = index,
        .capacity = capacity,
        .width = width,
        .height = height,
        .nominal_fps = fps,
    };
    if (avi_write_header(writer, false) != 0) {
        int saved_errno = errno;
        avi_writer_abort(writer);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

int avi_writer_append(avi_writer_t *writer, const uint8_t *jpeg, size_t length,
                      int64_t timestamp_us)
{
    if (writer == NULL || writer->file == NULL || writer->failed || jpeg == NULL ||
            length < 4 || length > UINT32_MAX - 9 || timestamp_us < 0 ||
            jpeg[0] != 0xff || jpeg[1] != 0xd8 || jpeg[length - 2] != 0xff ||
            jpeg[length - 1] != 0xd9 ||
            (writer->frames != 0 && timestamp_us <= writer->last_timestamp_us)) {
        errno = EINVAL;
        return -1;
    }
    if (writer->frames == writer->capacity) {
        errno = ENOSPC;
        return -1;
    }
    const uint32_t padded_size = (uint32_t)length + ((uint32_t)length & 1U);
    const uint64_t final_size = AVI_HEADER_SIZE + (uint64_t)writer->data_bytes +
        8 + padded_size + 8 + (uint64_t)(writer->frames + 1) * AVI_INDEX_ENTRY_SIZE;
    /* Stay below signed seek limits as well as RIFF's 32-bit chunk lengths. */
    if (final_size > INT32_MAX) {
        errno = EFBIG;
        return -1;
    }
    uint8_t chunk[8] = {'0', '0', 'd', 'c'};
    avi_u32(chunk + 4, length);
    const uint8_t padding = 0;
    if (fwrite(chunk, 1, sizeof(chunk), writer->file) != sizeof(chunk) ||
            fwrite(jpeg, 1, length, writer->file) != length ||
            ((length & 1U) && fwrite(&padding, 1, 1, writer->file) != 1)) {
        writer->failed = true;
        if (errno == 0) {
            errno = EIO;
        }
        return -1;
    }
    writer->index[writer->frames] = (avi_frame_index_t) {
        .offset = AVI_HEADER_SIZE - AVI_MOVI_TYPE_OFFSET + writer->data_bytes,
        .length = length,
    };
    if (writer->frames == 0) {
        writer->first_timestamp_us = timestamp_us;
    }
    writer->last_timestamp_us = timestamp_us;
    writer->frames++;
    writer->data_bytes += 8 + padded_size;
    if (length > writer->largest_frame) {
        writer->largest_frame = length;
    }
    return 0;
}

int avi_writer_close(avi_writer_t *writer)
{
    if (writer == NULL || writer->file == NULL) {
        errno = EINVAL;
        return -1;
    }
    int result = -1;
    int saved_errno = EIO;
    if (!writer->failed) {
        uint8_t index_header[8] = {'i', 'd', 'x', '1'};
        avi_u32(index_header + 4, writer->frames * AVI_INDEX_ENTRY_SIZE);
        bool complete = fwrite(index_header, 1, sizeof(index_header), writer->file) == sizeof(index_header);
        for (uint32_t i = 0; complete && i < writer->frames; i++) {
            uint8_t entry[AVI_INDEX_ENTRY_SIZE] = {'0', '0', 'd', 'c'};
            avi_u32(entry + 4, 0x10); /* Each JPEG is a key frame. */
            avi_u32(entry + 8, writer->index[i].offset);
            avi_u32(entry + 12, writer->index[i].length);
            complete = fwrite(entry, 1, sizeof(entry), writer->file) == sizeof(entry);
        }
        if (complete && avi_write_header(writer, true) == 0 &&
                fflush(writer->file) == 0 && fsync(fileno(writer->file)) == 0) {
            result = 0;
        } else if (errno != 0) {
            saved_errno = errno;
        }
    }
    if (fclose(writer->file) != 0) {
        result = -1;
        saved_errno = errno;
    }
    writer->file = NULL;
    free(writer->index);
    writer->index = NULL;
    writer->failed = result != 0;
    if (result != 0) {
        errno = saved_errno;
    }
    return result;
}

void avi_writer_abort(avi_writer_t *writer)
{
    if (writer == NULL) {
        return;
    }
    if (writer->file != NULL) {
        fclose(writer->file);
        writer->file = NULL;
    }
    free(writer->index);
    writer->index = NULL;
}

/* ==================== [Static Functions] ================================== */

static void avi_u32(uint8_t *p, uint32_t value)
{
    for (size_t i = 0; i < 4; i++) {
        p[i] = (uint8_t)(value >> (i * 8));
    }
}

static void avi_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static int avi_write_header(avi_writer_t *writer, bool final)
{
    uint8_t header[AVI_HEADER_SIZE] = {0};
    uint64_t interval = 1000000U / writer->nominal_fps;
    if (writer->frames > 1) {
        interval = (uint64_t)(writer->last_timestamp_us - writer->first_timestamp_us) /
                   (writer->frames - 1);
    }
    if (interval == 0 || interval > UINT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    uint32_t bytes_per_second = (uint32_t)((uint64_t)writer->largest_frame * 1000000 / interval);
    memcpy(header, "RIFF", 4);
    avi_u32(header + 4, AVI_HEADER_SIZE - 8 + writer->data_bytes +
            (final ? 8 + writer->frames * AVI_INDEX_ENTRY_SIZE : 0));
    memcpy(header + 8, "AVI LIST", 8);
    avi_u32(header + 16, 192);
    memcpy(header + 20, "hdrlavih", 8);
    avi_u32(header + 28, 56);
    avi_u32(header + 32, interval);
    avi_u32(header + 36, bytes_per_second);
    avi_u32(header + 44, final ? 0x10 : 0);
    avi_u32(header + 48, writer->frames);
    avi_u32(header + 56, 1);
    avi_u32(header + 60, writer->largest_frame);
    avi_u32(header + 64, writer->width);
    avi_u32(header + 68, writer->height);
    memcpy(header + 88, "LIST", 4);
    avi_u32(header + 92, 116);
    memcpy(header + 96, "strlstrh", 8);
    avi_u32(header + 104, 56);
    memcpy(header + 108, "vidsMJPG", 8);
    avi_u32(header + 128, interval);
    avi_u32(header + 132, 1000000);
    avi_u32(header + 140, writer->frames);
    avi_u32(header + 144, writer->largest_frame);
    avi_u32(header + 148, UINT32_MAX);
    avi_u16(header + 160, writer->width);
    avi_u16(header + 162, writer->height);
    memcpy(header + 164, "strf", 4);
    avi_u32(header + 168, 40);
    avi_u32(header + 172, 40);
    avi_u32(header + 176, writer->width);
    avi_u32(header + 180, writer->height);
    avi_u16(header + 184, 1);
    avi_u16(header + 186, 24);
    memcpy(header + 188, "MJPG", 4);
    avi_u32(header + 192, writer->largest_frame);
    memcpy(header + 212, "LIST", 4);
    avi_u32(header + 216, 4 + writer->data_bytes);
    memcpy(header + 220, "movi", 4);
    if (fseek(writer->file, 0, SEEK_SET) != 0 ||
            fwrite(header, 1, sizeof(header), writer->file) != sizeof(header)) {
        if (errno == 0) {
            errno = EIO;
        }
        return -1;
    }
    return 0;
}
