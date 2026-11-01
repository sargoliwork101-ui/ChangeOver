/**
 * @file    bsp_gpio.c
 * @brief   [EN] STM32F103C8T6 logical GPIO port for the ChangeOver schematic.
 * @note    [EN] This is the only layer that resolves logical signals to HAL
 *              ports and pins. Product modules do not include this file's
 *              physical mapping header.
 */
#include "bsp_gpio.h"
#include "board_pins.h"
#include <stddef.h>
/* ==================== BspGpio_GetPin ==================== */
/**
 * @brief  [EN] Resolve one logical signal to its physical HAL pin.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 * @param  GPIO_TypeDef__port [EN] Output physical GPIO port /
* @param uint16_t__pin Output pin mask /
* @return bool true when a mapping exists / true
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
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 * @return bool [EN] true when asserted is physical high /
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
            bool__activeHigh = (PIN_INT_24_IN_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_JITTER1:
            /*
 * [EN] LM393 open-collector trip: asserted = physical LOW
 * (audit 2026-10-20 - this used to be hard-coded active-high).
 */
            bool__activeHigh = (PIN_JITTER1_ACTIVE_HIGH != 0u);
            break;
        case BSP_GPIO_JITTER2:
            /*
 * [EN] Same active-low comparator contract as JITTER1.
 */
            bool__activeHigh = (PIN_JITTER2_ACTIVE_HIGH != 0u);
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
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
* @return bool true for a controllable output / true
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
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 * @param  bool__asserted [EN] true=asserted/on, false=deasserted/off /
 */
void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted)
{
    GPIO_TypeDef *GPIO_TypeDef__port = NULL;
    uint16_t uint16_t__pin = 0u;
    GPIO_PinState GPIO_PinState__level;
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
    GPIO_PinState__level = (bool__physicalHigh == true) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(GPIO_TypeDef__port, uint16_t__pin, GPIO_PinState__level);
}
/* ==================== BspGpio_DisconnectBatteryPathIfInputPresent ==================== */
/**
 * @brief  [EN] Atomic "input still present? then cut the battery path".
 *              See the header contract: the pair runs with interrupts masked so
 *              the PB4 EXTI reconnect cannot be overwritten by a stale pass.
* @return bool true = path cut, false = input absent, battery kept on /
 */
bool func__BspGpio_DisconnectBatteryPathIfInputPresent(void)
{
    uint32_t uint32_t__savedPrimask;
    bool bool__inputPresent;
    bool bool__disconnected = false;
    uint32_t__savedPrimask = __get_PRIMASK();
    __disable_irq();
    bool__inputPresent = func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);
    if (bool__inputPresent == false)
    {
        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true);
    }
    else
    {
        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false);
        bool__disconnected = true;
    }
    __set_PRIMASK(uint32_t__savedPrimask);
    return bool__disconnected;
}
/* ==================== BspGpio_Read ==================== */
/**
 * @brief  [EN] Read a logical board signal.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 * @return bool [EN] true when the logical signal is asserted /
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
