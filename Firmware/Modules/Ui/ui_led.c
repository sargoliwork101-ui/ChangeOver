/**
 * @file    ui_led.c
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in ui_led.h.
 *          CMSIS-RTOS2 simple readable, non-linear formulas, markers above each function and variable in h and c.
 *          [FA] سناریوهای LED ماژول UI - ثابت‌های LED در هدر خودش، هر تابع و متغیر با جدا کننده و کامنت.
 *
 * @note    [EN] ui_led.h provides defaults; since v1.16 the live cadence
 *          comes from the persisted UI_ALARM_T__G__Alarm struct (ids
 *          38..76), not from const APP_CONFIG (which now feeds only the
 *          one-shot BoardTest timings). Naming __ after type, func__ prefix.
 *          CMSIS-RTOS2: osDelay allowed, HAL_Delay forbidden. Formulas non-linear broken into steps.
 *          [FA] ui_led.h پیش‌فرض‌ها را می‌دهد؛ از v1.16 مقدارهای زنده از
 *          struct ماندگار UI می‌آیند (۳۸..۷۶) نه از APP_CONFIG (که فقط
 *          زمان‌بندی تست برد را می‌دهد). نام‌گذاری با __، پیشوند func__،
 *          فرمول غیرخطی.
 */

#include "ui_led.h"
#include "ui_buzzer.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#include "modules_enable.h"

#if MODULE_FAULT
#include "fault.h"
#endif

#if MODULE_CHARGER
#include "charger.h"
#endif

#include <stdbool.h>

/* ==================== UI Global Battery Alarm Flag / فلگ سراسری آلارم باتری UI ==================== */

/**
 * @brief  [EN] Global flag owned by the UI: true while a valid low-battery alarm is active.
 *         It is continuous (level), not a pulse. Set in Init to false, cleared on invalid
 *         snapshot, set when v_bat24_mv < 21000 and cleared when >=21200.
 *         [FA] فلگ سراسری در مالکیت UI: هنگام آلارم معتبر باتری کم true است.
 */
volatile bool BOOL__G__UiBatteryAlarmIssued = false;

/* ==================== Runtime UI cadence (v1.16) ==================== */

/* [EN] v1.16 (user order 2026-09-26): the 39 UI_* numbers as one live
 *      struct - ids 38..76 (76 = panel-session mute, RAM-only), STM32-flash
 *      persisted, clamped as a set on
 *      every write. Boot = the macro defaults, so a reflash with an
 *      unreadable NVM record changes no behaviour.
 * [FA] v1.16 (دستور کاربر ۲۰۲۶-۰۹-۲۶): ۳۹ عدد UI_* در یک struct زنده -
 *      شناسه‌های ۳۸..۷۶، ماندگار در فلش، گیرهٔ مجموعه‌ای با هر نوشتن.
 *      بوت = پیش‌فرض ماکروها. */
/* [EN] volatile: written by the EspLink task (panel edits), read by the UI
   task (scenarios) with no lock - single-word members stay atomic and no
   reader may cache a half-applied set across one pass (full-program audit
   2026-09-26). [FA] بین دو تسک بدون قفل خوانده/نوشته می‌شود پس volatile. */
static volatile ui_alarm_t UI_ALARM_T__G__Alarm =
{
    UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS,
    UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT,
    UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS,
    UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS,
    UI_INPUT_OVERVOLTAGE_BEEP_COUNT,
    UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS,
    UI_BAT_LOST_LED_PERIOD_MS,
    UI_BAT_LOST_LED_DUTY_PERCENT,
    UI_BAT_LOST_BEEP_PERIOD_MS,
    /* [EN] v1.16: durations are uniformly PER-BEEP; the legacy 900 ms
       BatLost window was shared by 3 beeps + 2 gaps, i.e. (900-200)/3 =
       233 ms per beep - this keeps the exact boot sound (duty 30 ->
       [233,233,234], see host_test_ui).
       [FA] مدت‌ها یکنواخت «هر بوق»‌اند؛ پنجرهٔ ۹۰۰ قدیم یعنی ۲۳۳ هر بوق. */
    233u,
    UI_BAT_LOST_BEEP_COUNT,
    UI_BAT_LOST_BEEP_GAP_MS,
    UI_BATTERY_RUN_BEEP_START_PERCENT,
    UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT,
    UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT,
    UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT,
    UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS,
    UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_DUTY_PERCENT,
    UI_BATTERY_RUN_BEEP_CRITICAL_COUNT,
    UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS,
    UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS,
    UI_BATTERY_RUN_BEEP_STANDARD_COUNT,
    UI_BATTERY_RUN_BEEP_DOUBLE_COUNT,
    UI_BATTERY_RUN_BEEP_TRIPLE_COUNT,
    UI_BATTERY_RUN_BEEP_GAP_MS,
    UI_BLINK_PERIOD_MS,
    UI_GREEN_MIN_OFF_MS,
    UI_CHARGING_BLINK_PERIOD_MS,
    UI_CHARGING_YELLOW_MIN_ON_MS,
    UI_INPUT_OVERVOLTAGE_THRESHOLD_MV,
    UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV,
    UI_LOW_BATTERY_ALARM_THRESHOLD_MV,
    UI_LOW_BATTERY_ALARM_CLEAR_MV,
    UI_BAT_V_MIN_MV,
    UI_BAT_V_MAX_MV,
    0u
};

/**
 * @brief  [EN] Clamp one value into an outer window.
 *         [FA] گیرهٔ یک مقدار در پنجرهٔ بیرونی.
 */
static uint32_t func__Ui_ClampWindow(uint32_t uint32_t__value,
                                     uint32_t uint32_t__low,
                                     uint32_t uint32_t__high)
{
    if (uint32_t__value < uint32_t__low)
    {
        return uint32_t__low;
    }
    if (uint32_t__value > uint32_t__high)
    {
        return uint32_t__high;
    }
    return uint32_t__value;
}

/**
 * @brief  [EN] Clamp one beep period: 0 disables the pattern, otherwise
 *              1000..600000 (below 1000 the buzzer service would go
 *              INVALID and silent anyway).
 *         [FA] گیرهٔ دورهٔ بوق: صفر یعنی خاموش، وگرنه ۱۰۰۰..۶۰۰۰۰۰.
 */
static uint32_t func__Ui_ClampPeriod(uint32_t uint32_t__value)
{
    if (uint32_t__value == 0u)
    {
        return 0u;
    }
    return func__Ui_ClampWindow(uint32_t__value, 1000u, 600000u);
}

/**
 * @brief  [EN] Largest single-beep duration that still fits its period:
 *              dur*count + gap*(count-1) <= period. Overflow-safe: gap <=
 *              5000 and count <= 10, so the gap total stays below 45000.
 *         [FA] بیشترین مدت تک‌بوق که هنوز در دوره جا می‌شود.
 */
static uint32_t func__Ui_MaxBeepDurMs(uint32_t uint32_t__periodMs,
                                      uint32_t uint32_t__count,
                                      uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__gapsTotal;

    if (uint32_t__count == 0u)
    {
        return uint32_t__periodMs;
    }
    uint32_t__gapsTotal = uint32_t__gapMs * (uint32_t__count - 1u);
    if (uint32_t__gapsTotal >= uint32_t__periodMs)
    {
        return 0u;
    }
    return (uint32_t__periodMs - uint32_t__gapsTotal) / uint32_t__count;
}

/**
 * @brief  [EN] Re-clamp the whole UI set, single pass, dependency order
 *              inside each group (period -> count -> gap -> duration, then
 *              the band/threshold orderings). A duration is left stale
 *              while its period is 0 (pattern off - the duty helper
 *              guards the divide-by-zero and the service stays silent).
 *         [FA] گیرهٔ کل مجموعهٔ UI، یک پاس، ترتیب وابستگی در هر گروه.
 */
