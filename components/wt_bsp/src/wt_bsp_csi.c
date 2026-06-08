/**
 * @file wt_bsp_csi.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-06-03
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_csi.h"
#include "wt_bsp_internal.h"

#if WT_BSP_CSI_ENABLE_IS_ENABLED

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ==================== [Defines] =========================================== */

#define CSI_STREAM_TASK_STACK_SIZE    (4096)
#define CSI_STREAM_TASK_PRIO          (5)

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static void wt_bsp_csi_stream_task(void *arg);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_csi";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_csi_init(wt_bsp_csi_t csi, const wt_bsp_csi_info_t *info)
{
    esp_err_t ret = ESP_OK;

    if (csi == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(csi, 0, sizeof(*csi));
    csi->info = *info;
    csi->v4l2_fd = -1;

    esp_video_init_csi_config_t csi_config[] = {
        {
            .sccb_config = {
                .init_sccb = true,
                .i2c_config = {
                    .port      = 0,
                    .scl_pin   = info->sccb_scl_pin,
                    .sda_pin   = info->sccb_sda_pin,
                },
                .freq      = 100000,
            },
            .reset_pin = info->reset_pin,
            .pwdn_pin  = info->pwdn_pin,
        },
    };

    esp_video_init_config_t cam_config = {
        .csi = csi_config,
    };

    /* Give the sensor some time to power up, especially if LDOs were just enabled */
    vTaskDelay(pdMS_TO_TICKS(50));

    ret = esp_video_init(&cam_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    csi->is_initialized = true;
    ESP_LOGI(TAG, "CSI initialized successfully");

    return ESP_OK;
}

esp_err_t wt_bsp_csi_start(wt_bsp_csi_t csi, wt_bsp_csi_frame_cb_t frame_cb, void *user_data)
{
    if (csi == NULL || !csi->is_initialized || frame_cb == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (csi->is_streaming) {
        return ESP_OK;
    }

    int fd = open(WT_BSP_CSI_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s", WT_BSP_CSI_DEV_PATH);
        return ESP_FAIL;
    }
    csi->v4l2_fd = fd;

    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix.width = csi->info.width,
        .fmt.pix.height = csi->info.height,
        .fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565, // Default to RGB565 for display
    };

    if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "Failed to set format");
        goto err;
    }

    struct v4l2_requestbuffers req = {
        .count = csi->info.buffer_count,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "Failed to request buffers");
        goto err;
    }

    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = i,
        };

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "Failed to query buffer %d", i);
            goto err;
        }

        csi->buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (csi->buffers[i] == MAP_FAILED) {
            ESP_LOGE(TAG, "Failed to mmap buffer %d", i);
            goto err;
        }
        csi->buffer_size = buf.length;

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "Failed to queue buffer %d", i);
            goto err;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "Failed to start stream");
        goto err;
    }

    csi->frame_cb = frame_cb;
    csi->user_data = user_data;
    csi->is_streaming = true;

    BaseType_t res = xTaskCreate(wt_bsp_csi_stream_task, "csi_stream", CSI_STREAM_TASK_STACK_SIZE, csi, CSI_STREAM_TASK_PRIO, (TaskHandle_t *)&csi->video_stream_task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stream task");
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        goto err;
    }

    return ESP_OK;

err:
    close(fd);
    csi->v4l2_fd = -1;
    return ESP_FAIL;
}

esp_err_t wt_bsp_csi_stop(wt_bsp_csi_t csi)
{
    if (csi == NULL || !csi->is_streaming) {
        return ESP_OK;
    }

    csi->is_streaming = false;
    // Task will delete itself and call streamoff
    
    // Wait for task to finish (optional, could use semaphores for better sync)
    vTaskDelay(pdMS_TO_TICKS(100));

    if (csi->v4l2_fd >= 0) {
        close(csi->v4l2_fd);
        csi->v4l2_fd = -1;
    }

    return ESP_OK;
}

esp_err_t wt_bsp_csi_deinit(wt_bsp_csi_t csi)
{
    if (csi == NULL || !csi->is_initialized) {
        return ESP_OK;
    }

    wt_bsp_csi_stop(csi);
    csi->is_initialized = false;

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static void wt_bsp_csi_stream_task(void *arg)
{
    wt_bsp_csi_t csi = (wt_bsp_csi_t)arg;
    int fd = csi->v4l2_fd;

    while (csi->is_streaming) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == 0) {
            if (csi->frame_cb) {
                csi->frame_cb(csi->buffers[buf.index], csi->info.width, csi->info.height, buf.bytesused, csi->user_data);
            }
            ioctl(fd, VIDIOC_QBUF, &buf);
        } else {
            if (errno != EAGAIN) {
                ESP_LOGE(TAG, "VIDIOC_DQBUF failed: %d", errno);
                break;
            }
            vTaskDelay(1);
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    
    csi->video_stream_task_handle = NULL;
    vTaskDelete(NULL);
}

#endif // WT_BSP_CSI_ENABLE_IS_ENABLED
