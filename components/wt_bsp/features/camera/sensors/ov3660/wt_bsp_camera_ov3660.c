/**
 * @file wt_bsp_camera_ov3660.c
 * @author Wireless-Tag
 * @brief Reusable OV3660 native JPEG and image tuning controls.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_ov3660.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_OV3660

#include "wt_bsp_camera_sensor.h"
#include "../../backends/wt_bsp_camera_dvp.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "ov3660.h"
#include "esp_log.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"

/* ==================== [Defines] =========================================== */

#define WT_BSP_OV3660_PID                  0x3660
#define WT_BSP_OV3660_SATURATION_REG_BASE  0x5381
#define WT_BSP_OV3660_DETAIL_CTRL_REG      0x5308

/* ==================== [Typedefs] ========================================== */

/* Parsed from unified integer properties; never exposed to board code. */
typedef struct {
    int ae_level;
    int jpeg_quality;
    int saturation;
    int sharpness;
    int denoise;
} camera_ov3660_info_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t camera_ov3660_validate(const wt_bsp_camera_info_t *info, camera_ov3660_info_t *settings);
static esp_err_t camera_ov3660_configure(int capture_fd, const wt_bsp_camera_info_t *info, void *state);
static esp_err_t camera_ov3660_apply_settings(int capture_fd, const camera_ov3660_info_t *info);
static esp_err_t camera_ov3660_get_sensor_id(int fd, esp_cam_sensor_id_t *sensor_id);
static esp_err_t camera_ov3660_set_ae_level(int fd, const camera_ov3660_info_t *info);
static esp_err_t camera_ov3660_set_jpeg_quality(int fd, const camera_ov3660_info_t *info);
static esp_err_t camera_ov3660_register_access(int fd, uint32_t command, esp_cam_sensor_reg_val_t *reg_value);
static esp_err_t camera_ov3660_set_saturation(int fd, const camera_ov3660_info_t *info);
static esp_err_t camera_ov3660_set_detail_tuning(int fd, const camera_ov3660_info_t *info);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_camera_ov3660";

static const wt_bsp_camera_sensor_ops_t s_sensor_ops = {
    .name = "OV3660",
    .pid = WT_BSP_OV3660_PID,
    .pixel_format = V4L2_PIX_FMT_JPEG,
    .reuse_buffers = false,
    .prepare = NULL,
    .configure = camera_ov3660_configure,
    .release = NULL,
};

/*
 * OV3660 saturation matrices from esp_cam_sensor 2.3.0 for levels -2..+2.
 * The VGA format table starts at -2; level 0 restores the sensor's neutral
 * color matrix without patching the managed component.
 */
static const uint8_t s_ov3660_saturation_levels[5][11] = {
    {0x1d, 0x60, 0x03, 0x0a, 0x60, 0x6a, 0x64, 0x56, 0x0e, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0b, 0x6c, 0x77, 0x70, 0x60, 0x10, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0c, 0x78, 0x84, 0x7d, 0x6b, 0x12, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0d, 0x84, 0x91, 0x8a, 0x76, 0x14, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0e, 0x90, 0x9e, 0x96, 0x80, 0x16, 0x01, 0x98},
};

/* ==================== [Macros] ============================================ */

WT_BSP_CAMERA_DRIVER_REGISTER(ov3660, "ov3660", wt_bsp_camera_ov3660_init);

#if !CONFIG_CAMERA_OV3660_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(wt_bsp_camera_ov3660_detect, ESP_CAM_SENSOR_DVP, OV3660_SCCB_ADDR)
{
    if (!wt_bsp_camera_sensor_is_selected(&s_sensor_ops)) {
        return NULL;
    }
    esp_cam_sensor_config_t *sensor_config = config;
    sensor_config->sensor_port = ESP_CAM_SENSOR_DVP;
    return ov3660_detect(sensor_config);
}
#endif

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_camera_ov3660_init(wt_bsp_camera_t camera,
                                    const wt_bsp_camera_info_t *info,
                                    const wt_bsp_camera_ops_t **ops)
{
    if (camera == NULL || ops == NULL) {
        ESP_LOGE(TAG, "Invalid OV3660 initializer arguments");
        return ESP_ERR_INVALID_ARG;
    }
    camera_ov3660_info_t settings;
    esp_err_t ret = camera_ov3660_validate(info, &settings);
    if (ret != ESP_OK) {
        return ret;
    }
    return wt_bsp_camera_dvp_init(camera, &s_sensor_ops, ops);
}

