/**
 * @file board.c
 * @author YobieZhou
 * @brief
 * @version 0.1
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "board.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#if WT_BSP_SDMMC_ENABLED
#include "driver/spi_master.h"
#endif
#if WT_BSP_DSI_ENABLED || WT_BSP_CSI_ENABLED || WT_BSP_TOUCH_ENABLED
#include "driver/i2c_master.h"
#endif

/* ==================== [Defines] =========================================== */

#define BOARD_VERSION_MAJOR 0
#define BOARD_VERSION_MINOR 1

#define BOARD_I2C_FEATURE_ENABLED (WT_BSP_DSI_ENABLED || WT_BSP_CSI_ENABLED || WT_BSP_TOUCH_ENABLED)

#define BOARD_NAME "WT9932P4C61-TINY"

#define BOARD_BUTTON_GPIO_NUM 35
#define BOARD_BUTTON_ACTIVE_LEVEL WT_BSP_BUTTON_ACTIVE_LOW

#define BOARD_RGB_GPIO_NUM 51
#define BOARD_RGB_MODEL WT_BSP_RGB_MODEL_WS2812
#define BOARD_RGB_FORMAT WT_BSP_RGB_FORMAT_GRB
#define BOARD_RGB_INVERT_OUT false

#define BOARD_SDMMC_MOUNT_POINT "/sdcard"
#define BOARD_SDMMC_SLOT 0
#define BOARD_SDMMC_WIDTH 4
#define BOARD_SDMMC_CD_GPIO -1
#define BOARD_SDMMC_WP_GPIO -1
#define BOARD_SDMMC_CLK_GPIO 43
#define BOARD_SDMMC_CMD_GPIO 44
#define BOARD_SDMMC_D0_GPIO 39
#define BOARD_SDMMC_D1_GPIO 40
#define BOARD_SDMMC_D2_GPIO 41
#define BOARD_SDMMC_D3_GPIO 42
#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE) && CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE
#define BOARD_SDMMC_USE_SDSPI 1
#else
#define BOARD_SDMMC_USE_SDSPI 0
#endif
#define BOARD_SDMMC_SPI_HOST SPI2_HOST

#define BOARD_SDMMC_USE_ON_CHIP_LDO 1
#define BOARD_SDMMC_LDO_CHAN_ID 4
#define BOARD_SDMMC_LDO_VOLTAGE_MV 3300

#define BOARD_DSI_BACKLIGHT_GPIO_NUM 26
#define BOARD_DSI_RESET_GPIO_NUM 27
#define BOARD_DSI_PHY_LDO_CHANNEL 3
#define BOARD_DSI_PHY_LDO_VOLTAGE_MV 2500
#define BOARD_DSI_LEDC_CHANNEL 1
#define BOARD_DSI_LEDC_TIMER 1

#define BOARD_CSI_SCCB_SCL_PIN 8
#define BOARD_CSI_SCCB_SDA_PIN 7
#define BOARD_CSI_RESET_PIN -1
#define BOARD_CSI_PWDN_PIN -1
#define BOARD_CSI_WIDTH 1024
#define BOARD_CSI_HEIGHT 600
#define BOARD_CSI_FPS 30
#define BOARD_CSI_BUF_COUNT 3

#define BOARD_I2C_SCAN_TIMEOUT_MS 200

/* I2C 设备地址定义 */
#define BOARD_I2C_ADDR_DISPLAY  0x28
#define BOARD_I2C_ADDR_TOUCH    0x55
#define BOARD_I2C_ADDR_CAMERA   0x30

#define BOARD_TOUCH_I2C_PORT 0
#define BOARD_TOUCH_I2C_SDA_PIN 7
#define BOARD_TOUCH_I2C_SCL_PIN 8
#define BOARD_TOUCH_RST_PIN -1
#define BOARD_TOUCH_INT_PIN -1

/* ==================== [Typedefs] ========================================== */

