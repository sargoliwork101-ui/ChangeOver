/**
 * @file    bsp_gpio.c
 * @brief   [EN] Maps logical GPIO signals to the current board pins.
 *          [FA] سیگنال‌های منطقی GPIO را به پایه‌های برد فعلی نگاشت می‌کند.
 *
 * @note    [EN] This is the board-specific GPIO port. A new board changes this
 *              file and its pin map; product modules keep the same interface.
 *          [FA] این فایل پورت GPIO مخصوص برد است. برای برد جدید این فایل و
 *              نقشهٔ پایه تغییر می‌کند و رابط ماژول‌های محصول ثابت می‌ماند.
 */

#include "bsp_gpio.h"
#include "board_pins.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  [EN] Resolve a logical signal to its physical HAL pin.
 *         [FA] سیگنال منطقی را به پورت و پایهٔ فیزیکی HAL تبدیل می‌کند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal / سیگنال منطقی
 * @param  GPIO_TypeDef__port [EN] Output physical port / پورت فیزیکی خروجی
 * @param  uint16_t__pin [EN] Output physical pin mask / ماسک پایهٔ فیزیکی خروجی
 * @return bool [EN] true when the mapping exists / اگر نگاشت وجود داشته باشد true
 */
static bool func__BspGpio_GetPin(bsp_gpio_id_t bsp_gpio_id_t__id,
                                  GPIO_TypeDef **GPIO_TypeDef__port,
                                  uint16_t *uint16_t__pin)
{
    if ((GPIO_TypeDef__port == NULL) || (uint16_t__pin == NULL))
    {
        return false;
    }

    switch (bsp_gpio_id_t__id)
    {
        case BSP_GPIO_BUZZER:
            *GPIO_TypeDef__port = PIN_BUZZER_PORT;
            *uint16_t__pin = PIN_BUZZER_PIN;
            break;
        case BSP_GPIO_LED_GREEN:
            *GPIO_TypeDef__port = PIN_LED_G_PORT;
            *uint16_t__pin = PIN_LED_G_PIN;
            break;
        case BSP_GPIO_LED_RED:
            *GPIO_TypeDef__port = PIN_LED_R_PORT;
            *uint16_t__pin = PIN_LED_R_PIN;
            break;
        case BSP_GPIO_LED_YELLOW:
            *GPIO_TypeDef__port = PIN_LED_Y_PORT;
            *uint16_t__pin = PIN_LED_Y_PIN;
            break;
        case BSP_GPIO_ESP_CHPD:
            *GPIO_TypeDef__port = PIN_ESP_CHPD_PORT;
            *uint16_t__pin = PIN_ESP_CHPD_PIN;
            break;
        case BSP_GPIO_INPUT_24V_PRESENT:
            *GPIO_TypeDef__port = PIN_INT_24_IN_PORT;
            *uint16_t__pin = PIN_INT_24_IN_PIN;
            break;
        case BSP_GPIO_BATTERY_SWITCH:
            *GPIO_TypeDef__port = PIN_BAT_SWITCH_PORT;
            *uint16_t__pin = PIN_BAT_SWITCH_PIN;
            break;
        case BSP_GPIO_RELAY:
            *GPIO_TypeDef__port = PIN_RELAY_PORT;
            *uint16_t__pin = PIN_RELAY_PIN;
            break;
        case BSP_GPIO_PROTECT_BATTERY:
            *GPIO_TypeDef__port = PIN_PROTECT_BATT_PORT;
            *uint16_t__pin = PIN_PROTECT_BATT_PIN;
            break;
        case BSP_GPIO_JITTER1:
            *GPIO_TypeDef__port = PIN_JITTER1_PORT;
            *uint16_t__pin = PIN_JITTER1_PIN;
            break;
        case BSP_GPIO_JITTER2:
            *GPIO_TypeDef__port = PIN_JITTER2_PORT;
            *uint16_t__pin = PIN_JITTER2_PIN;
            break;
        default:
            return false;
    }

    return true;
}

/**
 * @brief  [EN] Write a logical board signal high or low.
 *         [FA] یک سیگنال منطقی برد را High یا Low می‌کند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier / شناسهٔ سیگنال منطقی
 * @param  bool__high [EN] true=high, false=low / مقدار High یا Low
 */
/* ==================== BspGpio_Write ==================== */

void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__high)
{
    GPIO_TypeDef *GPIO_TypeDef__port;
    uint16_t uint16_t__pin;
    GPIO_PinState GPIO_PinState_level;

    if (func__BspGpio_GetPin(bsp_gpio_id_t__id,
                              &GPIO_TypeDef__port,
                              &uint16_t__pin) == false)
    {
        return;
    }

    if (bool__high == true)
    {
        GPIO_PinState_level = GPIO_PIN_SET;
    }
    else
    {
        GPIO_PinState_level = GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(GPIO_TypeDef__port, uint16_t__pin, GPIO_PinState_level);
}

/**
 * @brief  [EN] Read a logical board input signal.
 *         [FA] یک سیگنال ورودی منطقی برد را می‌خواند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier / شناسهٔ سیگنال منطقی
 * @return bool [EN] true if physical signal is high / اگر سیگنال فیزیکی High باشد true
 */
/* ==================== BspGpio_Read ==================== */

bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id)
{
    GPIO_TypeDef *GPIO_TypeDef__port;
    uint16_t uint16_t__pin;
    bool bool__isHigh = false;

    if (func__BspGpio_GetPin(bsp_gpio_id_t__id,
                              &GPIO_TypeDef__port,
                              &uint16_t__pin) == true)
    {
        if (HAL_GPIO_ReadPin(GPIO_TypeDef__port, uint16_t__pin) == GPIO_PIN_SET)
        {
            bool__isHigh = true;
        }
    }

    return bool__isHigh;
}