/* ==================== [Static Functions] ================================== */

static esp_err_t camera_ov3660_validate(const wt_bsp_camera_info_t *info,
                                        camera_ov3660_info_t *settings)
{
    if (info == NULL || settings == NULL || info->sensor_name == NULL ||
            strcmp(info->sensor_name, "ov3660") != 0 ||
            info->properties == NULL || info->property_count != 5) {
        ESP_LOGE(TAG, "OV3660 requires exactly five named image properties");
        return ESP_ERR_INVALID_ARG;
    }
    const struct {
        const char *key;
        int *output;
        int minimum;
        int maximum;
    } fields[] = {
        {.key = "ae_level", .output = &settings->ae_level, .minimum = -5, .maximum = 5},
        {.key = "jpeg_quality", .output = &settings->jpeg_quality, .minimum = 1, .maximum = 40},
        {.key = "saturation", .output = &settings->saturation, .minimum = -2, .maximum = 2},
        {.key = "sharpness", .output = &settings->sharpness, .minimum = -3, .maximum = 3},
        {.key = "denoise", .output = &settings->denoise, .minimum = 0, .maximum = 8},
    };
    for (size_t i = 0; i < info->property_count; i++) {
        bool known = false;
        for (size_t j = 0; j < sizeof(fields) / sizeof(fields[0]); j++) {
            if (info->properties[i].key != NULL &&
                    strcmp(info->properties[i].key, fields[j].key) == 0) {
                known = true;
                break;
            }
        }
        if (!known) {
            ESP_LOGE(TAG, "Unknown OV3660 image property");
            return ESP_ERR_INVALID_ARG;
        }
    }
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        int32_t value;
        esp_err_t ret = wt_bsp_camera_info_get_property(info, fields[i].key, &value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Missing or duplicate OV3660 property: %s", fields[i].key);
            return ESP_ERR_INVALID_ARG;
        }
        if (value < fields[i].minimum || value > fields[i].maximum) {
            ESP_LOGE(TAG, "OV3660 property outside supported range: %s", fields[i].key);
            return ESP_ERR_INVALID_ARG;
        }
        *fields[i].output = value;
    }
    if (info->format.width != 640 || info->format.height != 480 ||
            info->format.fps != 25 || info->dvp.xclk_freq_hz != 20000000) {
        ESP_LOGE(TAG, "OV3660 profile requires JPEG 640x480@25fps with a 20 MHz XCLK");
        return ESP_ERR_NOT_SUPPORTED;
    }
#if CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE && !CONFIG_CAMERA_OV3660_AUTO_DETECT_DVP_INTERFACE_SENSOR
    ESP_LOGE(TAG, "Static sensor detection requires OV3660 DVP auto detection");
    return ESP_ERR_NOT_SUPPORTED;
#endif
    return ESP_OK;
}

static esp_err_t camera_ov3660_configure(int capture_fd, const wt_bsp_camera_info_t *info,
                                         void *state)
{
    (void)state;
    camera_ov3660_info_t settings;
    esp_err_t ret = camera_ov3660_validate(info, &settings);
    if (ret != ESP_OK) {
        return ret;
    }
    return camera_ov3660_apply_settings(capture_fd, &settings);
}

static esp_err_t camera_ov3660_apply_settings(int capture_fd,
                                              const camera_ov3660_info_t *info)
{
    esp_err_t result = ESP_OK;
    esp_cam_sensor_id_t sensor_id = {0};

    if (capture_fd < 0 || info == NULL || info->ae_level < -5 || info->ae_level > 5 ||
            info->jpeg_quality < 1 || info->jpeg_quality > 40 ||
            info->saturation < -2 || info->saturation > 2 ||
            info->sharpness < -3 || info->sharpness > 3 ||
            info->denoise < 0 || info->denoise > 8) {
        ESP_LOGE(TAG, "Invalid OV3660 image settings");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = camera_ov3660_get_sensor_id(capture_fd, &sensor_id);
    if (ret != ESP_OK) {
        return ret;
    }
    if (sensor_id.pid != WT_BSP_OV3660_PID) {
        ESP_LOGE(TAG, "Refusing OV3660 tuning for sensor PID=0x%x", (unsigned int)sensor_id.pid);
        return ESP_ERR_INVALID_STATE;
    }

    const struct {
        const char *name;
        esp_err_t (*apply)(int fd, const camera_ov3660_info_t *settings);
    } controls[] = {
        {.name = "auto exposure", .apply = camera_ov3660_set_ae_level},
        {.name = "native JPEG quality", .apply = camera_ov3660_set_jpeg_quality},
        {.name = "saturation", .apply = camera_ov3660_set_saturation},
        {.name = "sharpness and denoise", .apply = camera_ov3660_set_detail_tuning},
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); i++) {
        ret = controls[i].apply(capture_fd, info);
        if (ret != ESP_OK) {
            // Independent image controls remain useful after another control fails.
            ESP_LOGW(TAG, "Continuing after %s tuning failed: %s", controls[i].name, esp_err_to_name(ret));
            if (result == ESP_OK) {
                result = ret;
            }
        }
    }
    return result;
}

