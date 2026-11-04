/**
 * @file    app_config.c
 * @brief   [EN] Default configuration values. Uses ui_led.h and ui_buzzer.h split per user request, constants in own headers.
 * @note    [EN] Fully RTOS, no delay, naming with __ after type, func__ prefix. LED constants in ui_led.h, buzzer in ui_buzzer.h.
 */
/* ==================== Includes ==================== */
#include "app_config.h"
#include "ui_led.h"
#include "ui_buzzer.h"
const app_config_t APP_CONFIG =
{
    .ui_selftest_led_ms  = UI_SELFTEST_LED_MS,
    .ui_boot_beep_ms     = UI_BOOT_BEEP_MS,
    .control_period_ms   = 10u,
    .protection_period_ms = 5u,
    .comm_period_ms      = 100u
};
