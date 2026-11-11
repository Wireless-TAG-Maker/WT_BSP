/**
 * @file wt_bsp_touch_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP touch port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_TOUCH_PORT_H__
#define __WT_BSP_TOUCH_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_touch.h"

#if WT_BSP_TOUCH_ENABLED

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 触摸控制器型号。
 */
typedef enum {
    WT_BSP_TOUCH_MODEL_AUTO = 0, /*!< 使用当前显示适配配置的触摸型号。 */
    WT_BSP_TOUCH_MODEL_ST7123,   /*!< 原板载 ST7123 触摸控制器。 */
    WT_BSP_TOUCH_MODEL_GT911,    /*!< GT911 触摸控制器, 保留工厂配置。 */
} wt_bsp_touch_model_t;

/**
 * @brief 触摸硬件配置。
 */
typedef struct {
    void *i2c_bus_handle;   /*!< 外部提供的 I2C 总线句柄 (i2c_master_bus_handle_t)。如果非 NULL，则不内部初始化 I2C。 */
    int i2c_port;           /*!< I2C 端口号。 */
    int scl_pin;            /*!< I2C SCL 引脚。 */
    int sda_pin;            /*!< I2C SDA 引脚。 */
    int rst_pin;            /*!< 复位引脚，未使用设为 -1。 */
    int int_pin;            /*!< 中断引脚，未使用设为 -1。 */
    uint16_t width;         /*!< 触摸屏宽度。 */
    uint16_t height;        /*!< 触摸屏高度。 */
    wt_bsp_touch_model_t model; /*!< 控制器型号, AUTO 沿用当前显示适配配置。 */
    bool swap_xy;          /*!< 交换 X/Y 坐标。 */
    bool mirror_x;         /*!< 镜像 X 坐标。 */
    bool mirror_y;         /*!< 镜像 Y 坐标。 */
} wt_bsp_touch_info_t;

/**
 * @brief 触摸对象。
 */
typedef struct wt_bsp_touch_obj_t {
    wt_bsp_touch_info_t info;              /*!< 已解析的触摸配置。 */
    esp_lcd_touch_handle_t handle;         /*!< 对象持有的触摸控制器句柄。 */
    esp_lcd_panel_io_handle_t io;          /*!< 对象创建并持有的 I2C panel IO。 */
    i2c_master_bus_handle_t i2c_bus;        /*!< 自建或借用的 I2C 总线。 */
    bool owns_i2c_bus;                     /*!< 是否由本对象创建总线。 */
    void *driver_data;                    /*!< 对象持有的控制器私有配置与坐标上下文。 */
    lv_indev_t *lvgl_indev;                /*!< 本对象注册并持有的 LVGL 输入设备。 */
    bool is_initialized;                  /*!< 是否完成初始化。 */
} wt_bsp_touch_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 获取当前显示适配配置的触摸默认参数。
 *
 * @param[out] info 默认控制器、分辨率和坐标方向。总线为空, 引脚为 -1, 调用方须填写板级 IO。
 * @return ESP_OK 或 info 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 */
esp_err_t wt_bsp_touch_get_default_info(wt_bsp_touch_info_t *info);

/**
 * @brief 探测当前显示适配配置对应的触摸设备。
 *
 * @param[in] bus 板级持有的共享 I2C 总线, 本函数不转移或释放所有权。
 * @param[in] legacy_address 原触摸设备地址; GT911 配置使用 0x5D 和 0x14。
 * @return ESP_OK, ESP_ERR_NOT_FOUND, ESP_ERR_INVALID_ARG 或底层 I2C 错误。
 */
esp_err_t wt_bsp_touch_probe(i2c_master_bus_handle_t bus, uint8_t legacy_address);

/**
 * @brief 初始化触摸对象。
 *
 * @param[in,out] touch 待初始化的触摸对象。
 * @param[in] info 硬件配置。
 * 对象须由零初始化的板级静态存储持有, 生命周期调用须串行。
 * 共享总线只借用; panel IO、控制器、私有上下文和本对象注册的 LVGL 输入设备由本对象管理。
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_ERR_NOT_FOUND,
 *         ESP_ERR_NOT_SUPPORTED, ESP_ERR_NO_MEM 或底层驱动错误。
 */
esp_err_t wt_bsp_touch_init(wt_bsp_touch_t touch, const wt_bsp_touch_info_t *info);

/**
 * @brief 反初始化触摸对象。
 *
 * @param[in,out] touch 待反初始化的触摸对象。
 * 先停止 LVGL 输入消费者, 再释放控制器、IO 和自建总线; 借用的总线不释放。
 * 支持部分初始化失败后的清理, 失败时保留未释放资源供重试。
 * @return ESP_OK, ESP_ERR_INVALID_ARG 或底层清理错误。
 */
esp_err_t wt_bsp_touch_deinit(wt_bsp_touch_t touch);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_TOUCH_ENABLED

#endif // __WT_BSP_TOUCH_PORT_H__