static esp_err_t camera_ov3660_get_sensor_id(int fd, esp_cam_sensor_id_t *sensor_id)
{
    struct v4l2_ext_control control = {
        .id = ESP_CAM_SENSOR_IOC_G_CHIP_ID,
        .p_u8 = (uint8_t *)sensor_id,
        .size = sizeof(*sensor_id),
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL,
        .count = 1,
        .controls = &control,
    };

    if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "Could not read camera sensor ID (errno=%d)", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t camera_ov3660_set_ae_level(int fd, const camera_ov3660_info_t *info)
{
    struct v4l2_query_ext_ctrl query = {
        .id = V4L2_CID_CAMERA_AE_LEVEL,
    };
    struct v4l2_ext_control control = {
        .id = V4L2_CID_CAMERA_AE_LEVEL,
        .value = info->ae_level,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_CAMERA_CLASS,
        .count = 1,
        .controls = &control,
    };

    if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) != 0) {
        ESP_LOGW(TAG, "Sensor does not expose an AE target-level control (errno=%d)", errno);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (info->ae_level < query.minimum ||
            info->ae_level > query.maximum) {
        ESP_LOGE(TAG,
                 "Configured AE target level %d is outside sensor range %d..%d",
                 info->ae_level,
                 (int)query.minimum,
                 (int)query.maximum);
        return ESP_ERR_INVALID_ARG;
    }
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG,
                 "Could not set sensor AE target level to %d (errno=%d)",
                 info->ae_level,
                 errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "OV3660 auto-exposure target level=%d (sensor range %d..%d)",
             info->ae_level,
             (int)query.minimum,
             (int)query.maximum);
    return ESP_OK;
}

static esp_err_t camera_ov3660_set_jpeg_quality(int fd, const camera_ov3660_info_t *info)
{
    struct v4l2_query_ext_ctrl query = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
    };
    struct v4l2_ext_control control = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
        .value = info->jpeg_quality,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_JPEG_CLASS,
        .count = 1,
        .controls = &control,
    };

    if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) != 0) {
        ESP_LOGW(TAG, "Sensor does not expose JPEG quality control (errno=%d)", errno);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (info->jpeg_quality < query.minimum ||
            info->jpeg_quality > query.maximum) {
        ESP_LOGE(TAG,
                 "Configured sensor JPEG quality %d is outside range %d..%d",
                 info->jpeg_quality,
                 (int)query.minimum,
                 (int)query.maximum);
        return ESP_ERR_INVALID_ARG;
    }
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG,
                 "Could not set OV3660 JPEG quality to %d (errno=%d)",
                 info->jpeg_quality,
                 errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "OV3660 native JPEG quality=%d (sensor range %d..%d)",
             info->jpeg_quality,
             (int)query.minimum,
             (int)query.maximum);
    return ESP_OK;
}