static void func__Ui_ClampAlarms(void)
{
    uint32_t uint32_t__maxDurMs;
    uint32_t uint32_t__maxCount;
    uint64_t uint64_t__critWindowMs;
    uint64_t uint64_t__critGapsMs;

    UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs, 100u, 10000u);
    UI_ALARM_T__G__Alarm.uint32_t__ovLedDutyPct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovLedDutyPct, 0u, 100u);
    UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs =
        func__Ui_ClampPeriod(UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs);
    UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount, 0u, 10u);
    UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs, 0u, 5000u);
    if ((UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount > 1u) &&
        (UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs != 0u) &&
        (UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs < UI_BUZZER_MIN_GAP_MS))
    {
        UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs = UI_BUZZER_MIN_GAP_MS;
    }
    UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs, 0u, 600000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs != 0u)
    {
        uint32_t__maxDurMs = func__Ui_MaxBeepDurMs(
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs,
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount,
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs);
        if (UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs > uint32_t__maxDurMs)
        {
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs = uint32_t__maxDurMs;
        }
    }

    UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs, 100u, 10000u);
    UI_ALARM_T__G__Alarm.uint32_t__blLedDutyPct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__blLedDutyPct, 0u, 100u);
    UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs =
        func__Ui_ClampPeriod(UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs);
    UI_ALARM_T__G__Alarm.uint32_t__blBeepCount =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__blBeepCount, 0u, 10u);
    UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs, 0u, 5000u);
    if ((UI_ALARM_T__G__Alarm.uint32_t__blBeepCount > 1u) &&
        (UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs != 0u) &&
        (UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs < UI_BUZZER_MIN_GAP_MS))
    {
        UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs = UI_BUZZER_MIN_GAP_MS;
    }
    UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs, 0u, 600000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs != 0u)
    {
        uint32_t__maxDurMs = func__Ui_MaxBeepDurMs(
            UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs,
            UI_ALARM_T__G__Alarm.uint32_t__blBeepCount,
            UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs);
        if (UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs > uint32_t__maxDurMs)
        {
            UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs = uint32_t__maxDurMs;
        }
    }

    /* [EN] Beep bands must stay ordered: start >= double >= triple >=
       crit. One top-down pass always converges (each level is pulled
       down to its ceiling); start is authoritative.
       [FA] باندها مرتب: یک پاس از بالا همیشه همگرا می‌شود؛ start مرجع. */
    UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct, 0u, 100u);
    UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct, 0u, 100u);
    UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct, 0u, 100u);
    UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct, 0u, 100u);
    if (UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct >
        UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct)
    {
        UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct =
            UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct;
    }
    if (UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct >
        UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct)
    {
        UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct =
            UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct;
    }
    if (UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct >
        UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct)
    {
        UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct =
            UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct;
    }

    UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs =
        func__Ui_ClampPeriod(UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs);
    UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs =
        func__Ui_ClampPeriod(UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs);
    UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs =
        func__Ui_ClampPeriod(UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs);
    UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct, 0u, 100u);
    UI_ALARM_T__G__Alarm.uint32_t__runCritCount =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runCritCount, 0u, 10u);
    UI_ALARM_T__G__Alarm.uint32_t__runStdCount =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runStdCount, 0u, 10u);
    UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount, 0u, 10u);
    UI_ALARM_T__G__Alarm.uint32_t__runTriCount =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runTriCount, 0u, 10u);
    UI_ALARM_T__G__Alarm.uint32_t__runGapMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runGapMs, 0u, 5000u);
    if (((UI_ALARM_T__G__Alarm.uint32_t__runCritCount > 1u) ||
         (UI_ALARM_T__G__Alarm.uint32_t__runStdCount > 1u) ||
         (UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount > 1u) ||
         (UI_ALARM_T__G__Alarm.uint32_t__runTriCount > 1u)) &&
        (UI_ALARM_T__G__Alarm.uint32_t__runGapMs < UI_BUZZER_MIN_GAP_MS))
    {
        UI_ALARM_T__G__Alarm.uint32_t__runGapMs = UI_BUZZER_MIN_GAP_MS;
    }
    UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs, 0u, 600000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs != 0u)
    {
        /* [EN] The standard duration is shared by the 1-beep and 2-beep
           bands, so it must fit the WIDER count of the two.
           [FA] مدت استاندارد بین باند ۱-بوق و ۲-بوق مشترک است پس باید
           در تعداد بیشتر جا شود. */
        uint32_t__maxCount = UI_ALARM_T__G__Alarm.uint32_t__runStdCount;
        if (UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount > uint32_t__maxCount)
        {
            uint32_t__maxCount = UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount;
        }
        uint32_t__maxDurMs = func__Ui_MaxBeepDurMs(
            UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
            uint32_t__maxCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
        if (UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs > uint32_t__maxDurMs)
        {
            UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs = uint32_t__maxDurMs;
        }
    }
    UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs, 0u, 600000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs != 0u)
    {
        uint32_t__maxDurMs = func__Ui_MaxBeepDurMs(
            UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs,
            UI_ALARM_T__G__Alarm.uint32_t__runTriCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
        if (UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs > uint32_t__maxDurMs)
        {
            UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs = uint32_t__maxDurMs;
        }
    }
    UI_ALARM_T__G__Alarm.uint32_t__runCritDurMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__runCritDurMs, 0u, 120000u);
    /* [EN] v1.16e: the critical pattern must fit its own window, or the
       buzzer service goes INVALID and the dying battery gets NO
       indication at all (every LED is already off below the crit band).
       Window = period*duty/100 must leave >= 1 ms per pulse after the
       gaps; otherwise pull the count down (it always converges: one
       pulse always fits a non-zero window, which is >= 10 ms here).
       Period 0 / duty 0 / count 0 stay a valid intentional silence.
       [FA] الگوی بحرانی باید در پنجرهٔ خودش جا شود وگرنه سرویس
       نامعتبر می‌شود و باتریِ در حال مرگ هیچ نشانه‌ای ندارد (همهٔ
       LEDها زیر باند بحرانی خاموش‌اند). پنجره باید بعد از گپ‌ها
       دست‌کم ۱ms برای هر بوق باقی بگذارد؛ وگرنه تعداد کم می‌شود.
       صفر بودن دوره/دیوتی/تعداد یعنی سکوت عمدی و دست نمی‌خورد. */
    if ((UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs != 0u) &&
        (UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct != 0u) &&
        (UI_ALARM_T__G__Alarm.uint32_t__runCritCount > 1u))
    {
        uint64_t__critWindowMs =
            ((uint64_t)UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs *
             (uint64_t)UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct) /
            UI_PERCENT_SCALE;
        while (UI_ALARM_T__G__Alarm.uint32_t__runCritCount > 1u)
        {
            uint64_t__critGapsMs =
                (uint64_t)UI_ALARM_T__G__Alarm.uint32_t__runGapMs *
                (uint64_t)(UI_ALARM_T__G__Alarm.uint32_t__runCritCount - 1u);
            if ((uint64_t__critGapsMs < uint64_t__critWindowMs) &&
                ((uint64_t__critWindowMs - uint64_t__critGapsMs) >=
                 (uint64_t)UI_ALARM_T__G__Alarm.uint32_t__runCritCount))
            {
                break;
            }
            UI_ALARM_T__G__Alarm.uint32_t__runCritCount--;
        }
    }

    UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs, 100u, 10000u);
    UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs, 0u, 10000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs >
        UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs)
    {
        UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs =
            UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs;
    }
    UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs, 100u, 10000u);
    UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs, 0u, 10000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs >
        UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs)
    {
        UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs =
            UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs;
    }

    UI_ALARM_T__G__Alarm.uint32_t__ovThreshMv =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovThreshMv, 24000u, 32000u);
    UI_ALARM_T__G__Alarm.uint32_t__ovHystMv =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__ovHystMv, 0u, 2000u);

    /* [EN] The threshold is authoritative: the clear level is pulled up to
       it, never the threshold down (zero hysteresis = a consistent level).
       [FA] آستانه مرجع است: سطح پاک‌شدن به آن بالا کشیده می‌شود. */
    UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv, 15000u, 24000u);
    UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv, 15000u, 24000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv <
        UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv)
    {
        UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv =
            UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv;
    }
    if (UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv >
        UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv)
    {
        UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv =
            UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv;
    }

    /* [EN] The percent map must keep a strictly positive range (division
       safety); Vmin is authoritative, Vmax is pulled up.
       [FA] نگاشت درصد باید بازهٔ اکیداً مثبت نگه دارد؛ Vmin مرجع است. */
    UI_ALARM_T__G__Alarm.uint32_t__pctVminMv =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__pctVminMv, 15000u, 25000u);
    UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv, 25000u, 32000u);
    if (UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv <
        (UI_ALARM_T__G__Alarm.uint32_t__pctVminMv + 100u))
    {
        UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv =
            UI_ALARM_T__G__Alarm.uint32_t__pctVminMv + 100u;
    }
    if (UI_ALARM_T__G__Alarm.uint32_t__pctVminMv >
        (UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv - 100u))
    {
        UI_ALARM_T__G__Alarm.uint32_t__pctVminMv =
            UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv - 100u;
    }

    UI_ALARM_T__G__Alarm.uint32_t__buzzerMute =
        func__Ui_ClampWindow(UI_ALARM_T__G__Alarm.uint32_t__buzzerMute, 0u, 1u);
}

/**
 * @brief  [EN] Duty percent of one beep pattern from its live window:
 *              ceil((dur*count + gap*(count-1)) * 100 / period). The ceil
 *              matches the legacy DUTY_PERCENT macros exactly on defaults
 *              (exact divisions are unaffected). A zero period yields 0
 *              (the caller passes period 0 = buzzer off). Overflow-safe:
 *              the clamped window never exceeds the period (<= 600000),
 *              x100 stays far below 2^32, and the result never exceeds
 *              100. Formula broken into steps (beeps, gaps+window, duty).
 *         [FA] درصد دیوتی یک الگوی بوق از پنجرهٔ زنده‌اش (سقف‌گرد مثل
 *              ماکروهای قدیم) - غیرخطی ۳ گام.
 */
static uint8_t func__Ui_BeepDutyPercent(uint32_t uint32_t__periodMs,
                                        uint32_t uint32_t__durMs,
                                        uint32_t uint32_t__count,
                                        uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__beepsTotalMs;
    uint32_t uint32_t__gapsTotalMs;
    uint32_t uint32_t__windowMs;
    uint32_t uint32_t__scaledWindow;

    if ((uint32_t__periodMs == 0u) || (uint32_t__durMs == 0u) ||
        (uint32_t__count == 0u))
    {
        return 0u;
    }

    /* [EN] Step 1: beeps window = dur * count
       [FA] گام ۱: پنجرهٔ بوق‌ها */
    uint32_t__beepsTotalMs = uint32_t__durMs * uint32_t__count;

    /* [EN] Step 2: gaps window + full duty window
       [FA] گام ۲: پنجرهٔ گپ‌ها + پنجرهٔ کامل دیوتی */
    uint32_t__gapsTotalMs = 0u;
    if (uint32_t__count > 1u)
    {
        uint32_t__gapsTotalMs = uint32_t__gapMs * (uint32_t__count - 1u);
    }
    uint32_t__windowMs = uint32_t__beepsTotalMs + uint32_t__gapsTotalMs;

    /* [EN] A window wider than the period is infeasible (gaps alone can
       overflow it): report 101 so the service goes INVALID = deterministic
       silence instead of a uint8-truncated phantom duty.
       [FA] پنجرهٔ بزرگ‌تر از دوره ناممکن است: ۱۰۱ بده تا سرویس deterministic
       ساکت شود نه دیوتی بریده‌شده. */
    if (uint32_t__windowMs > uint32_t__periodMs)
    {
        return (uint8_t)(UI_BUZZER_DUTY_MAX_PERCENT + 1u);
    }

    /* [EN] Step 3: duty = ceil(window * 100 / period)
       [FA] گام ۳: دیوتی با سقف‌گرد */
    uint32_t__scaledWindow = uint32_t__windowMs * UI_PERCENT_SCALE;
    return (uint8_t)((uint32_t__scaledWindow + uint32_t__periodMs - 1u) /
                     uint32_t__periodMs);
}

