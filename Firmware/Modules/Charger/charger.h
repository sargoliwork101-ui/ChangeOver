/**
 * @file    charger.h
 * @brief   [EN] Two independent 12 V flyback charger channels. One generic
 *          policy is applied to a per-channel state record; the channels are
 *          never driven as a shared 24 V charger.
 *          [FA] دو کانال مستقل شارژر فلای‌بک ۱۲ ولت. یک سیاست عمومی روی
 *          state هر کانال اجرا می‌شود و کانال‌ها هرگز به‌صورت شارژر ۲۴ ولت
 *          مشترک کنترل نمی‌شوند.
 */

#ifndef CHARGER_H
#define CHARGER_H

/* ==================== Includes / شامل‌ها ==================== */
#include "app_types.h"
#include <stdint.h>

/* ==================== Board/test selection constants / ثابت‌های انتخاب برد و تست ==================== */
/*
 * [EN] These are the only two assembly-selection constants to change when the
 * installed transformer changes. Current hardware has Trans2 only. Set both
 * to 1 only after the second transformer, its current path, and its JIT input
 * have been verified on the board.
 * [FA] برای عوض‌کردن ترانس مونتاژشده فقط همین دو ثابت تغییر می‌کنند. اکنون
 * فقط Trans2 نصب است. هر دو را فقط بعد از تأیید سخت‌افزاری ترانس دوم، مسیر
 * جریان و ورودی JIT آن ۱ کنید.
 */
#define CHG_CHANNEL_1_INSTALLED       0u
#define CHG_CHANNEL_2_INSTALLED       1u

#define CHG_CHANNEL_1_MASK            (1u << 0)
#define CHG_CHANNEL_2_MASK            (1u << 1)
#define CHG_INSTALLED_CHANNEL_MASK    \
    (((CHG_CHANNEL_1_INSTALLED != 0u) ? CHG_CHANNEL_1_MASK : 0u) | \
     ((CHG_CHANNEL_2_INSTALLED != 0u) ? CHG_CHANNEL_2_MASK : 0u))

/* ==================== Conservative bring-up gate / دروازه امن راه‌اندازی ==================== */
/* [EN] Remains 0 until transformer data and calibration are measured. */
/* [FA] تا اندازه‌گیری داده ترانس و کالیبراسیون صفر می‌ماند. */
#define CHG_TRANSFORMER_KNOWN         0u

/* ==================== Electrical policy / سیاست الکتریکی ==================== */
#define CHG_PWM_FREQUENCY_HZ          50000u
#define CHG_PWM_TIMER_CLOCK_HZ        72000000u
#define CHG_PWM_PRESCALER             0u
#define CHG_PWM_AUTO_RELOAD           1439u
#define CHG_ABSORB_MV                 14400u
#define CHG_FLOAT_MV                  13500u
#define CHG_REENTRY_MV               12800u
#define CHG_BULK_CURRENT_MAX_MA       675u
#define CHG_CURRENT_LIMIT_MA           675u
/* External current-limited source setting for the first board test only. */
#define CHG_FIRST_BOARD_TEST_MAX_MA    100u
#define CHG_DUTY_START_PERMILLE        10u
#define CHG_DUTY_STEP_PERMILLE          5u
#define CHG_DUTY_RETRY_SECOND_MAX       100u
#define CHG_ABSORB_HOLD_MS          600000u
#define CHG_JIT_LOCKOUT_MS            3000u
#define CHG_RELAY_SETTLE_MS            100u
#define CHG_MIN_VALID_BATTERY_MV      2000u

/* ==================== Charger_Init / مقداردهی اولیه ==================== */
/**
 * @brief  [EN] Initialize policy state, stop both PWM channels and open the
 *              NC transformer-input relay. No nonzero PWM is allowed here.
 *         [FA] state سیاست را مقداردهی، هر دو PWM را متوقف و رله ورودی NC را
 *              برای قطع ورودی باز می‌کند. اینجا PWM غیرصفر مجاز نیست.
 */
void func__Charger_Init(void);

/* ==================== Charger_Evaluate / ارزیابی شارژر ==================== */
/**
 * @brief  [EN] Run the same control/protection algorithm independently for
 *              every installed 12 V channel. A low current is a regulation
 *              request to increase duty, not a fault.
 *         [FA] الگوریتم یکسان کنترل و حفاظت را برای هر کانال نصب‌شدهٔ ۱۲ ولت
 *              مستقل اجرا می‌کند. جریان کم درخواست افزایش duty است، نه fault.
 * @param  measurement_snapshot_t__snap [EN] Independent low/high battery snapshot / نمونه مستقل دو باتری
 * @param  app_state_t__state [EN] System state / حالت سیستم
 */
void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state);

#endif /* CHARGER_H */