/**
 * @brief I2C 设备检测状态
 */
#if BOARD_I2C_FEATURE_ENABLED
typedef struct {
    bool display_available;
    bool touch_detected;
    bool camera_detected;
} board_i2c_device_status_t;
#endif

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_init(void);
static esp_err_t board_deinit(void);
static wt_bsp_board_t board_get_board(void);
static wt_bsp_button_t board_get_button(void);
static wt_bsp_rgb_t board_get_rgb(void);
static wt_bsp_sdmmc_t board_get_sdmmc(void);
static wt_bsp_dsi_t board_get_dsi(void);
static wt_bsp_csi_t board_get_csi(void);
static wt_bsp_touch_t board_get_touch(void);
static wt_bsp_usb_device_uvc_t board_get_usb_device_uvc(void);
static wt_bsp_usb_device_cdc_t board_get_usb_device_cdc(void);
#if BOARD_I2C_FEATURE_ENABLED
static esp_err_t board_i2c_scan_devices(i2c_master_bus_handle_t bus_handle, board_i2c_device_status_t *status);
#endif

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board";

static bool s_board_is_init = false;
#if BOARD_I2C_FEATURE_ENABLED
static i2c_master_bus_handle_t s_shared_i2c_bus = NULL;
#endif

static wt_bsp_interface_t s_bsp_interface = {
    .init = board_init,
    .deinit = board_deinit,
    .get_board = board_get_board,
    .get_button = board_get_button,
    .get_rgb = board_get_rgb,
    .get_sdmmc = board_get_sdmmc,
    .get_dsi = board_get_dsi,
    .get_csi = board_get_csi,
    .get_touch = board_get_touch,
    .get_usb_device_uvc = board_get_usb_device_uvc,
    .get_usb_device_cdc = board_get_usb_device_cdc,
    .get_camera = NULL,
};

static wt_bsp_board_obj_t s_bsp_board = {0};
#if WT_BSP_BUTTON_ENABLED
static wt_bsp_button_obj_t s_bsp_button = {0};
#endif
#if WT_BSP_RGB_ENABLED
static wt_bsp_rgb_obj_t s_bsp_rgb = {0};
#endif
#if WT_BSP_SDMMC_ENABLED
static wt_bsp_sdmmc_obj_t s_bsp_sdmmc = {0};
#endif
#if WT_BSP_DSI_ENABLED
static wt_bsp_dsi_obj_t s_bsp_dsi = {0};
#endif
#if WT_BSP_CSI_ENABLED
static wt_bsp_csi_obj_t s_bsp_csi = {0};
#endif
#if WT_BSP_TOUCH_ENABLED
static wt_bsp_touch_obj_t s_bsp_touch = {0};
#endif
#if WT_BSP_USB_DEVICE_UVC_ENABLED
static wt_bsp_usb_device_uvc_obj_t s_bsp_usb_device_uvc = {0};
#endif
#if WT_BSP_USB_DEVICE_CDC_ENABLED
static wt_bsp_usb_device_cdc_obj_t s_bsp_usb_device_cdc = {0};
#endif

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

wt_bsp_interface_t *board_get_bsp_interface(void)
{
    return &s_bsp_interface;
}

/* ==================== [Static Functions] ================================== */


/**
 * @brief 检查所选显示配置及 I2C 外设是否可初始化。
 * @param bus_handle I2C 总线句柄
 * @param status 输出检测到的设备状态
 * @return ESP_OK 或 ESP_ERR_NOT_FOUND（没有可初始化的外设）。
 */
#if BOARD_I2C_FEATURE_ENABLED
static esp_err_t board_i2c_scan_devices(i2c_master_bus_handle_t bus_handle, board_i2c_device_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(board_i2c_device_status_t));
    bool any_device_available = false;

    ESP_LOGI(TAG, "Scanning I2C bus for devices...");

    esp_err_t ret;