/**
 * @brief  [EN] Buzzer service with the panel-session mute: while id 76 is set,
 *              every SCENARIO pattern is replaced by an explicit off (the
 *              one-shot BoardTest wiring beep calls the service directly
 *              and still sounds, so a muted board still proves its buzzer
 *              works at boot).
 *         [FA] سرویس بوق با میوت جلسه‌ای: تا وقتی ۷۶ ست است هر الگوی
 *              سناریو با خاموش صریح جایگزین می‌شود (بوق تست برد مستقیم
 *              است و همچنان می‌زند).
 */
static int32_t func__Ui_Buzzer_Gated(uint32_t uint32_t__periodMs,
                                     uint8_t uint8_t__dutyPercent,
                                     uint8_t uint8_t__beepCount,
                                     uint32_t uint32_t__gapMs)
{
    if (UI_ALARM_T__G__Alarm.uint32_t__buzzerMute != 0u)
    {
        return func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    return func__Ui_Buzzer_Tick(uint32_t__periodMs,
                                uint8_t__dutyPercent,
                                uint8_t__beepCount,
                                uint32_t__gapMs);
}

bool func__Ui_SetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t uint32_t__value,
                            uint32_t *uint32_t__appliedValue)
{
    switch (uint8_t__paramId)
    {
        case UI_ALARM_PARAM_OV_LED_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_LED_DUTY_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__ovLedDutyPct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_BEEP_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_BEEP_DUR_MS:
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_BEEP_COUNT:
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_BEEP_GAP_MS:
            UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BL_LED_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BL_LED_DUTY_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__blLedDutyPct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BL_BEEP_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BL_BEEP_DUR_MS:
            UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BL_BEEP_COUNT:
            UI_ALARM_T__G__Alarm.uint32_t__blBeepCount = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BL_BEEP_GAP_MS:
            UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_BEEP_START_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_BEEP_DOUBLE_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_BEEP_TRIPLE_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_BEEP_CRIT_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_STD_INTERVAL_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_TRI_INTERVAL_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_CRIT_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_CRIT_DUTY_PCT:
            UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_CRIT_COUNT:
            UI_ALARM_T__G__Alarm.uint32_t__runCritCount = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_STD_DUR_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_TRI_DUR_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_CRIT_DUR_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runCritDurMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_STD_COUNT:
            UI_ALARM_T__G__Alarm.uint32_t__runStdCount = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_DOUBLE_COUNT:
            UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_TRI_COUNT:
            UI_ALARM_T__G__Alarm.uint32_t__runTriCount = uint32_t__value;
            break;
        case UI_ALARM_PARAM_RUN_GAP_MS:
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_GREEN_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_GREEN_MIN_OFF_MS:
            UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_YELLOW_PERIOD_MS:
            UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_YELLOW_MIN_ON_MS:
            UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_THRESH_MV:
            UI_ALARM_T__G__Alarm.uint32_t__ovThreshMv = uint32_t__value;
            break;
        case UI_ALARM_PARAM_OV_HYST_MV:
            UI_ALARM_T__G__Alarm.uint32_t__ovHystMv = uint32_t__value;
            break;
        case UI_ALARM_PARAM_LOWBAT_THRESH_MV:
            UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv = uint32_t__value;
            break;
        case UI_ALARM_PARAM_LOWBAT_CLEAR_MV:
            UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv = uint32_t__value;
            break;
        case UI_ALARM_PARAM_PCT_VMIN_MV:
            UI_ALARM_T__G__Alarm.uint32_t__pctVminMv = uint32_t__value;
            break;
        case UI_ALARM_PARAM_PCT_VMAX_MV:
            UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv = uint32_t__value;
            break;
        case UI_ALARM_PARAM_BUZZER_MUTE:
            UI_ALARM_T__G__Alarm.uint32_t__buzzerMute = uint32_t__value;
            break;
        default:
            return false;
    }

    func__Ui_ClampAlarms();
    return func__Ui_GetAlarmParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Ui_GetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t *uint32_t__value)
{
    switch (uint8_t__paramId)
    {
        case UI_ALARM_PARAM_OV_LED_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs;
            return true;
        case UI_ALARM_PARAM_OV_LED_DUTY_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovLedDutyPct;
            return true;
        case UI_ALARM_PARAM_OV_BEEP_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs;
            return true;
        case UI_ALARM_PARAM_OV_BEEP_DUR_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs;
            return true;
        case UI_ALARM_PARAM_OV_BEEP_COUNT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount;
            return true;
        case UI_ALARM_PARAM_OV_BEEP_GAP_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs;
            return true;
        case UI_ALARM_PARAM_BL_LED_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs;
            return true;
        case UI_ALARM_PARAM_BL_LED_DUTY_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__blLedDutyPct;
            return true;
        case UI_ALARM_PARAM_BL_BEEP_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs;
            return true;
        case UI_ALARM_PARAM_BL_BEEP_DUR_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs;
            return true;
        case UI_ALARM_PARAM_BL_BEEP_COUNT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__blBeepCount;
            return true;
        case UI_ALARM_PARAM_BL_BEEP_GAP_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs;
            return true;
        case UI_ALARM_PARAM_RUN_BEEP_START_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct;
            return true;
        case UI_ALARM_PARAM_RUN_BEEP_DOUBLE_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct;
            return true;
        case UI_ALARM_PARAM_RUN_BEEP_TRIPLE_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct;
            return true;
        case UI_ALARM_PARAM_RUN_BEEP_CRIT_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct;
            return true;
        case UI_ALARM_PARAM_RUN_STD_INTERVAL_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs;
            return true;
        case UI_ALARM_PARAM_RUN_TRI_INTERVAL_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs;
            return true;
        case UI_ALARM_PARAM_RUN_CRIT_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs;
            return true;
        case UI_ALARM_PARAM_RUN_CRIT_DUTY_PCT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct;
            return true;
        case UI_ALARM_PARAM_RUN_CRIT_COUNT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runCritCount;
            return true;
        case UI_ALARM_PARAM_RUN_STD_DUR_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs;
            return true;
        case UI_ALARM_PARAM_RUN_TRI_DUR_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs;
            return true;
        case UI_ALARM_PARAM_RUN_CRIT_DUR_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runCritDurMs;
            return true;
        case UI_ALARM_PARAM_RUN_STD_COUNT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runStdCount;
            return true;
        case UI_ALARM_PARAM_RUN_DOUBLE_COUNT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount;
            return true;
        case UI_ALARM_PARAM_RUN_TRI_COUNT:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runTriCount;
            return true;
        case UI_ALARM_PARAM_RUN_GAP_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__runGapMs;
            return true;
        case UI_ALARM_PARAM_GREEN_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs;
            return true;
        case UI_ALARM_PARAM_GREEN_MIN_OFF_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs;
            return true;
        case UI_ALARM_PARAM_YELLOW_PERIOD_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs;
            return true;
        case UI_ALARM_PARAM_YELLOW_MIN_ON_MS:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs;
            return true;
        case UI_ALARM_PARAM_OV_THRESH_MV:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovThreshMv;
            return true;
        case UI_ALARM_PARAM_OV_HYST_MV:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__ovHystMv;
            return true;
        case UI_ALARM_PARAM_LOWBAT_THRESH_MV:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv;
            return true;
        case UI_ALARM_PARAM_LOWBAT_CLEAR_MV:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv;
            return true;
        case UI_ALARM_PARAM_PCT_VMIN_MV:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__pctVminMv;
            return true;
        case UI_ALARM_PARAM_PCT_VMAX_MV:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv;
            return true;
        case UI_ALARM_PARAM_BUZZER_MUTE:
            *uint32_t__value = UI_ALARM_T__G__Alarm.uint32_t__buzzerMute;
            return true;
        default:
            return false;
    }
}

/* ==================== Battery Voltage To Percent / تبدیل ولتاژ باتری به درصد ==================== */

/**
 * @brief  [EN] Battery voltage to percent 0..100. Non-linear formula broken into 4 steps: range, offset, scaled, percent.
 *         [FA] ولتاژ باتری به درصد - فرمول غیرخطی ۴ گام: بازه، فاصله، مقیاس، درصد.
 * @param  uint32_t__batteryMv [EN] Battery voltage in mV, 0..40000mV, 21000=0% 29000=100% / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__voltageRangeMv;
    uint32_t uint32_t__voltageOffsetMv;
    uint32_t uint32_t__scaledOffset;
    uint8_t uint8_t__batteryPercent;

    /* [EN] v1.16: the percent map is runtime (ids 74/75), clamped to a
       strictly positive range; the range==0 guard below stays as a belt.
       [FA] نسخه ۱.۱۶: نگاشت درصد زمان‌اجرا است (۷۴/۷۵). */
    if (uint32_t__batteryMv <= UI_ALARM_T__G__Alarm.uint32_t__pctVminMv)
    {
        return 0u;
    }

    if (uint32_t__batteryMv >= UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv)
    {
        return UI_PERCENT_FULL;
    }

    /* [EN] Step 1: range = Vmax - Vmin
       [FA] گام ۱: بازه ولتاژ */
    uint32_t__voltageRangeMv = UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv -
                               UI_ALARM_T__G__Alarm.uint32_t__pctVminMv;

    /* [EN] Step 2: offset = Vbat - Vmin
       [FA] گام ۲: فاصله از کف */
    uint32_t__voltageOffsetMv = uint32_t__batteryMv -
                                UI_ALARM_T__G__Alarm.uint32_t__pctVminMv;

    if (uint32_t__voltageRangeMv == 0u)
    {
        return 0u;
    }

    /* [EN] Step 3: scaled = offset * 100
       [FA] گام ۳: مقیاس به درصد */
    uint32_t__scaledOffset = uint32_t__voltageOffsetMv * UI_PERCENT_SCALE;

    /* [EN] Step 4: percent = scaled / range
       [FA] گام ۴: تقسیم برای درصد */
    uint8_t__batteryPercent = (uint8_t)(uint32_t__scaledOffset / uint32_t__voltageRangeMv);

    if (uint8_t__batteryPercent > UI_PERCENT_FULL)
    {
        uint8_t__batteryPercent = UI_PERCENT_FULL;
    }

    return uint8_t__batteryPercent;
}

