/**
 * @file    bsp_gpio.c
 * @brief   [EN] Thin HAL_GPIO_WritePin / ReadPin wrapper.
 *          [FA] پوشش نازک نوشتن و خواندن GPIO با HAL.
 */

#include "bsp_gpio.h"
#include <stddef.h>

void BspGpio_Write(GPIO_TypeDef *port, uint16_t pin, bool high)
{
    GPIO_PinState level;

    if (port == NULL)
    {
        return;
    }

    if (high == true)
    {
        level = GPIO_PIN_SET;
    }
    else
    {
        level = GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(port, pin, level);
}

bool BspGpio_Read(GPIO_TypeDef *port, uint16_t pin)
{
    bool is_high = false;

    if (port != NULL)
    {
        if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET)
        {
            is_high = true;
        }
    }

    return is_high;
}