static esp_err_t camera_ov3660_register_access(int fd,
                                               uint32_t command,
                                               esp_cam_sensor_reg_val_t *reg_value)
{
    struct v4l2_ext_control control = {
        .id = command,
        .p_u8 = (uint8_t *)reg_value,
        .size = sizeof(*reg_value),
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL,
        .count = 1,
        .controls = &control,
    };
    int request;

    if (command == ESP_CAM_SENSOR_IOC_G_REG) {
        request = VIDIOC_G_EXT_CTRLS;
    } else if (command == ESP_CAM_SENSOR_IOC_S_REG) {
        request = VIDIOC_S_EXT_CTRLS;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ioctl(fd, request, &controls) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t camera_ov3660_set_saturation(int fd, const camera_ov3660_info_t *info)
{
    esp_cam_sensor_id_t sensor_id = {0};

    if (camera_ov3660_get_sensor_id(fd, &sensor_id) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read sensor ID before color tuning (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (sensor_id.pid != WT_BSP_OV3660_PID) {
        ESP_LOGW(TAG,
                 "Skipping OV3660 color tuning for sensor PID=0x%x",
                 (unsigned int)sensor_id.pid);
        return ESP_ERR_NOT_SUPPORTED;
    }

    const uint8_t *values =
        s_ov3660_saturation_levels[info->saturation + 2];
    for (size_t i = 0; i < sizeof(s_ov3660_saturation_levels[0]); i++) {
        esp_cam_sensor_reg_val_t reg_value = {
            .regaddr = WT_BSP_OV3660_SATURATION_REG_BASE + i,
            .value = values[i],
        };
        if (camera_ov3660_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &reg_value) != ESP_OK) {
            ESP_LOGW(TAG,
                     "Could not set OV3660 saturation register 0x%x (errno=%d)",
                     (unsigned int)reg_value.regaddr,
                     errno);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "OV3660 saturation level=%d", info->saturation);
    return ESP_OK;
}

static esp_err_t camera_ov3660_set_detail_tuning(int fd, const camera_ov3660_info_t *info)
{
    esp_cam_sensor_id_t sensor_id = {0};
    esp_cam_sensor_reg_val_t detail_ctrl = {
        .regaddr = WT_BSP_OV3660_DETAIL_CTRL_REG,
    };

    if (camera_ov3660_get_sensor_id(fd, &sensor_id) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read sensor ID before detail tuning (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (sensor_id.pid != WT_BSP_OV3660_PID) {
        ESP_LOGW(TAG,
                 "Skipping OV3660 detail tuning for sensor PID=0x%x",
                 (unsigned int)sensor_id.pid);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (camera_ov3660_register_access(fd, ESP_CAM_SENSOR_IOC_G_REG, &detail_ctrl) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read OV3660 detail-control register (errno=%d)", errno);
        return ESP_FAIL;
    }

    const uint8_t original_detail_ctrl = detail_ctrl.value;
    const uint8_t sharpness_offset_2 = (info->sharpness + 3) * 8;
    const struct {
        uint32_t regaddr;
        uint8_t value;
    } sharpness_registers[] = {
        {0x5300, 0x10},
        {0x5301, 0x10},
        {0x5302, sharpness_offset_2 + 1},
        {0x5303, sharpness_offset_2},
        {0x5309, 0x10},
        {0x530a, 0x10},
        {0x530b, 0x04},
        {0x530c, 0x06},
    };

    /* Bit 6 selects automatic sharpness; clear it for the configured level. */
    detail_ctrl.value &= ~0x40;
    if (info->denoise > 0) {
        detail_ctrl.value |= 0x10;
    } else {
        detail_ctrl.value &= ~0x10;
    }
    if (camera_ov3660_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &detail_ctrl) != ESP_OK) {
        ESP_LOGW(TAG, "Could not set OV3660 detail-control register (errno=%d)", errno);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < sizeof(sharpness_registers) / sizeof(sharpness_registers[0]); i++) {
        esp_cam_sensor_reg_val_t reg_value = {
            .regaddr = sharpness_registers[i].regaddr,
            .value = sharpness_registers[i].value,
        };
        if (camera_ov3660_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &reg_value) != ESP_OK) {
            ESP_LOGW(TAG,
                     "Could not set OV3660 sharpness register 0x%x (errno=%d)",
                     (unsigned int)reg_value.regaddr,
                     errno);
            return ESP_FAIL;
        }
    }

    if (info->denoise > 0) {
        esp_cam_sensor_reg_val_t denoise_offset = {
            .regaddr = 0x5306,
            .value = (info->denoise - 1) * 4,
        };
        if (camera_ov3660_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &denoise_offset) != ESP_OK) {
            ESP_LOGW(TAG, "Could not set OV3660 denoise level (errno=%d)", errno);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG,
             "OV3660 detail tuning: sharpness=%d, denoise=%d (control 0x%02x->0x%02x)",
             info->sharpness,
             info->denoise,
             (unsigned int)original_detail_ctrl,
             (unsigned int)detail_ctrl.value);
    return ESP_OK;
}

#endif // WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_OV3660