/* ==================== Green LED / LED سبز ==================== */

/**
 * @brief  [EN] Drive green LED on/off. Low-level wrapper around BSP GPIO.
 *         [FA] ال‌ای‌دی سبز را روشن/خاموش می‌کند - سطح پایین.
 * @param  bool__greenOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__green(bool bool__greenOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_GREEN, bool__greenOn);
}

/* ==================== Red LED / LED قرمز ==================== */

/**
 * @brief  [EN] Drive red LED on/off. Low-level.
 *         [FA] ال‌ای‌دی قرمز را روشن/خاموش می‌کند.
 * @param  bool__redOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__red(bool bool__redOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_RED, bool__redOn);
}

/* ==================== Yellow LED / LED زرد ==================== */

/**
 * @brief  [EN] Drive yellow LED on/off. Low-level.
 *         [FA] ال‌ای‌دی زرد را روشن/خاموش می‌کند.
 * @param  bool__yellowOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__yellow(bool bool__yellowOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_YELLOW, bool__yellowOn);
}

/* ==================== All Off Safe / خاموشی امن همه خروجی‌ها ==================== */

/**
 * @brief  [EN] Drive all LEDs off and request the buzzer service to enter its safe-off state.
 *         [FA] همه ال‌ای‌دی‌ها را خاموش می‌کند و سرویس بوق را به حالت خاموش امن می‌برد.
 */
static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
}

/* ==================== Input connection state / وضعیت اتصال ورودی ==================== */

/**
 * @brief  [EN] Stateful input-connected result used by the 20V/21V hysteresis.
 *         It starts disconnected as the safe default.
 *         [FA] نتیجه دارای وضعیت تشخیص اتصال ورودی با هیسترزیس ۲۰/۲۱ ولت.
 *         مقدار اولیه برای حالت امن، قطع است.
 */
static bool BOOL__G__UiInputPresent = false;

/* ==================== Input overvoltage state / وضعیت اضافه‌ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Stateful input overvoltage error flag.
 *         The flag remains active between 27V and 28V after an overvoltage event.
 *         [FA] پرچم دارای وضعیت خطای اضافه‌ولتاژ ورودی.
 *         بعد از رخداد خطا، بین ۲۷ و ۲۸ ولت فعال باقی می‌ماند.
 */
static bool BOOL__G__UiInputOverVoltage = false;

/**
 * @brief  [EN] CMSIS-RTOS2 tick at which the current input overvoltage display started.
 *         [FA] تیک RTOS در زمان شروع نمایش خطای اضافه‌ولتاژ ورودی.
 */
static uint32_t TICKTYPE_T__G__UiInputOverVoltageStartTick = 0;

/* ==================== BatteryRun critical beep state / وضعیت بوق بحرانی BatteryRun ==================== */

/**
 * @brief  [EN] TRUE while the one-time critical BatteryRun beep is active.
 *         [FA] هنگام فعال‌بودن بوق بحرانی تک‌باره BatteryRun مقدار TRUE دارد.
 */
static bool BOOL__G__UiBatteryCriticalBeepActive = false;

/**
 * @brief  [EN] TRUE after the one-time critical BatteryRun beep has completed.
 *         [FA] بعد از پایان بوق بحرانی تک‌باره BatteryRun مقدار TRUE دارد.
 */
static bool BOOL__G__UiBatteryCriticalBeepCompleted = false;

/**
 * @brief  [EN] RTOS tick at which the critical BatteryRun beep started.
 *         [FA] تیک RTOS در زمان شروع بوق بحرانی BatteryRun.
 */
static uint32_t TICKTYPE_T__G__UiBatteryCriticalBeepStartTick = 0;

/* ==================== BatteryRun green blink state / وضعیت چشمک سبز BatteryRun ==================== */

/**
 * @brief  [EN] Current green LED phase in the non-blocking BatteryRun blink.
 *         [FA] فاز فعلی LED سبز در چشمک غیرمسدودکننده BatteryRun.
 */
static bool BOOL__G__UiBatteryGreenOn = false;

/**
 * @brief  [EN] TRUE after the BatteryRun green blink phase has been initialized.
 *         [FA] بعد از مقداردهی فاز چشمک سبز BatteryRun مقدار TRUE دارد.
 */
static bool BOOL__G__UiBatteryGreenBlinkInitialized = false;

/**
 * @brief  [EN] RTOS tick at which the current BatteryRun green phase started.
 *         [FA] تیک RTOS در زمان شروع فاز فعلی LED سبز BatteryRun.
 */
static uint32_t TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = 0;

/**
 * @brief  [EN] Stored BatteryRun green ON duration used to restart phase timing when percentage changes.
 *         [FA] مدت ذخیره‌شده روشن‌بودن سبز BatteryRun برای شروع مجدد فاز هنگام تغییر درصد.
 */
static uint32_t UINT32_T__G__UiBatteryGreenOnMs = 0u;

/**
 * @brief  [EN] Stored BatteryRun green OFF duration used to restart phase timing when percentage changes.
 *         [FA] مدت ذخیره‌شده خاموش‌بودن سبز BatteryRun برای شروع مجدد فاز هنگام تغییر درصد.
 */
static uint32_t UINT32_T__G__UiBatteryGreenOffMs = 0u;

/* ==================== Battery percent hysteresis state / وضعیت هیسترزیس درصد باتری ==================== */

/**
 * @brief  [EN] Stable battery percent after hysteresis (0..100). Used for BatteryRun blink timing and buzzer bands.
 *         Raw percent is from func__Ui_BatteryVoltageToPercent; stable changes only with 2% hysteresis and special 0/1 handling.
 *         [FA] درصد پایدار باتری پس از هیسترزیس (۰..۱۰۰). برای زمان‌بندی چشمک و بازه‌های بوق BatteryRun استفاده می‌شود.
 */
static uint8_t UINT8_T__G__UiBatteryStablePercent = 0u;

/**
 * @brief  [EN] TRUE after stable percent has been initialized from the first valid raw percent.
 *         [FA] بعد از مقداردهی اولیه درصد پایدار از اولین درصد خام معتبر، TRUE می‌شود.
 */
static bool BOOL__G__UiBatteryStableInitialized = false;

/* ==================== BatteryRun stable percent detailed / درصد پایدار BatteryRun ==================== */

/**
 * @brief  [EN] Alias stable percent for BatteryRun (raw/hysteresis 2% + 0/1).
 *         [FA] درصد پایدار BatteryRun (هیسترزیس 2٪ + ۰/۱).
 */
#define UINT8_T__G__UiBatteryRunStablePercent UINT8_T__G__UiBatteryStablePercent
#define BOOL__G__UiBatteryRunStableInitialized BOOL__G__UiBatteryStableInitialized

/* ==================== Charging stable percent / درصد پایدار شارژ ==================== */

/**
 * @brief  [EN] Stable battery percent for Charging yellow timing (hysteresis 5%).
 *         Raw only updates this when |raw-stable|>=5.
 *         [FA] درصد پایدار برای زمان‌بندی زرد شارژ (هیسترزیس ۵٪).
 */
static uint8_t UINT8_T__G__UiChargingStablePercent = 0u;

/**
 * @brief  [EN] TRUE after Charging stable percent initialized.
 *         [FA] بعد از مقداردهی اولیه درصد پایدار شارژ.
 */
static bool BOOL__G__UiChargingStableInitialized = false;

/* ==================== Charging full hysteresis state / وضعیت هیسترزیس فول شارژ ==================== */

/**
 * @brief  [EN] TRUE while Charging has entered full (InputOk) and stays until raw<95.
 *         [FA] وقتی Charging وارد فول (InputOk) شده تا raw کمتر از ۹۵ شود TRUE می‌ماند.
 */
static bool BOOL__G__UiChargingFullActive = false;

/* ==================== Charging yellow blink state / وضعیت چشمک زرد شارژ ==================== */

/**
 * @brief  [EN] Current yellow LED phase in the non-blocking Charging blink.
 *         [FA] فاز فعلی LED زرد در چشمک غیرمسدودکننده شارژ.
 */
static bool BOOL__G__UiChargingYellowOn = false;

/**
 * @brief  [EN] TRUE after the Charging yellow blink phase has been initialized.
 *         [FA] بعد از مقداردهی فاز چشمک زرد شارژ، TRUE می‌شود.
 */
static bool BOOL__G__UiChargingYellowBlinkInitialized = false;

/**
 * @brief  [EN] RTOS tick at which the current Charging yellow phase started.
 *         [FA] تیک RTOS در زمان شروع فاز فعلی LED زرد شارژ.
 */
static uint32_t TICK_T__G__UiChargingYellowPhaseStartTick = 0u;

/**
 * @brief  [EN] Stored Charging yellow ON duration.
 *         [FA] مدت ذخیره‌شده روشن‌بودن زرد شارژ.
 */
static uint32_t UINT32_T__G__UiChargingYellowOnMs = 0u;

