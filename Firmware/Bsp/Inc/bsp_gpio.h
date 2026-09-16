/**
 * @file    bsp_gpio.h
 * @brief   [EN] Board-independent logical GPIO interface.
 *          [FA] رابط منطقی و مستقل از برد برای GPIO.
 *
 * @note    [EN] Product modules use logical signals; port/pin mapping stays in
 *          the board-specific BSP implementation.
 *          [FA] ماژول‌های محصول از سیگنال منطقی استفاده می‌کنند؛ نگاشت پورت و
 *          پایه فقط در پیاده‌سازی BSP مخصوص برد می‌ماند.
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>

typedef enum
{
    BSP_GPIO_BUZZER = 0,
    BSP_GPIO_LED_GREEN,
    BSP_GPIO_LED_RED,
    BSP_GPIO_LED_YELLOW,
    BSP_GPIO_ESP_CHPD,
    BSP_GPIO_INPUT_24V_PRESENT,
    BSP_GPIO_BATTERY_SWITCH,
    BSP_GPIO_RELAY,
    BSP_GPIO_PROTECT_BATTERY,
    BSP_GPIO_JITTER1,
    BSP_GPIO_JITTER2
} bsp_gpio_id_t;

/**
 * @brief  [EN] Write a logical board signal high or low.
 *         [FA] یک سیگنال منطقی برد را High یا Low می‌کند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier / شناسهٔ سیگنال منطقی
 * @param  bool__high [EN] true=high, false=low / مقدار High یا Low
 */
/* ==================== Functions ==================== */
void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__high);

/**
 * @brief  [EN] Read a logical board input signal.
 *         [FA] یک سیگنال ورودی منطقی برد را می‌خواند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier / شناسهٔ سیگنال منطقی
 * @return bool [EN] true if the physical signal is high / اگر سیگنال فیزیکی High باشد true
 */
bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id);

#endif /* BSP_GPIO_H */
