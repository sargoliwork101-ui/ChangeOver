/**
 * @file    board_pins.h
 * @brief   [EN] Confirmed physical map from the ChangeOver schematic.
 * @note    [EN] This header belongs to the STM32F103C8T6 board port. It is
 *          intentionally not included by product modules or public BSP APIs.
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H
#include "stm32f1xx_hal.h"
/* ==================== PWM outputs / PWM ==================== */
/*
 * [EN] Schematic MCU_PWM1 = PA0/TIM2_CH1 and MCU_PWM2 = PA6/TIM3_CH1.
 */
/* ==================== ADC inputs / ADC ==================== */
/* ==================== Digital outputs ==================== */
#define PIN_BUZZER_PORT         GPIOA
#define PIN_BUZZER_PIN          GPIO_PIN_4      /* PA4 MCU_BUZZER, active high */
#define PIN_ESP_CHPD_PORT       GPIOA
#define PIN_ESP_CHPD_PIN        GPIO_PIN_8      /* PA8 MCU_ESP_CHPD, active high */
#define PIN_LED_R_PORT          GPIOB
#define PIN_LED_R_PIN           GPIO_PIN_0      /* PB0 MCU_R_LED, active high */
#define PIN_LED_Y_PORT          GPIOB
#define PIN_LED_Y_PIN           GPIO_PIN_1      /* PB1 MCU_Y_LED, active high */
#define PIN_LED_G_PORT          GPIOB
#define PIN_LED_G_PIN           GPIO_PIN_10     /* PB10 MCU_G_LED, active high */
#define PIN_BAT_SWITCH_PORT     GPIOB
#define PIN_BAT_SWITCH_PIN      GPIO_PIN_5      /* PB5 MCU_BAT_SWITCH */
#define PIN_RELAY_PORT          GPIOB
#define PIN_RELAY_PIN           GPIO_PIN_7      /* PB7 MCU_PROTECT_CHARGER / Relay_MICRO */
#define PIN_PROTECT_BATT_PORT   GPIOB
#define PIN_PROTECT_BATT_PIN    GPIO_PIN_11     /* PB11 MCU_LOW_BAT / MCU_PROTECT_BATT */
/*
 * [EN] Logical output polarity is board data, not module data.
 *      BAT_SWITCH (logical meaning: "battery switch closed") is ACTIVE LOW:
 *      PB5 High -> Q1 on -> Q2 on -> Q3 gate to 24 V -> IRF4905 off -> cut.
 *      PROTECT_BATT (logical meaning: "battery protect asserted / path cut")
 *      is ACTIVE HIGH: PB11 High -> Q17 on -> GATE_ON_OFF pulled low -> Q12
 *      off -> Q15 gate high -> series Q15/Q16 off -> battery never reaches OUT.
 *      Audit 2026-10-22 (user symptom: with the mains gone the battery never
 *      reached the output): PROTECT_BATT was declared active-low here while the
 *      schematic cuts with a physical High, so every logical "connect" was a
 *      real cut and PB11 stayed High from boot. Module logic and every
 *      threshold are unchanged; only this board datum was wrong.
 */
#define PIN_BUZZER_ACTIVE_HIGH       1u
#define PIN_ESP_CHPD_ACTIVE_HIGH     1u
#define PIN_LED_R_ACTIVE_HIGH        1u
#define PIN_LED_Y_ACTIVE_HIGH        1u
#define PIN_LED_G_ACTIVE_HIGH        1u
#define PIN_BAT_SWITCH_ACTIVE_HIGH   0u
#define PIN_RELAY_ACTIVE_HIGH        1u
#define PIN_PROTECT_BATT_ACTIVE_HIGH 1u
/* ==================== Digital inputs ==================== */
/*
 * [EN] Schematic LM393 outputs are open-collector active-low: high is the
 *      released/no-trip state and low is the over-current/JIT trip state.
 */
#define PIN_JITTER1_PORT        GPIOB
#define PIN_JITTER1_PIN         GPIO_PIN_2      /* PB2 MCU_JITTER1 / JITT1, falling trip */
#define PIN_JITTER2_PORT        GPIOB
#define PIN_JITTER2_PIN         GPIO_PIN_6      /* PB6 MCU_JITTER2 / JITT2, falling trip */
#define PIN_INT_24_IN_PORT      GPIOB
#define PIN_INT_24_IN_PIN       GPIO_PIN_4      /* PB4 MCU_INT_24_IN */
/*
 * [EN] Logical INPUT polarity, same convention as the outputs above. The two
 *      LM393 comparator outputs are open-collector and ACTIVE LOW, so the
 *      logical "trip" of JITTER1/JITTER2 is a physical Low. PB4 presence is
 *      the opposite: the comparator/divider drives it High when the DC input
 *      is present, so it is ACTIVE HIGH (see McuPowerPath/README.md).
 *      Before the 2026-10-20 audit all three were hard-coded active-high in
 *      bsp_gpio.c, which made a logical Read() of the JIT pins report the
 *      inverse of the trip state.
 */
#define PIN_JITTER1_ACTIVE_HIGH    0u
#define PIN_JITTER2_ACTIVE_HIGH    0u
#define PIN_INT_24_IN_ACTIVE_HIGH  1u
/* ==================== ESP-Link UART / UART ESP ==================== */
/* ==================== Safe startup levels ==================== */
/*
 * [EN] Active-high loads are kept low; PB5 (MCU battery path Q1, active-low)
 *      is kept low to keep the MCU supplied from the battery at boot.
 *      PB11 (Changeover Q17, active-high) is kept high at boot: that is the
 *      disconnected safe state, and Changeover closes the battery path on
 *      its first valid evaluation. PB11 stays independent from PB5.
 */
#define PIN_SAFE_BUZZER_HIGH        0u
#define PIN_SAFE_ESP_CHPD_HIGH      0u
#define PIN_SAFE_LED_R_HIGH         0u
#define PIN_SAFE_LED_Y_HIGH         0u
#define PIN_SAFE_LED_G_HIGH         0u
#define PIN_SAFE_BAT_SWITCH_HIGH    0u
#define PIN_SAFE_RELAY_HIGH         0u
#define PIN_SAFE_PROTECT_BATT_HIGH  1u
#endif /* BOARD_PINS_H */