/**
 * @brief  [EN] Stored Charging yellow OFF duration.
 *         [FA] مدت ذخیره‌شده خاموش‌بودن زرد شارژ.
 */
static uint32_t UINT32_T__G__UiChargingYellowOffMs = 0u;

/* ==================== BatteryRun critical beep reset / بازنشانی بوق بحرانی BatteryRun ==================== */

/**
 * @brief  [EN] Reset the one-time critical BatteryRun beep state.
 *         [FA] وضعیت بوق بحرانی تک‌باره BatteryRun را بازنشانی می‌کند.
 */
static void func__Ui_ResetBatteryCriticalBeep(void)
{
    BOOL__G__UiBatteryCriticalBeepActive = false;
    BOOL__G__UiBatteryCriticalBeepCompleted = false;
    TICKTYPE_T__G__UiBatteryCriticalBeepStartTick = 0;
}

/* ==================== BatteryRun green blink reset / بازنشانی چشمک سبز BatteryRun ==================== */

/**
 * @brief  [EN] Reset non-blocking BatteryRun green blink timing.
 *         [FA] زمان‌بندی چشمک غیرمسدودکننده سبز BatteryRun را بازنشانی می‌کند.
 */
static void func__Ui_ResetBatteryRunGreenBlink(void)
{
    BOOL__G__UiBatteryGreenOn = false;
    BOOL__G__UiBatteryGreenBlinkInitialized = false;
    TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = 0;
    UINT32_T__G__UiBatteryGreenOnMs = 0u;
    UINT32_T__G__UiBatteryGreenOffMs = 0u;
}

/* ==================== Battery stable percent helpers / کمک‌های درصد پایدار باتری ==================== */

/**
 * @brief  [EN] Reset the BatteryRun stable percent hysteresis state. Called on UI init, invalid snapshot, or explicit reset.
 *         [FA] وضعیت هیسترزیس درصد پایدار را بازنشانی می‌کند.
 */
static void func__Ui_ResetBatteryStablePercent(void)
{
    UINT8_T__G__UiBatteryStablePercent = 0u;
    BOOL__G__UiBatteryStableInitialized = false;
}

/**
 * @brief  [EN] Update stable battery percent from raw percent with 2% hysteresis and special 0/1 handling.
 *         General: |raw-stable| <2 → keep stable; >=2 → stable = raw. Special: stable 0 stays 0 until raw>=2 then →1; stable 1: raw==0→0, raw>=3→2, else keep 1.
 *         [FA] درصد پایدار را از درصد خام با هیسترزیس ۲٪ و رفتار خاص ۰/۱ به‌روز می‌کند.
 * @param  uint8_t__rawPercent [EN] Raw percent 0..100 / درصد خام
 * @return uint8_t [EN] Stable percent after hysteresis / درصد پایدار
 */
static uint8_t func__Ui_UpdateBatteryStablePercent(uint8_t uint8_t__rawPercent)
{
    uint8_t uint8_t__stablePercent;
    uint8_t uint8_t__diffPercent;

    if (BOOL__G__UiBatteryStableInitialized == false)
    {
        UINT8_T__G__UiBatteryStablePercent = uint8_t__rawPercent;
        BOOL__G__UiBatteryStableInitialized = true;
        return UINT8_T__G__UiBatteryStablePercent;
    }

    uint8_t__stablePercent = UINT8_T__G__UiBatteryStablePercent;

    if (uint8_t__stablePercent == 0u)
    {
        if (uint8_t__rawPercent >= UI_BATTERY_ZERO_EXIT_THRESHOLD)
        {
            uint8_t__stablePercent = 1u;
        }
        else
        {
            /* [EN] Keep 0 until raw reaches 2.
               [FA] تا raw به ۲ نرسیده روی ۰ بماند. */
        }
    }
    else if (uint8_t__stablePercent == 1u)
    {
        if (uint8_t__rawPercent == 0u)
        {
            uint8_t__stablePercent = 0u;
        }
        else if (uint8_t__rawPercent >= UI_BATTERY_ONE_EXIT_THRESHOLD)
        {
            uint8_t__stablePercent = 2u;
        }
        else
        {
            /* [EN] Keep 1 for raw 1..2.
               [FA] برای raw ۱ تا ۲ روی ۱ بماند. */
        }
    }
    else
    {
        if (uint8_t__rawPercent > uint8_t__stablePercent)
        {
            uint8_t__diffPercent = uint8_t__rawPercent - uint8_t__stablePercent;
        }
        else
        {
            uint8_t__diffPercent = uint8_t__stablePercent - uint8_t__rawPercent;
        }

        if (uint8_t__diffPercent >= UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT)
        {
            uint8_t__stablePercent = uint8_t__rawPercent;
        }
        else
        {
            /* [EN] Jitter <2% keeps BatteryRun stable, e.g., 56↔57, 57↔58 preserves 57.
               [FA] نوسان کمتر از ۲٪، پایدار BatteryRun را نگه می‌دارد. */
        }
    }

    UINT8_T__G__UiBatteryStablePercent = uint8_t__stablePercent;
    return uint8_t__stablePercent;
}

/**
 * @brief  [EN] Reset Charging stable percent (5% hysteresis).
 *         [FA] درصد پایدار شارژ را بازنشانی می‌کند.
 */
static void func__Ui_ResetChargingStablePercent(void)
{
    UINT8_T__G__UiChargingStablePercent = 0u;
    BOOL__G__UiChargingStableInitialized = false;
}

/**
 * @brief  [EN] Update Charging stable percent with 5% hysteresis. |raw-stable| <5 keeps stable, >=5 updates.
 *         [FA] درصد پایدار شارژ را با هیسترزیس ۵٪ به‌روز می‌کند.
 * @param  uint8_t__rawPercent [EN] Raw percent 0..100
 * @return uint8_t stable
 */
static uint8_t func__Ui_UpdateChargingStablePercent(uint8_t uint8_t__rawPercent)
{
    uint8_t uint8_t__stablePercent;
    uint8_t uint8_t__diffPercent;

    if (BOOL__G__UiChargingStableInitialized == false)
    {
        UINT8_T__G__UiChargingStablePercent = uint8_t__rawPercent;
        BOOL__G__UiChargingStableInitialized = true;
        return UINT8_T__G__UiChargingStablePercent;
    }

    uint8_t__stablePercent = UINT8_T__G__UiChargingStablePercent;

    if (uint8_t__rawPercent > uint8_t__stablePercent)
    {
        uint8_t__diffPercent = uint8_t__rawPercent - uint8_t__stablePercent;
    }
    else
    {
        uint8_t__diffPercent = uint8_t__stablePercent - uint8_t__rawPercent;
    }

    if (uint8_t__diffPercent >= UI_CHARGING_PERCENT_HYSTERESIS_PERCENT)
    {
        uint8_t__stablePercent = uint8_t__rawPercent;
    }
    else
    {
        /* [EN] Jitter <5% keeps charging stable, e.g., 57 with 53..61 stays 57.
           [FA] نوسان کمتر از ۵٪ پایدار شارژ را نگه می‌دارد. */
    }

    UINT8_T__G__UiChargingStablePercent = uint8_t__stablePercent;
    return uint8_t__stablePercent;
}

/**
 * @brief  [EN] Reset Charging full (InputOk) hysteresis state.
 *         [FA] وضعیت هیسترزیس فول شارژ را بازنشانی می‌کند.
 */
static void func__Ui_ResetChargingFullHysteresis(void)
{
    BOOL__G__UiChargingFullActive = false;
}

/**
 * @brief  [EN] Update Charging full hysteresis. Enter InputOk only at raw 100, stay until raw<95.
 *         [FA] هیسترزیس ورود/خروج InputOk را به‌روز می‌کند.
 * @param  uint8_t__rawPercent [EN] Raw percent
 * @return bool true = InputOk (full), false = Charging
 */
static bool func__Ui_UpdateChargingFullHysteresis(uint8_t uint8_t__rawPercent)
{
    if (BOOL__G__UiChargingFullActive == true)
    {
        if (uint8_t__rawPercent < UI_CHARGING_FULL_EXIT_PERCENT)
        {
            BOOL__G__UiChargingFullActive = false;
        }
        else
        {
            /* [EN] Keep InputOk until <95.
               [FA] تا کمتر از ۹۵ در InputOk بمان. */
        }
    }
    else
    {
        if (uint8_t__rawPercent >= UI_CHARGING_FULL_ENTER_PERCENT)
        {
            BOOL__G__UiChargingFullActive = true;
        }
        else
        {
            /* [EN] Stay Charging until 100.
               [FA] تا ۱۰۰ در Charging بمان. */
        }
    }

    return BOOL__G__UiChargingFullActive;
}

/* ==================== Charging yellow blink helpers / کمک‌های چشمک زرد شارژ ==================== */

/**
 * @brief  [EN] Reset Charging yellow blink phase.
 *         [FA] فاز چشمک زرد شارژ را بازنشانی می‌کند.
 */
static void func__Ui_ResetChargingYellowBlink(void)
{
    BOOL__G__UiChargingYellowOn = false;
    BOOL__G__UiChargingYellowBlinkInitialized = false;
    TICK_T__G__UiChargingYellowPhaseStartTick = 0u;
    UINT32_T__G__UiChargingYellowOnMs = 0u;
    UINT32_T__G__UiChargingYellowOffMs = 0u;
}

/**
 * @brief  [EN] Update non-blocking Charging yellow blink. Preserves phase on duration change.
 *         [FA] چشمک غیرمسدودکننده زرد شارژ را به‌روز می‌کند و فاز را حفظ می‌کند.
 * @param  uint32_t__yellowOnMs [EN] Yellow ON duration / مدت روشن‌بودن
 * @param  uint32_t__yellowOffMs [EN] Yellow OFF duration / مدت خاموش‌بودن
 */
