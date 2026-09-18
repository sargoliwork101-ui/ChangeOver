/**
 * @file    bsp_gpio.c
 * @brief   [EN] STM32F103C8T6 logical GPIO port for the ChangeOver schematic.
 *          [FA] پورت GPIO منطقی STM32F103C8T6 برای شماتیک ChangeOver.
 *
 * @note    [EN] This is the only layer that resolves logical signals to HAL
 *              ports and pins. Product modules do not include this file's
 *              physical mapping header.
 *          [FA] این تنها لایه‌ای است که سیگنال منطقی را به پورت و پایه HAL
 *              تبدیل می‌کند. ماژول‌های محصول هدر نگاشت فیزیکی این فایل را
 *              وارد نمی‌کنند.
 */

#include "bsp_gpio.h"
#include "board_pins.h"

#include <stddef.h>

/* ==================== BspGpio_GetPin ==================== */
/**
 * @brief  [EN] Resolve one logical signal to its physical HAL pin.
 *         [FA] یک سیگنال منطقی را به پایهٔ فیزیکی HAL تبدیل می‌کند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @param  GPIO_TypeDef__port [EN] Output physical GPIO port /
 *                                 پورت فیزیکی GPIO خروجی
 * @param  uint16_t__pin [EN] Output pin mask / ماسک پایهٔ خروجی
 * @return bool [EN] true when a mapping exists / اگر نگاشت وجود داشته باشد true
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

/* ==================== BspGpio_IsActiveHigh ==================== */
/**
 * @brief  [EN] Return the board active polarity for one logical signal.
 *         [FA] قطبیت فعال برد را برای یک سیگنال منطقی برمی‌گرداند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @return bool [EN] true when asserted is physical high /
 *                   اگر فعال‌بودن برابر High فیزیکی باشد true
 */
