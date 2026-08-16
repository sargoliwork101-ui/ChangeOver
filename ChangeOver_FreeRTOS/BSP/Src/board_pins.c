/**
 * @file BSP/Src/board_pins.c
 * @brief C source for board_pins.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "board_pins.h"

const board_pin_t BOARD_PIN_PWM1 = { GPIOA, GPIO_PIN_0 };
const board_pin_t BOARD_PIN_PWM2 = { GPIOA, GPIO_PIN_6 };
const board_pin_t BOARD_PIN_BUZZER = { GPIOA, GPIO_PIN_4 };
const board_pin_t BOARD_PIN_ESP_CHIP_ENABLE = { GPIOA, GPIO_PIN_8 };
const board_pin_t BOARD_PIN_LED_RED = { GPIOB, GPIO_PIN_0 };
const board_pin_t BOARD_PIN_LED_YELLOW = { GPIOB, GPIO_PIN_1 };
const board_pin_t BOARD_PIN_BATTERY_SWITCH = { GPIOB, GPIO_PIN_5 };
const board_pin_t BOARD_PIN_CHARGER_RELAY = { GPIOB, GPIO_PIN_7 };
const board_pin_t BOARD_PIN_LED_GREEN = { GPIOB, GPIO_PIN_10 };
const board_pin_t BOARD_PIN_BATTERY_PROTECTION = { GPIOB, GPIO_PIN_11 };
const board_pin_t BOARD_PIN_JITTER1 = { GPIOB, GPIO_PIN_2 };
const board_pin_t BOARD_PIN_INPUT_DETECT = { GPIOB, GPIO_PIN_4 };
const board_pin_t BOARD_PIN_JITTER2 = { GPIOB, GPIO_PIN_6 };