static void func__Ui_UpdateChargingYellowBlink(uint32_t uint32_t__yellowOnMs, uint32_t uint32_t__yellowOffMs)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__currentPhaseMs;

    ticktype__nowTick = osKernelGetTickCount();

    if (BOOL__G__UiChargingYellowBlinkInitialized == false)
    {
        BOOL__G__UiChargingYellowBlinkInitialized = true;
        BOOL__G__UiChargingYellowOn = true;
        TICK_T__G__UiChargingYellowPhaseStartTick = ticktype__nowTick;
        UINT32_T__G__UiChargingYellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__UiChargingYellowOffMs = uint32_t__yellowOffMs;
    }
    else if ((UINT32_T__G__UiChargingYellowOnMs != uint32_t__yellowOnMs) ||
             (UINT32_T__G__UiChargingYellowOffMs != uint32_t__yellowOffMs))
    {
        /* [EN] Duration changed: keep current phase and start tick, only update stored durations.
             New timing takes effect from next phase boundary.
           [FA] مدت تغییر کرد: فاز فعلی و تیک شروع حفظ شود، فقط مدت ذخیره به‌روز شود. */
        UINT32_T__G__UiChargingYellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__UiChargingYellowOffMs = uint32_t__yellowOffMs;
    }
    else
    {
        uint32_t__currentPhaseMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICK_T__G__UiChargingYellowPhaseStartTick);

        if ((BOOL__G__UiChargingYellowOn == true) &&
            (uint32_t__currentPhaseMs >= uint32_t__yellowOnMs))
        {
            BOOL__G__UiChargingYellowOn = false;
            TICK_T__G__UiChargingYellowPhaseStartTick = ticktype__nowTick;
        }
        else if ((BOOL__G__UiChargingYellowOn == false) &&
                 (uint32_t__currentPhaseMs >= uint32_t__yellowOffMs))
        {
            BOOL__G__UiChargingYellowOn = true;
            TICK_T__G__UiChargingYellowPhaseStartTick = ticktype__nowTick;
        }
        else
        {
            /* [EN] Keep current yellow phase until duration expires.
               [FA] فاز فعلی زرد را تا پایان مدت حفظ کن. */
        }
    }

    func__yellow(BOOL__G__UiChargingYellowOn);
}

/* ==================== BatteryRun green blink update / به‌روزرسانی چشمک سبز BatteryRun ==================== */

/**
 * @brief  [EN] Update the non-blocking BatteryRun green blink and service its phase timing.
 *         [FA] چشمک غیرمسدودکننده سبز BatteryRun و زمان‌بندی فاز آن را به‌روز می‌کند.
 * @param  uint32_t__greenOnMs [EN] Green ON duration / مدت روشن‌بودن سبز
 * @param  uint32_t__greenOffMs [EN] Green OFF duration / مدت خاموش‌بودن سبز
 */
static void func__Ui_UpdateBatteryRunGreenBlink(uint32_t uint32_t__greenOnMs, uint32_t uint32_t__greenOffMs)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__currentPhaseMs;

    ticktype__nowTick = osKernelGetTickCount();

    if (BOOL__G__UiBatteryGreenBlinkInitialized == false)
    {
        BOOL__G__UiBatteryGreenBlinkInitialized = true;
        BOOL__G__UiBatteryGreenOn = true;
        TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        UINT32_T__G__UiBatteryGreenOnMs = uint32_t__greenOnMs;
        UINT32_T__G__UiBatteryGreenOffMs = uint32_t__greenOffMs;
    }
    else if ((UINT32_T__G__UiBatteryGreenOnMs != uint32_t__greenOnMs) ||
             (UINT32_T__G__UiBatteryGreenOffMs != uint32_t__greenOffMs))
    {
        /* [EN] Duration changed due to stable percent jitter: keep current phase and start tick, only update stored durations.
             New timing applies from next phase boundary, so a 10ms jitter does not restart the green ON.
           [FA] مدت به خاطر نوسان درصد پایدار تغییر کرد: فاز و تیک شروع حفظ شود، فقط مدت ذخیره به‌روز شود. */
        UINT32_T__G__UiBatteryGreenOnMs = uint32_t__greenOnMs;
        UINT32_T__G__UiBatteryGreenOffMs = uint32_t__greenOffMs;
    }
    else
    {
        uint32_t__currentPhaseMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__UiBatteryGreenPhaseStartTick);

        if ((BOOL__G__UiBatteryGreenOn == true) &&
            (uint32_t__currentPhaseMs >= uint32_t__greenOnMs))
        {
            BOOL__G__UiBatteryGreenOn = false;
            TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        }
        else if ((BOOL__G__UiBatteryGreenOn == false) &&
                 (uint32_t__currentPhaseMs >= uint32_t__greenOffMs))
        {
            BOOL__G__UiBatteryGreenOn = true;
            TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        }
        else
        {
            /* [EN] Keep the current LED phase until its configured duration expires.
               [FA] فاز فعلی LED را تا پایان مدت تنظیم‌شده حفظ کن. */
        }
    }

    func__green(BOOL__G__UiBatteryGreenOn);
}

/* ==================== Input state update / به‌روزرسانی وضعیت ورودی ==================== */

/**
 * @brief  [EN] Update input presence and input overvoltage state with hysteresis.
 *         Connected: input >= 21V. Disconnected: input <= 20V (fixed wiring band).
 *         Overvoltage enters above id 70 and clears at or below 70-71 (defaults 28V / 27V).
 *         [FA] وضعیت اتصال و خطای اضافه‌ولتاژ ورودی را با هیسترزیس به‌روز می‌کند.
 *         وصل: ورودی حداقل ۲۱ ولت. قطع: ورودی حداکثر ۲۰ ولت (باند ثابت).
 *         خطا: بالاتر از ۷۰ فعال و در ۷۰−۷۱ یا پایین‌تر پاک می‌شود (پیش‌فرض ۲۸/۲۷ ولت).
 * @param  uint32_t__inputVoltageMv [EN] Input voltage in mV / ولتاژ ورودی بر حسب میلی‌ولت
 */
static void func__Ui_UpdateInputState(uint32_t uint32_t__inputVoltageMv)
{
    /* [EN] v1.16: the overvoltage threshold (id 70) and its hysteresis
       (id 71) are runtime; clear = thresh - hyst, no underflow (71 <=
       2000 < 24000 <= 70 by clamp). The 21 V / 20 V input-present band
       stays a fixed wiring constant.
       [FA] آستانه/هیسترزیس اضافه‌ولتاژ زمان‌اجرا (۷۰/۷۱)؛ باند اتصال
       ورودی ثابت می‌ماند. */
    if (BOOL__G__UiInputOverVoltage == false)
    {
        if (uint32_t__inputVoltageMv > UI_ALARM_T__G__Alarm.uint32_t__ovThreshMv)
        {
            BOOL__G__UiInputOverVoltage = true;
            TICKTYPE_T__G__UiInputOverVoltageStartTick = osKernelGetTickCount();
        }
    }
    else if (uint32_t__inputVoltageMv <=
             (UI_ALARM_T__G__Alarm.uint32_t__ovThreshMv -
              UI_ALARM_T__G__Alarm.uint32_t__ovHystMv))
    {
        BOOL__G__UiInputOverVoltage = false;
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    else
    {
        /* [EN] Keep the overvoltage error in the 27V..28V hysteresis band.
           [FA] خطای اضافه‌ولتاژ را در بازه هیسترزیس ۲۷ تا ۲۸ ولت حفظ کن. */
    }

    if (BOOL__G__UiInputPresent == false)
    {
        if (uint32_t__inputVoltageMv >= UI_INPUT_CONNECTED_THRESHOLD_MV)
        {
            BOOL__G__UiInputPresent = true;
        }
    }
    else if (uint32_t__inputVoltageMv <= UI_INPUT_DISCONNECTED_THRESHOLD_MV)
    {
        BOOL__G__UiInputPresent = false;
    }
    else
    {
        /* [EN] Keep the previous connected state in the 20V..21V band.
           [FA] وضعیت قبلی اتصال را در بازه ۲۰ تا ۲۱ ولت حفظ کن. */
    }
}

/* ==================== Scenario Input Overvoltage / سناریوی اضافه‌ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Display input overvoltage: green steady, yellow off, red blink (38/39)
 *         and the periodic beep pattern (40..43). Defaults: red 50%, one 1-second
 *         pulse every 10 seconds. This tick is non-blocking.
 *         [FA] نمایش اضافه‌ولتاژ ورودی: سبز ثابت، زرد خاموش، قرمز چشمک (۳۸/۳۹)
 *         و الگوی بوق دوره‌ای (۴۰..۴۳). پیش‌فرض: قرمز ۵۰٪ و یک بوق یک‌ثانیه‌ای
 *         هر ۱۰ ثانیه. این تیک غیرمسدودکننده است.
 */
static void func__Ui_ScenarioInputOverVoltage_Tick(void)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__phaseMs;
    uint32_t uint32_t__redOnMs;
    uint64_t uint64_t__redDutyProduct;
    bool bool__redOn;

    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();

    ticktype__nowTick = osKernelGetTickCount();
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__UiInputOverVoltageStartTick);
    uint32_t__phaseMs = uint32_t__elapsedMs % UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs;

    uint64_t__redDutyProduct = (uint64_t)UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs *
                               UI_ALARM_T__G__Alarm.uint32_t__ovLedDutyPct;
    uint32_t__redOnMs = (uint32_t)(uint64_t__redDutyProduct / UI_PERCENT_SCALE);
    bool__redOn = (uint32_t__phaseMs < uint32_t__redOnMs);

    func__green(true);
    func__yellow(false);
    func__red(bool__redOn);

    (void)func__Ui_Buzzer_Gated(
        UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs,
        func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount,
                                 UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs),
        (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount,
        UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs);
}

/* ==================== Scenario BatLost Tick / تیک سناریوی قطع باتری ==================== */

