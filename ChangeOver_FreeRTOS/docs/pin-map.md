# Pin Map

| MCU pin | نام شماتیک | عملکرد | نوع |
|---|---|---|---|
| PA0 | `MCU_PWM1` | PWM شارژر ۱ | TIM2_CH1 |
| PA1 | `MCU_ADC_CURRENT1` | Current 1 | ADC1_IN1 |
| PA2 | `MCU_ADC_24_IN` | 24V Input | ADC1_IN2 |
| PA3 | `MCU_ADC_24_BAT` | 24V Battery | ADC1_IN3 |
| PA4 | `MCU_BUZZER` | Buzzer | GPIO output |
| PA5 | `MCU_ADC_12_BAT` | 12V Battery/Common | ADC1_IN5 |
| PA6 | `MCU_PWM2` | PWM شارژر ۲ | TIM3_CH1 |
| PA7 | `MCU_ADC_CURRENT2` | Current 2 | ADC1_IN7 |
| PA8 | `MCU_ESP_CHPD` | ESP8266 CH_PD | GPIO output |
| PA9 | `MCU_TX` | UART TX | USART1_TX |
| PA10 | `MCU_RX` | UART RX | USART1_RX |
| PA13 | `SWDIO` | Debug | SWD |
| PA14 | `SWCLK` | Debug | SWD |
| PB0 | `MCU_R_LED` | LED قرمز | GPIO output |
| PB1 | `MCU_Y_LED` | LED زرد | GPIO output |
| PB2 | `MCU_JITTER1` | Jitter 1 | EXTI2 |
| PB3 | `SWO` | Trace در شماتیک | Debug conflict |
| PB4 | `MCU_INT_24_IN` | Input detect | EXTI4 / NJTRST conflict |
| PB5 | `MCU_BAT_SWITCH` | Battery Switch | GPIO output |
| PB6 | `MCU_JITTER2` | Jitter 2 | EXTI6 |
| PB7 | `MCU_PROTECT_CHARGER` | Relay / charger protection | GPIO output |
| PB10 | `MCU_G_LED` | LED سبز | GPIO output |
| PB11 | `MCU_PROTECT_BATT` | Battery protection | GPIO output |

## Debug

برای استفاده از PB4، `SYS -> Debug` باید روی `Serial Wire` باشد. استفاده‌ی هم‌زمان از SWO روی PB3 باید با نیاز واقعی Trace تطبیق داده شود.
