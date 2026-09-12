#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * Pin map from schematic MICROCONTROLLER.SchDoc
 * Do not change without updating the board.
 *
 * Polarity comments come from schematic review, not from a powered measurement.
 * After step 2, replace "SCHEMATIC" with "MEASURED".
 */

#include "stm32f1xx_hal.h"

/* ---- PWM (chargers) ---- */
#define PIN_PWM1_PORT           GPIOA
#define PIN_PWM1_PIN            GPIO_PIN_0      /* PA0 TIM2_CH1 MCU_PWM1 */

#define PIN_PWM2_PORT           GPIOA
#define PIN_PWM2_PIN            GPIO_PIN_6      /* PA6 TIM3_CH1 MCU_PWM2 */

/* ---- ADC ---- */
#define PIN_ADC_CURRENT1_PORT   GPIOA
#define PIN_ADC_CURRENT1_PIN    GPIO_PIN_1      /* PA1 ADC1_IN1 */

#define PIN_ADC_24_IN_PORT      GPIOA
#define PIN_ADC_24_IN_PIN       GPIO_PIN_2      /* PA2 ADC1_IN2 */

#define PIN_ADC_24_BAT_PORT     GPIOA
#define PIN_ADC_24_BAT_PIN      GPIO_PIN_3      /* PA3 ADC1_IN3 */

#define PIN_ADC_12_BAT_PORT     GPIOA
#define PIN_ADC_12_BAT_PIN      GPIO_PIN_5      /* PA5 ADC1_IN5 */

#define PIN_ADC_CURRENT2_PORT   GPIOA
#define PIN_ADC_CURRENT2_PIN    GPIO_PIN_7      /* PA7 ADC1_IN7 */

/* ---- Digital outputs ---- */
/* مرحله ۱: فقط همین چهار پایه در CubeMX به‌صورت GPIO_Output تعریف شوند. */
#define PIN_BUZZER_PORT         GPIOA
#define PIN_BUZZER_PIN          GPIO_PIN_4      /* PA4  HIGH = بازر روشن از طریق Q7 */

#define PIN_ESP_CHPD_PORT       GPIOA
#define PIN_ESP_CHPD_PIN        GPIO_PIN_8      /* PA8  SCHEMATIC: high = ESP on */

#define PIN_LED_R_PORT          GPIOB
#define PIN_LED_R_PIN           GPIO_PIN_0      /* PB0  HIGH = LED قرمز روشن از طریق Q4 */

#define PIN_LED_Y_PORT          GPIOB
#define PIN_LED_Y_PIN           GPIO_PIN_1      /* PB1  HIGH = LED زرد روشن از طریق Q5 */

#define PIN_LED_G_PORT          GPIOB
#define PIN_LED_G_PIN           GPIO_PIN_10     /* PB10 HIGH = LED سبز روشن از طریق Q6 */

#define PIN_BAT_SWITCH_PORT     GPIOB
#define PIN_BAT_SWITCH_PIN      GPIO_PIN_5      /* PB5  MCU_CONTROL_PS
                                                 * SCHEMATIC: HIGH = Q3 off = battery disconnected from control PSU
                                                 *            LOW  = Q3 on  = battery feeds control PSU */

#define PIN_RELAY_PORT          GPIOB
#define PIN_RELAY_PIN           GPIO_PIN_7      /* PB7  SCHEMATIC: HIGH = charger relay on */

#define PIN_PROTECT_BATT_PORT   GPIOB
#define PIN_PROTECT_BATT_PIN    GPIO_PIN_11     /* PB11 SCHEMATIC: HIGH = force battery path OFF (Q17) */

/* ---- Digital inputs ---- */
#define PIN_JITTER1_PORT        GPIOB
#define PIN_JITTER1_PIN         GPIO_PIN_2      /* PB2  from LM393, 5V-tolerant pin */

#define PIN_JITTER2_PORT        GPIOB
#define PIN_JITTER2_PIN         GPIO_PIN_6      /* PB6 */

#define PIN_INT_24_IN_PORT      GPIOB
#define PIN_INT_24_IN_PIN       GPIO_PIN_4      /* PB4  needs SWD not Full JTAG */

#define PIN_TX_PORT             GPIOA
#define PIN_TX_PIN              GPIO_PIN_9      /* PA9  USART1_TX */

#define PIN_RX_PORT             GPIOA
#define PIN_RX_PIN              GPIO_PIN_10     /* PA10 USART1_RX */

#endif /* BOARD_PINS_H */