/**
 * @brief  [EN] Battery-lost announcement: red fast blink (defaults 50% of a 1 s period,
 *         distinctly unlike the slow overvoltage pulse; ids 44/45), green steady since
 *         the input is present in both detection cases, and the periodic
 *         buzzer pattern (defaults: three short beeps plus a pause; ids 46..49). The pattern phase uses
 *         the absolute kernel tick so no state needs remembering here; the
 *         fault bit itself is owned (set and cleared) only by the Fault
 *         module. Must be called every Ui pass while active so the buzzer
 *         pattern advances.
 *         [FA] اعلان قطع باتری: قرمز چشمک‌تند (پیش‌فرض ۵۰٪ در دوره یک‌ثانیه؛ ۴۴/۴۵)،
 *         سبز ثابت (ورودی حاضر است) و الگوی بوق دوره‌ای (پیش‌فرض سه بیپ کوتاه + مکث؛ ۴۶..۴۹).
 *         فاز از تیک مطلق
 *         گرفته می‌شود تا وضعیتی لازم نباشد؛ خود پرچم فقط در Fault مدیریت
 *         می‌شود. تا وقتی فعال است هر پاس صدا زده شود.
 */
void func__Ui_ScenarioBatLost_Tick(void)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__phaseMs;
    bool     bool__redOn;

    /* [EN] Foreign blink/beep states belong to other scenarios; stop them so
       nothing from BatteryRun/Charging bleeds into this pattern.
       [FA] وضعیت‌های چشمک/بوق سناریوهای دیگر ریست شود تا اثری از آن‌ها در
       این الگو نیفتد. */
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();

    uint32_t__nowTick   = osKernelGetTickCount();
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick);
    uint32_t__phaseMs   = uint32_t__elapsedMs % UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs;

    /* [EN] First duty% of the period = ON (default 0..50% -> 500/500 ms
       fast blink); v1.16 reads the live period/duty (ids 44/45).
       [FA] duty٪ اول دوره روشن؛ نسخه ۱.۱۶ دوره/دیوتی زنده (۴۴/۴۵). */
    bool__redOn = (uint32_t__phaseMs <
                   ((UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs *
                     UI_ALARM_T__G__Alarm.uint32_t__blLedDutyPct) /
                    UI_PERCENT_SCALE));

    func__green(true);
    func__yellow(false);
    func__red(bool__redOn);

    (void)func__Ui_Buzzer_Gated(
        UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs,
        func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__blBeepCount,
                                 UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs),
        (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__blBeepCount,
        UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs);
}

/* ==================== Scenario InputOk / سناریوی ورودی عادی ==================== */

/**
 * @brief  [EN] InputOk scenario: green steady, red/yellow off, and buzzer off.
 *         [FA] سناریو ورودی وصل: سبز ثابت، قرمز و زرد خاموش و بوق خاموش.
 */
void func__Ui_ScenarioInputOk(void)
{
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();

    func__green(true);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);

    /* [EN] InputOk is steady green, no blink — return immediately. The UI task's 10ms loop provides the poll period,
         so a 500ms blocking delay inside the scenario is not needed and would slow UI reaction.
       [FA] InputOk سبز ثابت است؛ بدون چشمک و بدون تاخیر مسدودکننده. حلقه ۱۰ms تسک، دوره polling را می‌دهد. */
}

/* ==================== Scenario Charging Tick / تیک سناریوی شارژ ==================== */

/**
 * @brief  [EN] Charging scenario tick: green steady, yellow shows remaining to full non-linear.
 *         Formula: remainingPercent = 100-pct, periodPerPercent = period/100, yellowOnMs = remaining*periodPer, yellowOffMs = period-yellowOn.
 *         [FA] سناریو شارژ: سبز ثابت، زرد مانده تا فول غیرخطی.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, percent map 74/75 / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__rawPercent;
    uint8_t uint8_t__stablePercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__yellowOnMs;
    uint32_t uint32_t__yellowOffMs;

    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);

    /* [EN] Charging yellow timing uses hysteresis 5%: stable only moves when |raw-stable|>=5. v_bat is PA3 / v_bat24_mv.
       [FA] زمان‌بندی زرد شارژ با هیسترزیس ۵٪: پایدار فقط وقتی اختلاف حداقل ۵ باشد به‌روز می‌شود. */
    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);
    uint8_t__stablePercent = func__Ui_UpdateChargingStablePercent(uint8_t__rawPercent);

    func__green(true);
    func__red(false);

    if (uint8_t__stablePercent >= UI_PERCENT_FULL)
    {
        func__Ui_ResetChargingYellowBlink();
        func__yellow(false);
        return;
    }

    if (uint8_t__stablePercent == 0u)
    {
        func__Ui_ResetChargingYellowBlink();
        func__yellow(true);
        return;
    }

    /* [EN] Non-linear formula with chargingStablePercent: the REMAINING to
       full drives yellow ON time (final user directive 2026-09-19): the more
       charged the battery, the SHORTER the yellow ON, so with 5% remaining
       it blinks 50 ms per 1000 ms and a nearly empty battery keeps yellow
       almost fully ON.
       [FA] فرمول غیرخطی با درصد پایدار شارژ: «مانده تا فول» زمان روشن‌بودن
       زرد را می‌دهد (دستور نهایی کاربر): هرچه باتری پرتر، روشن‌بودن زرد
       کوتاه‌تر؛ با ۵٪ مانده، ۵۰ms از ۱۰۰۰ms چشمک می‌زند و باتری خالی زرد را
       تقریباً دائم روشن نگه می‌دارد. */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__stablePercent;
    uint32_t__periodPerPercent = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs / UI_PERCENT_SCALE;
    uint32_t__yellowOnMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__yellowOnMs < UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs)
    {
        uint32_t__yellowOnMs = UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs;
    }
    if (uint32_t__yellowOnMs > UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs)
    {
        uint32_t__yellowOnMs = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs;
    }

    uint32_t__yellowOffMs = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs - uint32_t__yellowOnMs;

    func__Ui_UpdateChargingYellowBlink(uint32_t__yellowOnMs, uint32_t__yellowOffMs);
}

/* ==================== Scenario BatteryRun Tick / تیک سناریوی دشارژ ==================== */

