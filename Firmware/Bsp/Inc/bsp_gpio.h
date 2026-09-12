#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>
#include "board_pins.h"

void BspGpio_Write(GPIO_TypeDef *port, uint16_t pin, bool high);
bool BspGpio_Read(GPIO_TypeDef *port, uint16_t pin);

#endif /* BSP_GPIO_H */