#if WT_BSP_DSI_ENABLED
    ret = wt_bsp_dsi_probe(bus_handle, BOARD_I2C_ADDR_DISPLAY);
    if (ret == ESP_OK) {
        status->display_available = true;
        any_device_available = true;
        ESP_LOGI(TAG, "DSI initialization is available for the selected configuration");
    } else {
        ESP_LOGW(TAG, "DSI initialization is unavailable: %s", esp_err_to_name(ret));
    }
#endif

#if WT_BSP_TOUCH_ENABLED
    ret = wt_bsp_touch_probe(bus_handle, BOARD_I2C_ADDR_TOUCH);
    if (ret == ESP_OK) {
        status->touch_detected = true;
        any_device_available = true;
        ESP_LOGI(TAG, "Touch device is available");
    } else {
        ESP_LOGW(TAG, "Touch device not found: %s", esp_err_to_name(ret));
    }
#endif

    // 检测摄像头 (0x30)
    ret = i2c_master_probe(bus_handle, BOARD_I2C_ADDR_CAMERA, BOARD_I2C_SCAN_TIMEOUT_MS);
    if (ret == ESP_OK) {
        status->camera_detected = true;
        any_device_available = true;
        ESP_LOGI(TAG, "Found camera device at address: 0x%02x", BOARD_I2C_ADDR_CAMERA);
    } else {
        ESP_LOGW(TAG, "Camera device not found at address: 0x%02x", BOARD_I2C_ADDR_CAMERA);
    }

    ESP_LOGI(TAG, "Peripheral availability. DSI: %s, Touch: %s, Camera: %s",
             status->display_available ? "YES" : "NO",
             status->touch_detected ? "YES" : "NO",
             status->camera_detected ? "YES" : "NO");

    return any_device_available ? ESP_OK : ESP_ERR_NOT_FOUND;
}
#endif