/**
 * @brief  [EN] BatteryRun scenario: green blink follows the runtime 74/75 percent map;
 *         buzzer warnings use the four percentage bands (50..53).
 *         Below the crit band (53) all LEDs turn off and the critical pattern (56/57/58/65)
 *         plays once for the latch length (61), then silence until the battery recovers.
 *         [FA] سناریو دشارژ: سبز بر اساس نگاشت درصد ۷۴/۷۵ چشمک می‌زند؛
 *         بوق بر اساس چهار بازه درصدی (۵۰..۵۳) اجرا می‌شود.
 *         زیر باند بحرانی (۵۳) همهٔ LEDها خاموش و الگوی بحرانی (۵۶/۵۷/۵۸/۶۵) فقط یک‌بار
 *         به‌اندازهٔ طول یک‌باره (۶۱) پخش می‌شود، بعد سکوت تا برگشت باتری.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, percent map 74/75 / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__rawPercent;
    uint8_t uint8_t__stablePercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__greenOffMs;
    uint32_t uint32_t__greenOnMs;
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__criticalElapsedMs;

    func__Ui_ResetChargingYellowBlink();

    /* [EN] Step 1: raw percent from PA3 / ADC1_IN3 / v_bat24_mv.
       [FA] گام ۱: درصد خام از PA3. */
    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    /* [EN] Step 2: stable percent with 2% hysteresis and special 0/1 handling.
       This hysteresis is only for BatteryRun display/timing, not for input/overvoltage/Low Alarm.
       [FA] گام ۲: درصد پایدار با هیسترزیس ۲٪ و رفتار خاص ۰/۱ (فقط BatteryRun). */
    uint8_t__stablePercent = func__Ui_UpdateBatteryStablePercent(uint8_t__rawPercent);

    /* [EN] Critical 0% uses stable percent: one 10s beep, then LEDs off until battery exits critical area.
       Noise 0<->1 does not restart the beep because stable 0 stays 0 until raw>=2.
       [FA] ۰٪ بحرانی با درصد پایدار: یک بوق ۱۰ ثانیه‌ای فقط یک بار، سپس LED خاموش تا خروج از ناحیه بحرانی. */
    if (uint8_t__stablePercent < (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct)
    {
        func__Ui_ResetBatteryRunGreenBlink();
        func__green(false);
        func__red(false);
        func__yellow(false);

        if (BOOL__G__UiBatteryCriticalBeepCompleted == true)
        {
            (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
            return;
        }

        if (BOOL__G__UiBatteryCriticalBeepActive == false)
        {
            BOOL__G__UiBatteryCriticalBeepActive = true;
            TICKTYPE_T__G__UiBatteryCriticalBeepStartTick = osKernelGetTickCount();
        }

        ticktype__nowTick = osKernelGetTickCount();
        uint32_t__criticalElapsedMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__UiBatteryCriticalBeepStartTick);

        if (uint32_t__criticalElapsedMs >= UI_ALARM_T__G__Alarm.uint32_t__runCritDurMs)
        {
            (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
            BOOL__G__UiBatteryCriticalBeepActive = false;
            BOOL__G__UiBatteryCriticalBeepCompleted = true;
            return;
        }

        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs,
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runCritDutyPct,
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runCritCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
        return;
    }

    func__Ui_ResetBatteryCriticalBeep();

    /* [EN] Non-linear: green blink OFF = remaining * period/100, with min. Input is stablePercent.
       [FA] فرمول غیرخطی سبز چشمک: خاموشی برابر مانده درصد پایدار ضربدر دوره است. */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__stablePercent;
    uint32_t__periodPerPercent = UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs / UI_PERCENT_SCALE;
    uint32_t__greenOffMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__greenOffMs < UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs)
    {
        uint32_t__greenOffMs = UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs;
    }

    uint32_t__greenOnMs = UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs - uint32_t__greenOffMs;

    if (uint8_t__stablePercent >= (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct)
    {
        (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
    }
    else if (uint8_t__stablePercent >= (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct)
    {
        /* [EN] v1.16: duty from the live window (dur/count/gap), shared
           standard duration (id 59) + band count (id 62).
           [FA] دیوتی از پنجرهٔ زنده. */
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runStdCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runStdCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
    }
    else if (uint8_t__stablePercent >= (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct)
    {
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
    }
    else
    {
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runTriCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runTriCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
    }

    func__red(false);
    func__yellow(false);
    func__Ui_UpdateBatteryRunGreenBlink(uint32_t__greenOnMs, uint32_t__greenOffMs);
}

/* ==================== Ui Tick / تیک اصلی UI ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario from the real Measurement snapshot.
 *         The snapshot is obtained via func__Measurement_GetSnapshot(); valid is checked
 *         before any decision. If invalid, UI enters safe-off, alarm flag is cleared and
 *         no stale/manual values are used. Battery production source is snapshot.v_bat24_mv only.
 *         [FA] تیک اصلی UI - تصمیم سناریو را از snapshot واقعی Measurement می‌گیرد.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL / اشاره‌گر snapshot
 */
void func__Ui_Tick(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__batteryVoltageMv;
    uint32_t uint32_t__batteryClampedMv;
    uint8_t uint8_t__rawPercent;
    bool bool__snapshotValid;
    bool bool__isFull;

    if (measurement_snapshot_t__snap == NULL)
    {
        func__all_off();
        BOOL__G__UiBatteryAlarmIssued = false;
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetBatteryCriticalBeep();
        func__Ui_ResetBatteryRunGreenBlink();
        func__Ui_ResetChargingYellowBlink();
        func__Ui_ResetBatteryStablePercent();
        func__Ui_ResetChargingStablePercent();
        func__Ui_ResetChargingFullHysteresis();
        return;
    }

    bool__snapshotValid = measurement_snapshot_t__snap->valid;

    if (bool__snapshotValid == false)
    {
        func__all_off();
        BOOL__G__UiBatteryAlarmIssued = false;
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetBatteryCriticalBeep();
        func__Ui_ResetBatteryRunGreenBlink();
        func__Ui_ResetChargingYellowBlink();
        /* [EN] BatteryRun and Charging stable percents keep previous values on invalid but phases reset; full hysteresis keeps state to avoid flicker on noisy valid->invalid.
           Battery voltage source is PA3 / v_bat24_mv only; do not use v_in here.
           [FA] درصدهای پایدار مقدار قبلی را نگه می‌دارند اما فازها ریست می‌شوند. */
        return;
    }

    uint32_t__inputVoltageMv = measurement_snapshot_t__snap->v_in_mv;
    uint32_t__batteryVoltageMv = measurement_snapshot_t__snap->v_bat24_mv;

    /* [EN] Update global low-battery alarm flag continuously with hysteresis.
       Threshold <21000 sets true, >=21200 clears false, otherwise hold.
       [FA] فلگ سراسری آلارم باتری کم را به‌صورت پیوسته با هیسترزیس به‌روز کن. */
    if (uint32_t__batteryVoltageMv < UI_ALARM_T__G__Alarm.uint32_t__lowBatThreshMv)
    {
        BOOL__G__UiBatteryAlarmIssued = true;
    }
    else if (uint32_t__batteryVoltageMv >= UI_ALARM_T__G__Alarm.uint32_t__lowBatClearMv)
    {
        BOOL__G__UiBatteryAlarmIssued = false;
    }
    else
    {
        /* [EN] Keep previous flag in hysteresis band 21000..21200.
           [FA] فلگ قبلی را در بازه هیسترزیس حفظ کن. */
    }

    func__Ui_UpdateInputState(uint32_t__inputVoltageMv);

    if (BOOL__G__UiInputOverVoltage == true)
    {
        func__Ui_ScenarioInputOverVoltage_Tick();
        return;
    }

#if MODULE_FAULT
    /* [EN] Battery-lost, priority 2 (overvoltage first, this second, normal
       scenarios after). The Fault module latches and clears the bit; while it
       is set we a) show this scenario and b) return, so the BatteryRun
       critical beep (input-absent world) can never overlap with this pattern
       (input-present world). When the battery is back and the settle time
       passed, the bit clears and the previous scenario resumes by itself.
       [FA] قطع باتری با اولویت دوم (بعد از اضافه‌ولتاژ، قبل از سناریوهای
       نرمال). فقط تا وقتی پرچم متمرکز قفل است نشان می‌دهیم و return می‌کنیم
       تا هرگز با بوق بحرانی دشارژ قاطی نشود؛ با پاک‌شدن پرچم، سناریوی قبلی
       خودبه‌خود برمی‌گردد. */
    if ((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE)
    {
        func__Ui_ScenarioBatLost_Tick();
        return;
    }
#endif

    uint32_t__batteryClampedMv = uint32_t__batteryVoltageMv;
    if (uint32_t__batteryClampedMv > UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv)
    {
        uint32_t__batteryClampedMv = UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv;
    }

    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryClampedMv);
    bool__isFull = func__Ui_UpdateChargingFullHysteresis(uint8_t__rawPercent);

    if (BOOL__G__UiInputPresent == true)
    {
        if (bool__isFull == true)
        {
            /* [EN] Full hysteresis: entered at 100, stays InputOk until <95.
               [FA] هیسترزیس فول: ورود در ۱۰۰، ماندن تا کمتر از ۹۵. */
            func__Ui_ScenarioInputOk();
        }
#if MODULE_CHARGER
        else if (func__Charger_IsAnyChannelActive() == false)
        {
            /* [EN] Not full and NO channel charging (OFF / JIT retry /
               input wait / final fault / battery-lost): the yellow charge
               blink must NOT run - the user wants it only while the charger
               module really works (channel 1, channel 2 or both). Green
               steady stays as the honest "input ok" face instead.
               [FA] نه فول و نه هیچ کانالِ شارژِ فعال: چشمک زرد نشان داده
               نمی‌شود - کاربر خواسته چشمک فقط وقتی شارژر واقعاً کار کند؛
               سبز ثابت به‌جای آن. */
            func__Ui_ScenarioInputOk();
        }
#endif
        else
        {
            func__Ui_ScenarioCharging_Tick(uint32_t__batteryClampedMv);
        }
    }
    else
    {
        /* [EN] Input disconnected → BatteryRun uses 2% hysteresis + 0/1.
           [FA] ورودی قطع → BatteryRun با هیسترزیس ۲٪ + ۰/۱. */
        func__Ui_ScenarioBatteryRun_Tick(uint32_t__batteryClampedMv);
    }
}

/* ==================== Ui Init / مقداردهی اولیه UI ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void)
{
    func__all_off();
    BOOL__G__UiBatteryAlarmIssued = false;
    BOOL__G__UiInputPresent = false;
    BOOL__G__UiInputOverVoltage = false;
    TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();
    func__Ui_ResetBatteryStablePercent();
    func__Ui_ResetChargingStablePercent();
    func__Ui_ResetChargingFullHysteresis();
}

/* ==================== Board Test Start / شروع تست برد ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous 150ms-style buzzer check.
 *         The buzzer uses the new periodic API with a safe period and then is explicitly turned off.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز و بوق کوتاه قبلی.
 *         بوق با API دوره‌ای جدید و دوره امن اجرا و سپس صریحاً خاموش می‌شود.
 */
void func__Ui_BoardTest_Start(void)
{
    uint32_t uint32_t__beepPeriodMs;
    uint32_t uint32_t__beepDutyPercent;
    uint64_t uint64_t__beepDutyProduct;
    int32_t int32_t__buzzerResult;

    func__all_off();
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();
    func__Ui_ResetBatteryStablePercent();
    func__Ui_ResetChargingStablePercent();
    func__Ui_ResetChargingFullHysteresis();

    func__red(true);
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_selftest_led_ms);
    func__red(false);

    func__yellow(true);
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_selftest_led_ms);
    func__yellow(false);

    func__green(true);
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_selftest_led_ms);
    func__green(false);

    if (APP_CONFIG.ui_boot_beep_ms == 0u)
    {
        return;
    }

    uint32_t__beepPeriodMs = APP_CONFIG.ui_boot_beep_ms;
    if (uint32_t__beepPeriodMs < UI_BUZZER_MIN_PERIOD_MS)
    {
        uint32_t__beepPeriodMs = UI_BUZZER_MIN_PERIOD_MS;
    }

    uint32_t__beepDutyPercent = UI_BUZZER_DUTY_MAX_PERCENT;
    if (APP_CONFIG.ui_boot_beep_ms < uint32_t__beepPeriodMs)
    {
        uint64_t__beepDutyProduct = (uint64_t)APP_CONFIG.ui_boot_beep_ms * UI_BUZZER_PERCENT_SCALE;
        uint32_t__beepDutyPercent = (uint32_t)(uint64_t__beepDutyProduct / uint32_t__beepPeriodMs);
        if ((uint64_t__beepDutyProduct % uint32_t__beepPeriodMs) != 0u)
        {
            uint32_t__beepDutyPercent++;
        }
        if (uint32_t__beepDutyPercent == 0u)
        {
            uint32_t__beepDutyPercent = 1u;
        }
    }

    int32_t__buzzerResult = func__Ui_Buzzer_Tick(
        uint32_t__beepPeriodMs,
        (uint8_t)uint32_t__beepDutyPercent,
        1u,
        0u);

    if (int32_t__buzzerResult != UI_BUZZER_INVALID_RESULT)
    {
        func__Rtos_DelayMilliseconds(APP_CONFIG.ui_boot_beep_ms);
    }

    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
}