static bool func__BspGpio_IsActiveHigh(bsp_gpio_id_t bsp_gpio_id_t__id)
{
    bool bool__activeHigh = true;

    switch (bsp_gpio_id_t__id)
    {
        case BSP_GPIO_BUZZER:
            bool__activeHigh = (PIN_BUZZER_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_LED_GREEN:
            bool__activeHigh = (PIN_LED_G_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_LED_RED:
            bool__activeHigh = (PIN_LED_R_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_LED_YELLOW:
            bool__activeHigh = (PIN_LED_Y_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_ESP_CHPD:
            bool__activeHigh = (PIN_ESP_CHPD_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_BATTERY_SWITCH:
            bool__activeHigh = (PIN_BAT_SWITCH_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_RELAY:
            bool__activeHigh = (PIN_RELAY_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_PROTECT_BATTERY:
            bool__activeHigh = (PIN_PROTECT_BATT_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_INPUT_24V_PRESENT:
        case BSP_GPIO_JITTER1:
        case BSP_GPIO_JITTER2:
            /* [EN] Comparator/presence inputs are logically high when physical high.
               [FA] ورودی‌های comparator و حضور، در سطح فیزیکی High منطقی فعال‌اند. */
            bool__activeHigh = true;
            break;
        default:
            bool__activeHigh = true;
            break;
    }

    return bool__activeHigh;
}

/* ==================== BspGpio_IsWritable ==================== */
/**
 * @brief  [EN] Identify logical outputs accepted by the write API.
 *         [FA] خروجی‌های منطقی مجاز برای API نوشتن را مشخص می‌کند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @return bool [EN] true for a controllable output / برای خروجی قابل‌کنترل true
 */
static bool func__BspGpio_IsWritable(bsp_gpio_id_t bsp_gpio_id_t__id)
{
    bool bool__writable = false;

    switch (bsp_gpio_id_t__id)
    {
        case BSP_GPIO_BUZZER:
        case BSP_GPIO_LED_GREEN:
        case BSP_GPIO_LED_RED:
        case BSP_GPIO_LED_YELLOW:
        case BSP_GPIO_ESP_CHPD:
        case BSP_GPIO_BATTERY_SWITCH:
        case BSP_GPIO_RELAY:
        case BSP_GPIO_PROTECT_BATTERY:
            bool__writable = true;
            break;
        default:
            break;
    }

    return bool__writable;
}

/* ==================== BspGpio_Init ==================== */
/**
 * @brief  [EN] Apply safe startup levels to every board output.
 *         [FA] سطح امن شروع را روی همهٔ خروجی‌های برد اعمال می‌کند.
 */
void func__BspGpio_Init(void)
{
    HAL_GPIO_WritePin(PIN_BUZZER_PORT,
                      PIN_BUZZER_PIN,
                      (PIN_SAFE_BUZZER_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_ESP_CHPD_PORT,
                      PIN_ESP_CHPD_PIN,
                      (PIN_SAFE_ESP_CHPD_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_LED_R_PORT,
                      PIN_LED_R_PIN,
                      (PIN_SAFE_LED_R_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_LED_Y_PORT,
                      PIN_LED_Y_PIN,
                      (PIN_SAFE_LED_Y_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_LED_G_PORT,
                      PIN_LED_G_PIN,
                      (PIN_SAFE_LED_G_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_BAT_SWITCH_PORT,
                      PIN_BAT_SWITCH_PIN,
                      (PIN_SAFE_BAT_SWITCH_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_RELAY_PORT,
                      PIN_RELAY_PIN,
                      (PIN_SAFE_RELAY_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_PROTECT_BATT_PORT,
                      PIN_PROTECT_BATT_PIN,
                      (PIN_SAFE_PROTECT_BATT_HIGH != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ==================== BspGpio_Write ==================== */
/**
 * @brief  [EN] Assert or deassert a logical output; board polarity is private.
 *         [FA] خروجی منطقی را فعال یا غیرفعال می‌کند؛ قطبیت برد خصوصی است.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @param  bool__asserted [EN] true=asserted/on, false=deasserted/off /
 *                             فعال/روشن یا غیرفعال/خاموش
 */
void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted)
{
    GPIO_TypeDef *GPIO_TypeDef__port = NULL;
    uint16_t uint16_t__pin = 0u;
    GPIO_PinState GPIO_PinState_level;
    bool bool__activeHigh;
    bool bool__physicalHigh;

    if (func__BspGpio_IsWritable(bsp_gpio_id_t__id) == false)
    {
        return;
    }

    if (func__BspGpio_GetPin(bsp_gpio_id_t__id,
                              &GPIO_TypeDef__port,
                              &uint16_t__pin) == false)
    {
        return;
    }

    bool__activeHigh = func__BspGpio_IsActiveHigh(bsp_gpio_id_t__id);
    bool__physicalHigh = (bool__asserted == bool__activeHigh);
    GPIO_PinState_level = (bool__physicalHigh == true) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(GPIO_TypeDef__port, uint16_t__pin, GPIO_PinState_level);
}

/* ==================== BspGpio_Read ==================== */
/**
 * @brief  [EN] Read a logical board signal.
 *         [FA] یک سیگنال منطقی برد را می‌خواند.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 *                               شناسهٔ سیگنال منطقی
 * @return bool [EN] true when the logical signal is asserted /
 *                   اگر سیگنال منطقی فعال باشد true
 */
bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id)
{
    GPIO_TypeDef *GPIO_TypeDef__port = NULL;
    uint16_t uint16_t__pin = 0u;
    bool bool__isPhysicalHigh = false;
    bool bool__activeHigh = true;

    if (func__BspGpio_GetPin(bsp_gpio_id_t__id,
                              &GPIO_TypeDef__port,
                              &uint16_t__pin) == true)
    {
        bool__isPhysicalHigh = (HAL_GPIO_ReadPin(GPIO_TypeDef__port,
                                                 uint16_t__pin) == GPIO_PIN_SET);
        bool__activeHigh = func__BspGpio_IsActiveHigh(bsp_gpio_id_t__id);
    }

    return (bool__isPhysicalHigh == bool__activeHigh);
}
