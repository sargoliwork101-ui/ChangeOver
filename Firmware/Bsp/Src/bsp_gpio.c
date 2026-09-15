/**
 * @file    bsp_gpio.c
 * @brief   [EN] Thin HAL_GPIO wrapper. Full type naming, func__ prefix.
 *          [FA] پوشش نازک GPIO با HAL. نام تایپ کامل.
 */

#include "bsp_gpio.h"
#include <stddef.h>

/**
 * @brief  [EN] Write a pin high or low. Ignores NULL port.
 *         [FA] پایه را High یا Low می‌کند. پورت NULL را نادیده می‌گیرد.
 * @param  GPIO_TypeDef__port [EN] Port / پورت
 * @param  uint16_t__pin [EN] Pin mask / ماسک پایه
 * @param  bool__high [EN] true=3.3V / High یعنی ۳٫۳ ولت
 */
/* ==================== BspGpio_Write ==================== */

void func__BspGpio_Write(GPIO_TypeDef *GPIO_TypeDef__port, uint16_t uint16_t__pin, bool bool__high)
{
    GPIO_PinState GPIO_PinState_level;

    if (GPIO_TypeDef__port == NULL)
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
 * @brief  [EN] Read pin logic level. NULL port returns false.
 *         [FA] سطح منطقی پایه را می‌خواند. پورت NULL یعنی false.
 * @param  GPIO_TypeDef__port [EN] Port / پورت
 * @param  uint16_t__pin [EN] Pin mask / ماسک پایه
 * @return bool [EN] true if high / اگر High باشد true
 */
/* ==================== BspGpio_Read ==================== */

bool func__BspGpio_Read(GPIO_TypeDef *GPIO_TypeDef__port, uint16_t uint16_t__pin)
{
    bool bool__isHigh = false;

    if (GPIO_TypeDef__port != NULL)
    {
        if (HAL_GPIO_ReadPin(GPIO_TypeDef__port, uint16_t__pin) == GPIO_PIN_SET)
        {
            bool__isHigh = true;
        }
    }

    return bool__isHigh;
}
