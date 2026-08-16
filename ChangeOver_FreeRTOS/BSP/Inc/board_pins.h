/**
 * @file BSP/Inc/board_pins.h
 * @brief C interface for board_pins.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

typedef struct
{
    GPIO_TypeDef * port;
    uint16_t pin;
} board_pin_t;

extern const board_pin_t BOARD_PIN_PWM1;
extern const board_pin_t BOARD_PIN_PWM2;
extern const board_pin_t BOARD_PIN_BUZZER;
extern const board_pin_t BOARD_PIN_ESP_CHIP_ENABLE;
extern const board_pin_t BOARD_PIN_LED_RED;
extern const board_pin_t BOARD_PIN_LED_YELLOW;
extern const board_pin_t BOARD_PIN_BATTERY_SWITCH;
extern const board_pin_t BOARD_PIN_CHARGER_RELAY;
extern const board_pin_t BOARD_PIN_LED_GREEN;
extern const board_pin_t BOARD_PIN_BATTERY_PROTECTION;
extern const board_pin_t BOARD_PIN_JITTER1;
extern const board_pin_t BOARD_PIN_INPUT_DETECT;
extern const board_pin_t BOARD_PIN_JITTER2;

#endif /* BOARD_PINS_H */