static esp_err_t board_init(void)
{
    esp_err_t ret = ESP_OK;

    if (s_board_is_init) {
        ESP_LOGW(TAG, "Board is already initialized");
        return ESP_OK;
    }

    // Initialize board
    wt_bsp_board_info_t board_info = {
        .name = BOARD_NAME,
        .version_major = BOARD_VERSION_MAJOR,
        .version_minor = BOARD_VERSION_MINOR,
    };

    ret = wt_bsp_board_init(&s_bsp_board, &board_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize board: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize button
#if WT_BSP_BUTTON_ENABLED
    ret = wt_bsp_button_init(&s_bsp_button, &(wt_bsp_button_info_t) {
        .gpio_num = BOARD_BUTTON_GPIO_NUM,
        .active_level = BOARD_BUTTON_ACTIVE_LEVEL,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    // Initialize RGB
#if WT_BSP_RGB_ENABLED
    ret = wt_bsp_rgb_init(&s_bsp_rgb, &(wt_bsp_rgb_info_t) {
        .gpio_num = BOARD_RGB_GPIO_NUM,
        .model = BOARD_RGB_MODEL,
        .format = BOARD_RGB_FORMAT,
        .invert_out = BOARD_RGB_INVERT_OUT,
    });
    if (ret != ESP_OK) {
#if WT_BSP_BUTTON_ENABLED
        wt_bsp_button_deinit(&s_bsp_button);
#endif
        ESP_LOGE(TAG, "Failed to initialize RGB: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

#if WT_BSP_SDMMC_ENABLED
    // Initialize SDMMC
    ret = wt_bsp_sdmmc_init(&s_bsp_sdmmc, &(wt_bsp_sdmmc_info_t) {
        .mount_point = BOARD_SDMMC_MOUNT_POINT,
        .slot = BOARD_SDMMC_SLOT,
        .width = BOARD_SDMMC_WIDTH,
        .cd_gpio = BOARD_SDMMC_CD_GPIO,
        .wp_gpio = BOARD_SDMMC_WP_GPIO,
        .clk_gpio = BOARD_SDMMC_CLK_GPIO,
        .cmd_gpio = BOARD_SDMMC_CMD_GPIO,
        .d0_gpio = BOARD_SDMMC_D0_GPIO,
        .d1_gpio = BOARD_SDMMC_D1_GPIO,
        .d2_gpio = BOARD_SDMMC_D2_GPIO,
        .d3_gpio = BOARD_SDMMC_D3_GPIO,
        .use_sdspi = BOARD_SDMMC_USE_SDSPI,
        .spi_host = BOARD_SDMMC_SPI_HOST,
        .use_on_chip_ldo = 0,  // WT9932P4C61-TINY does not use VO4
        .ldo_chan_id = -1,
        .ldo_voltage_mv = BOARD_SDMMC_LDO_VOLTAGE_MV,
    });
    if (ret != ESP_OK) {
#if WT_BSP_RGB_ENABLED
        wt_bsp_rgb_deinit(&s_bsp_rgb);
#endif
#if WT_BSP_BUTTON_ENABLED
        wt_bsp_button_deinit(&s_bsp_button);
#endif
        ESP_LOGE(TAG, "Failed to initialize SDMMC: %s", esp_err_to_name(ret));
        return ret;
    }

#endif
#if BOARD_I2C_FEATURE_ENABLED
    // 硬件限制：IO0 连接到了摄像头的 PWDN/LDO/RESET 引脚。
    // 正常使用摄像头前，必须将 IO0 拉高以启用摄像头供电及解除复位。
    // 在 I2C 扫描前需要先开启摄像头电源，确保摄像头能被检测到。
    gpio_config_t csi_pwr_conf = {
        .pin_bit_mask = (1ULL << 0),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&csi_pwr_conf);
    gpio_set_level(0, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create shared I2C bus for Touch and CSI
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = BOARD_TOUCH_I2C_PORT,
        .sda_io_num = BOARD_TOUCH_I2C_SDA_PIN,
        .scl_io_num = BOARD_TOUCH_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };
    ret = i2c_new_master_bus(&i2c_config, &s_shared_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize shared I2C bus: %s", esp_err_to_name(ret));
        s_shared_i2c_bus = NULL;
#if WT_BSP_SDMMC_ENABLED
        wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
#endif
#if WT_BSP_RGB_ENABLED
        wt_bsp_rgb_deinit(&s_bsp_rgb);
#endif
#if WT_BSP_BUTTON_ENABLED
        wt_bsp_button_deinit(&s_bsp_button);
#endif
        return ESP_ERR_NOT_FOUND;
    }

    // Scan I2C bus for expected devices
    board_i2c_device_status_t device_status = {0};
    ret = board_i2c_scan_devices(s_shared_i2c_bus, &device_status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device detection failed: no devices found");
        i2c_del_master_bus(s_shared_i2c_bus);
        s_shared_i2c_bus = NULL;
        // Keep RGB, Button, and SDMMC for status indication
        s_board_is_init = true;  // Mark as init to allow deinit later
        return ESP_ERR_NOT_FOUND;
    }

#if WT_BSP_DSI_ENABLED
    // The feature owns panel selection and protocol-specific configuration.
    if (device_status.display_available) {
        wt_bsp_dsi_info_t dsi_info;
        ret = wt_bsp_dsi_get_default_info(&dsi_info);
        if (ret == ESP_OK) {
            dsi_info.backlight_gpio_num = BOARD_DSI_BACKLIGHT_GPIO_NUM;
            dsi_info.reset_gpio_num = BOARD_DSI_RESET_GPIO_NUM;
            dsi_info.phy_ldo_channel = BOARD_DSI_PHY_LDO_CHANNEL;
            dsi_info.phy_ldo_voltage_mv = BOARD_DSI_PHY_LDO_VOLTAGE_MV;
            dsi_info.ledc_channel = BOARD_DSI_LEDC_CHANNEL;
            dsi_info.ledc_timer = BOARD_DSI_LEDC_TIMER;
            ret = wt_bsp_dsi_init(&s_bsp_dsi, &dsi_info);
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "DSI initialization failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "DSI initialized successfully");
        }
    } else {
        ESP_LOGW(TAG, "DSI configuration unavailable, skipping initialization");
    }

#endif
#if WT_BSP_CSI_ENABLED
    // Initialize CSI only if camera device was detected
    if (device_status.camera_detected) {
        ret = wt_bsp_csi_init(&s_bsp_csi, &(wt_bsp_csi_info_t) {
            .i2c_bus_handle = s_shared_i2c_bus,
            .sccb_scl_pin = BOARD_CSI_SCCB_SCL_PIN,
            .sccb_sda_pin = BOARD_CSI_SCCB_SDA_PIN,
            .reset_pin = BOARD_CSI_RESET_PIN,
            .pwdn_pin = BOARD_CSI_PWDN_PIN,
            .width = BOARD_CSI_WIDTH,
            .height = BOARD_CSI_HEIGHT,
            .fps = BOARD_CSI_FPS,
            .buffer_count = BOARD_CSI_BUF_COUNT,
        });
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CSI initialization failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "CSI initialized successfully");
        }
    } else {
        ESP_LOGW(TAG, "Camera device not detected, skipping CSI initialization");
    }

#endif
#if WT_BSP_TOUCH_ENABLED
    // Initialize touch only if touch device was detected
    if (device_status.touch_detected) {
        wt_bsp_touch_info_t touch_info;
        ret = wt_bsp_touch_get_default_info(&touch_info);
        if (ret == ESP_OK) {
            touch_info.i2c_bus_handle = s_shared_i2c_bus;
            touch_info.i2c_port = BOARD_TOUCH_I2C_PORT;
            touch_info.scl_pin = BOARD_TOUCH_I2C_SCL_PIN;
            touch_info.sda_pin = BOARD_TOUCH_I2C_SDA_PIN;
            touch_info.rst_pin = BOARD_TOUCH_RST_PIN;
            touch_info.int_pin = BOARD_TOUCH_INT_PIN;
            ret = wt_bsp_touch_init(&s_bsp_touch, &touch_info);
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch initialization failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Touch initialized successfully");
        }
    } else {
        ESP_LOGW(TAG, "Touch device not detected, skipping touch initialization");
    }

#endif
#endif
#if WT_BSP_USB_DEVICE_UVC_ENABLED
    // Prepare the USB endpoint object; the application supplies and starts its stream.
    ret = wt_bsp_usb_device_uvc_init(&s_bsp_usb_device_uvc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Device UVC object initialization failed: %s", esp_err_to_name(ret));
        s_board_is_init = true;
        board_deinit();
        return ret;
    }
#endif
#if WT_BSP_USB_DEVICE_CDC_ENABLED
    ret = wt_bsp_usb_device_cdc_init(&s_bsp_usb_device_cdc, &(wt_bsp_usb_device_cdc_info_t) {
        .port = 0,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Device CDC initialization failed: %s", esp_err_to_name(ret));
        s_board_is_init = true;
        board_deinit();
        return ret;
    }
#endif
    s_board_is_init = true;

    return ESP_OK;
}

static esp_err_t board_deinit(void)
{
    esp_err_t ret = ESP_OK;
    if (!s_board_is_init) {
        ESP_LOGW(TAG, "Board is not initialized");
        return ESP_OK;
    }

#if WT_BSP_USB_DEVICE_CDC_ENABLED
    ret = wt_bsp_usb_device_cdc_deinit(&s_bsp_usb_device_cdc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize USB Device CDC: %s", esp_err_to_name(ret));
    }
#endif

#if WT_BSP_USB_DEVICE_UVC_ENABLED
    ret = wt_bsp_usb_device_uvc_deinit(&s_bsp_usb_device_uvc);
    if (ret != ESP_OK) {
        // Keep application frame sources valid until USB callbacks have stopped.
        ESP_LOGE(TAG, "Failed to deinitialize USB Device UVC: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

#if WT_BSP_TOUCH_ENABLED
    // Deinitialize touch
    ret = wt_bsp_touch_deinit(&s_bsp_touch);
    if (ret != ESP_OK) {
        // Keep the object and shared resources available for a cleanup retry.
        ESP_LOGE(TAG, "Failed to deinitialize touch: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

#if WT_BSP_CSI_ENABLED
    // Deinitialize CSI
    ret = wt_bsp_csi_deinit(&s_bsp_csi);
    if (ret != ESP_OK) {
        // A live capture task still owns the camera and its shared SCCB bus.
        ESP_LOGE(TAG, "Failed to deinitialize CSI: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

#if WT_BSP_DSI_ENABLED
    // Deinitialize DSI
    ret = wt_bsp_dsi_deinit(&s_bsp_dsi);
    if (ret != ESP_OK) {
        // Keep the object and shared resources available for a cleanup retry.
        ESP_LOGE(TAG, "Failed to deinitialize DSI: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

#if BOARD_I2C_FEATURE_ENABLED
    // Delete I2C bus
    if (s_shared_i2c_bus != NULL) {
        ret = i2c_del_master_bus(s_shared_i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(ret));
            return ret;
        }
        s_shared_i2c_bus = NULL;
    }
#endif

#if WT_BSP_SDMMC_ENABLED
    // Deinitialize SDMMC
    ret = wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize SDMMC: %s", esp_err_to_name(ret));
    }
#endif

    // Deinitialize RGB
#if WT_BSP_RGB_ENABLED
    ret = wt_bsp_rgb_deinit(&s_bsp_rgb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize RGB: %s", esp_err_to_name(ret));
    }
#endif

    // Deinitialize button

#if WT_BSP_BUTTON_ENABLED
    ret = wt_bsp_button_deinit(&s_bsp_button);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize button: %s", esp_err_to_name(ret));
    }
#endif

    s_board_is_init = false;

    return ESP_OK;
}

static wt_bsp_board_t board_get_board(void)
{
    return &s_bsp_board;
}

static wt_bsp_button_t board_get_button(void)
{
#if WT_BSP_BUTTON_ENABLED
    return &s_bsp_button;
#else
    return NULL;
#endif
}

static wt_bsp_rgb_t board_get_rgb(void)
{
#if WT_BSP_RGB_ENABLED
    return &s_bsp_rgb;
#else
    return NULL;
#endif
}

static wt_bsp_sdmmc_t board_get_sdmmc(void)
{
#if WT_BSP_SDMMC_ENABLED
    return s_bsp_sdmmc.is_initialized ? &s_bsp_sdmmc : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_dsi_t board_get_dsi(void)
{
#if WT_BSP_DSI_ENABLED
    return s_bsp_dsi.is_initialized ? &s_bsp_dsi : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_csi_t board_get_csi(void)
{
#if WT_BSP_CSI_ENABLED
    return s_bsp_csi.is_initialized ? &s_bsp_csi : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_touch_t board_get_touch(void)
{
#if WT_BSP_TOUCH_ENABLED
    return s_bsp_touch.is_initialized ? &s_bsp_touch : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_usb_device_uvc_t board_get_usb_device_uvc(void)
{
#if WT_BSP_USB_DEVICE_UVC_ENABLED
    return s_board_is_init && s_bsp_usb_device_uvc.is_initialized ? &s_bsp_usb_device_uvc : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_usb_device_cdc_t board_get_usb_device_cdc(void)
{
#if WT_BSP_USB_DEVICE_CDC_ENABLED
    return s_board_is_init && s_bsp_usb_device_cdc.is_initialized ? &s_bsp_usb_device_cdc : NULL;
#else
    return NULL;
#endif
}
