/**
 * @file    bsp_gpio.h
 * @brief   [EN] Board-independent logical GPIO interface.
 *          [FA] رابط منطقی و مستقل از برد برای GPIO.
 *
 * @note    [EN] Product modules use logical signals; physical ports, pins and
 *              active levels remain private to the board-specific port.
 *          [FA] ماژول‌های محصول از سیگنال‌های منطقی استفاده می‌کنند؛ پورت،
 *              پایه و سطح فعال فیزیکی در پورت مخصوص برد خصوصی می‌ماند.
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

/* ==================== BspGpio_Init ==================== */
/**
 * @brief  [EN] Apply the board safe state to every controllable output.
 *         [FA] وضعیت امن برد را روی همهٔ خروجی‌های قابل‌کنترل اعمال می‌کند.
 */
void func__BspGpio_Init(void);

/* ==================== BspGpio_Write ==================== */
/**
 * @brief  [EN] Assert or deassert a logical output. Board-specific active
 *              polarity is applied privately; input-only identifiers are ignored.
 *         [FA] یک خروجی منطقی را فعال یا غیرفعال می‌کند. قطبیت فعال مخصوص برد
 *              در پورت اعمال می‌شود و شناسه‌های فقط‌ورودی نادیده گرفته می‌شوند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @param  bool__asserted [EN] true=asserted/on, false=deasserted/off /
 *                             فعال/روشن یا غیرفعال/خاموش
 */
void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted);

/* ==================== BspGpio_Read ==================== */
/**
 * @brief  [EN] Read a logical signal. Board-specific active polarity is
 *              applied privately for outputs and active-level inputs.
 *         [FA] یک سیگنال منطقی را می‌خواند. قطبیت فعال مخصوص برد برای
 *              خروجی‌ها و ورودی‌های دارای سطح فعال در پورت اعمال می‌شود.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @return bool [EN] true when the logical signal is asserted /
 *                   اگر سیگنال منطقی فعال باشد true
 */
bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id);

#endif /* BSP_GPIO_H */
