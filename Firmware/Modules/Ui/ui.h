/**
 * @file    ui.h
 * @brief   [EN] UI main header - single source for all UI thresholds, includes LED and BUZZER split.
 *          Fully RTOS simple & readable, non-linear formulas broken into steps.
 *          [FA] هدر اصلی UI - تک فایل برای همه آستانه‌ها، شامل LED و BUZZER جدا شده.
 *
 * @note    [EN] Battery 0% = 21V (21000mV) = 0%, 100% = 28V (28000mV). Input present if V_in >=20V.
 *          Naming: after type double underscore __, func__ prefix.
 *          RTOS: vTaskDelay allowed (does not lock MCU), HAL_Delay forbidden. Formulas non-linear.
 *          Split into ui_led.h and ui_buzzer.h per user request, both in same UI folder.
 *          [FA] باتری 0% 21V، 100% 28V. نام‌گذاری با __، RTOS ساده، دو بخش LED و BUZZER.
 */

#ifndef UI_H
#define UI_H

/* ==================== Includes ==================== */

#include <stdint.h>
#include <stdbool.h>

/* ==================== Battery voltage mapping ==================== */

#define UI_BAT_V_MIN_MV                 21000u  /* [EN] 0% = 21V / صفر درصد = ۲۱ ولت */
#define UI_BAT_V_MAX_MV                 28000u  /* [EN] 100% = 28V / فول = ۲۸ ولت */

/* ==================== Input voltage threshold ==================== */

#define UI_INPUT_THRESHOLD_MV           20000u  /* [EN] <20V = no input, >=20V = present / زیر ۲۰ ولت نداریم */

/* ==================== Blink / Poll timings ==================== */

#define UI_INPUT_OK_POLL_MS             500u    /* [EN] InputOk steady hold / سبز ثابت ورودی وصل */
#define UI_SELFTEST_LED_MS              500u    /* [EN] Board test LED step / گام تست برد */
#define UI_BOOT_BEEP_MS                 150u    /* [EN] Board test beep / بوق تست برد */
#define UI_BLINK_PERIOD_MS              1000u   /* [EN] Green blink period BatteryRun / دوره چشمک سبز دشارژ */
#define UI_GREEN_MIN_OFF_MS             10u     /* [EN] Min off for green full / حداقل خاموشی سبز فول */
#define UI_CHARGING_BLINK_PERIOD_MS     1000u   /* [EN] Yellow blink period Charging / دوره چشمک زرد شارژ */
#define UI_CHARGING_YELLOW_MIN_OFF_MS   10u     /* [EN] Min off yellow almost full / حداقل خاموشی زرد */

/* ==================== Buzzer / Beep ==================== */

#define UI_BEEP_BASE_MS                 250u    /* [EN] Base beep / طول بوق پایه */
#define UI_BEEP_DOUBLE_THRESH_PCT       20u     /* [EN] Below this, duration x2 / زیر این ۲ برابر */
#define UI_BEEP_START_PCT               50u     /* [EN] Below this, periodic beep starts / زیر این بوق دوره‌ای */

/* ==================== Buzzer Pattern ==================== */

#define UI_BUZZER_DEFAULT_GAP_PERCENT   20u     /* [EN] Default gap 20% of onTime when repeat>1 / گپ پیش‌فرض ۲۰٪ */

/* ==================== Percent helpers ==================== */

#define UI_PERCENT_FULL                 100u
#define UI_PERCENT_SCALE                100u

/* ==================== RTOS tick ==================== */

#define UI_TICK_MS                      10u     /* [EN] Ui task tick 10ms, simple RTOS / تیکه ۱۰ میلی‌ثانیه */

/* ==================== LED and Buzzer Split ==================== */

/* [EN] UI split into LED and BUZZER per user request, both in same folder, markers above each function.
   [FA] UI دو بخش شد: LED و BUZZER، هر دو در همین پوشه، بالای هر تابع جدا کننده. */
#include "ui_led.h"
#include "ui_buzzer.h"

#endif /* UI_H */
