/**
 * @file    board_pins.h
 * @brief   [EN] Confirmed physical map from the ChangeOver schematic.
 *          [FA] نگاشت فیزیکی تأییدشده از شماتیک ChangeOver.
 *
 * @note    [EN] This header belongs to the STM32F103C8T6 board port. It is
 *          intentionally not included by product modules or public BSP APIs.
 *          [FA] این هدر متعلق به پورت برد STM32F103C8T6 است و عمداً توسط
 *          ماژول‌های محصول یا APIهای عمومی BSP وارد نمی‌شود.
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32f1xx_hal.h"

/* ==================== PWM outputs / خروجی‌های PWM ==================== */
/* [EN] Schematic MCU_PWM1 = PA0/TIM2_CH1 and MCU_PWM2 = PA6/TIM3_CH1.
 * [FA] در شماتیک MCU_PWM1 برابر ‎PA0/TIM2_CH1‎ و MCU_PWM2 برابر ‎PA6/TIM3_CH1‎ است. */

/* ==================== ADC inputs / ورودی‌های ADC ==================== */

/* ==================== Digital outputs / خروجی‌های دیجیتال ==================== */
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

/* [EN] Logical output polarity is board data, not module data. BAT_SWITCH and
 *      PROTECT_BATT are active-low on the confirmed schematic.
 * [FA] قطبیت خروجی منطقی دادهٔ برد است، نه دادهٔ ماژول. BAT_SWITCH و
 *      PROTECT_BATT در شماتیک تأییدشده ‎active-low‎ هستند. */
#define PIN_BUZZER_ACTIVE_HIGH       1u
#define PIN_ESP_CHPD_ACTIVE_HIGH     1u
#define PIN_LED_R_ACTIVE_HIGH        1u
#define PIN_LED_Y_ACTIVE_HIGH        1u
#define PIN_LED_G_ACTIVE_HIGH        1u
#define PIN_BAT_SWITCH_ACTIVE_HIGH   0u
#define PIN_RELAY_ACTIVE_HIGH        1u
#define PIN_PROTECT_BATT_ACTIVE_HIGH 0u

/* ==================== Digital inputs / ورودی‌های دیجیتال ==================== */
/* [EN] Schematic LM393 outputs are open-collector active-low: high is the
 *      released/no-trip state and low is the over-current/JIT trip state.
 * [FA] خروجی‌های LM393 در شماتیک ‎open-collector‎ و ‎active-low‎ هستند: high حالت
 *      آزاد/بدون تریپ و low حالت تریپ اضافه‌جریان/JIT است. */
#define PIN_JITTER1_PORT        GPIOB
#define PIN_JITTER1_PIN         GPIO_PIN_2      /* PB2 MCU_JITTER1 / JITT1, falling trip */
#define PIN_JITTER2_PORT        GPIOB
#define PIN_JITTER2_PIN         GPIO_PIN_6      /* PB6 MCU_JITTER2 / JITT2, falling trip */
#define PIN_INT_24_IN_PORT      GPIOB
#define PIN_INT_24_IN_PIN       GPIO_PIN_4      /* PB4 MCU_INT_24_IN */

/* ==================== ESP-Link UART / UART ارتباط ESP ==================== */

/* ==================== Safe startup levels / سطوح امن شروع ==================== */
/* [EN] Active-high loads are kept low; PB5 (MCU battery path Q1, active-low)
 *      is kept low to keep the MCU supplied from the battery at boot.
 *      PB11 (Changeover Q17, active-low) is kept high (safe disconnected)
 *      and remains independent from PB5.
 * [FA] بارهای ‎active-high‎ پایین نگه داشته می‌شوند؛ PB5 (مسیر باتری MCU با Q1،
 *      ‎active-low)‎ پایین نگه داشته می‌شود تا تغذیهٔ MCU از باتری در boot وصل بماند.
 *      PB11 (Changeover Q17، ‎active-low)‎ بالا و در وضعیت امن و مستقل از PB5 می‌ماند. */
#define PIN_SAFE_BUZZER_HIGH        0u
#define PIN_SAFE_ESP_CHPD_HIGH      0u
#define PIN_SAFE_LED_R_HIGH         0u
#define PIN_SAFE_LED_Y_HIGH         0u
#define PIN_SAFE_LED_G_HIGH         0u
#define PIN_SAFE_BAT_SWITCH_HIGH    0u
#define PIN_SAFE_RELAY_HIGH         0u
#define PIN_SAFE_PROTECT_BATT_HIGH  1u

#endif /* BOARD_PINS_H */
