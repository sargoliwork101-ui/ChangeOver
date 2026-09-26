/**
 * @file    fault.c
 * @brief   [EN] Latched fault bits (placeholder). Full type naming, func__ prefix.
 *          [FA] بیت‌های خطای قفل‌شده (اسکلت). نام تایپ کامل.
 */

#include "fault.h"

/* [EN] Install map + active query come from the charger header (one concept,
   one constant: which half is wired/used lives ONLY there). No HAL inside.
   [FA] نقشهٔ نصب کانال‌ها و پرسش «در حال پمپ» از هدر شارژر می‌آید تا مفهوم
   تکثیر نشود. */
#include "charger.h"
#include "cmsis_os2.h"
#include "rtos_time.h"

#include <stddef.h>
#include <stdbool.h>

static fault_mask_t FAULT_MASK_T__G__Mask = FAULT_NONE;

/* [EN] Battery-lost debounce timers: start tick of the current sustained
   condition, 0 = condition not running.
   [FA] تایمرهای دبانس قطع باتری؛ صفر یعنی شرط در جریان نیست. */
static uint32_t UINT32_T__G__BatOverSinceTick    = 0u;
static uint32_t UINT32_T__G__BatAbsentSinceTick  = 0u;
static uint32_t UINT32_T__G__BatHealthySinceTick = 0u;

/* ==================== Runtime alarm thresholds (v1.15) ==================== */
/* [EN] v1.15 (user order 2026-09-26: alarms tab): the eight FAULT_* numbers
 *      as one live struct - ids 27..34, STM32-flash persisted, clamped as a
 *      set on every write. Boot = the macro defaults, so a reflash with an
 *      unreadable NVM record changes no behaviour.
 * [FA] v1.15 (دستور کاربر ۲۰۲۶-۰۹-۲۶: تب آلارم‌ها): هشت عدد FAULT_* در یک
 *      struct زنده - شناسه‌های ۲۷..۳۴، ماندگار در فلش، گیرهٔ مجموعه‌ای با
 *      هر نوشتن. بوت = پیش‌فرض ماکروها. */
static fault_alarm_t FAULT_ALARM_T__G__Alarm =
{
    FAULT_BAT_DISCONNECT_MV,
    FAULT_BAT_DISCONNECT_DEBOUNCE_MS,
    FAULT_BAT_ABSENT_MV,
    FAULT_BATTERY_BACK_MV,
    FAULT_BAT_ABSENT_DEBOUNCE_MS,
    FAULT_BAT_RECOVER_MS,
    FAULT_INPUT_PRESENT_MIN_MV,
    FAULT_INPUT_PRESENT_MAX_MV
};

/* [EN] Interdependency clamps: the disconnect threshold must sit strictly
 *      between the charge band and the OV cutoff (over+50 <= disc <= OV-100)
 *      so the pump rule can neither false-trip on legit absorb voltages nor
 *      die under the validity cut; absent/back keep >= 500 mV hysteresis and
 *      the input window >= 1000 mV. If a transient replay order empties the
 *      disconnect range, the floor wins (no false trips; the OV cutoff still
 *      protects the hardware) and the next profile/OV write re-converges it.
 * [FA] گیره‌های وابستگی: آستانهٔ قطع باید اکیداً بین باند شارژ و قطع OV
 *      باشد تا قانون پمپ نه روی ابزورب سالم فایر کند نه زیر قطع اعتبار
 *      بمیرد؛ غیبت/برگشت ≥۵۰۰mV هیسترزیس و پنجرهٔ ورودی ≥۱۰۰۰mV نگه می‌دارند. */
static void func__Fault_ClampAlarms(void)
{
    uint32_t uint32_t__overMv = FAULT_BAT_DISCONNECT_MV;
    uint32_t uint32_t__ovCutMv = CHG_MAX_VALID_BATTERY_MV;
    uint32_t uint32_t__floorMv;
    uint32_t uint32_t__ceilMv;

    (void)func__Charger_GetProfileParam(CHG_PROFILE_PARAM_ABSORB_OVER_MV,
                                       &uint32_t__overMv);
    (void)func__Charger_GetAlarmParam(CHG_ALARM_PARAM_OV_CUTOFF_MV,
                                      &uint32_t__ovCutMv);

    uint32_t__floorMv = 14000u;
    if ((uint32_t__overMv + 50u) > uint32_t__floorMv)
    {
        uint32_t__floorMv = uint32_t__overMv + 50u;
    }
    uint32_t__ceilMv = 15000u;
    if ((uint32_t__ovCutMv > 100u) &&
        ((uint32_t__ovCutMv - 100u) < uint32_t__ceilMv))
    {
        uint32_t__ceilMv = uint32_t__ovCutMv - 100u;
    }
    if (uint32_t__floorMv > uint32_t__ceilMv)
    {
        uint32_t__ceilMv = uint32_t__floorMv;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv < uint32_t__floorMv)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv = uint32_t__floorMv;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv > uint32_t__ceilMv)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv = uint32_t__ceilMv;
    }

    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs < 50u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs = 50u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs > 1000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs = 1000u;
    }

    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv < 3000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentMv = 3000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv > 8000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentMv = 8000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__backMv < 4000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__backMv = 4000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__backMv > 9000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__backMv = 9000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv >
        (FAULT_ALARM_T__G__Alarm.uint32_t__backMv - 500u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__backMv - 500u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__backMv <
        (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv + 500u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__backMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__absentMv + 500u;
    }

    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs < 100u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs = 100u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs > 5000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs = 5000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs < 100u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs = 100u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs > 5000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs = 5000u;
    }

    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv < 18000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv = 18000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv > 24000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv = 24000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv < 24000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv = 24000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv > 30000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv = 30000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv >
        (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv - 1000u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv - 1000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv <
        (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv + 1000u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv + 1000u;
    }
}

bool func__Fault_SetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue)
{
    switch (uint8_t__paramId)
    {
        case FAULT_ALARM_PARAM_DISCONNECT_MV:
            FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_DISCONNECT_DEB_MS:
            FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_ABSENT_MV:
            FAULT_ALARM_T__G__Alarm.uint32_t__absentMv = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_BACK_MV:
            FAULT_ALARM_T__G__Alarm.uint32_t__backMv = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_ABSENT_DEB_MS:
            FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_RECOVER_MS:
            FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_INPUT_MIN_MV:
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv = uint32_t__value;
            break;
        case FAULT_ALARM_PARAM_INPUT_MAX_MV:
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv = uint32_t__value;
            break;
        default:
            return false;
    }

    func__Fault_ClampAlarms();
    return func__Fault_GetAlarmParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Fault_GetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value)
{
    switch (uint8_t__paramId)
    {
        case FAULT_ALARM_PARAM_DISCONNECT_MV:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv;
            return true;
        case FAULT_ALARM_PARAM_DISCONNECT_DEB_MS:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs;
            return true;
        case FAULT_ALARM_PARAM_ABSENT_MV:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__absentMv;
            return true;
        case FAULT_ALARM_PARAM_BACK_MV:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__backMv;
            return true;
        case FAULT_ALARM_PARAM_ABSENT_DEB_MS:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs;
            return true;
        case FAULT_ALARM_PARAM_RECOVER_MS:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs;
            return true;
        case FAULT_ALARM_PARAM_INPUT_MIN_MV:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv;
            return true;
        case FAULT_ALARM_PARAM_INPUT_MAX_MV:
            *uint32_t__value = FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv;
            return true;
        default:
            return false;
    }
}

void func__Fault_OnSupervisionChange(void)
{
    func__Fault_ClampAlarms();
}

/**
 * @brief  [EN] Shared debounce helper: returns true once the condition has
 *         been continuously true for uint32_t__milliseconds. Call it only
 *         while the condition is true; reset the start tick to 0 when the
 *         condition is false.
 *         [FA] دبانس مشترک: وقتی شرط به‌طور پیوسته به مدت خواسته‌شده برقرار
 *         بود true می‌دهد؛ با false‌شدن شرط، تیک شروع را صفر کنید.
 * @param  uint32_t_ptr__sinceTick [EN] Start tick storage (0 = not running) / محل نگه‌داشت تیک شروع
 * @param  uint32_t__nowTick       [EN] Current kernel tick / تیک فعلی
 * @param  uint32_t__milliseconds  [EN] Required duration / مدت لازم
 * @return bool [EN] true when the duration elapsed / وقتی مدت گذشت true
 */
static bool func__Fault_DebounceDone(uint32_t *uint32_t_ptr__sinceTick,
                                     uint32_t uint32_t__nowTick,
                                     uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__durationTicks;

    uint32_t__durationTicks = func__Rtos_MillisecondsToTicks(uint32_t__milliseconds);

    if (uint32_t__durationTicks == 0u)
    {
        /* [EN] Zero-tick conversion (tick frequency 0): never fire instantly.
           [FA] تبدیل صفر: هیچ‌وقت فوری فعال نشود. */
        return false;
    }

    if (*uint32_t_ptr__sinceTick == 0u)
    {
        *uint32_t_ptr__sinceTick = uint32_t__nowTick;
        return false;
    }

    return ((uint32_t)(uint32_t__nowTick - *uint32_t_ptr__sinceTick) >=
            uint32_t__durationTicks);
}

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
/* ==================== Fault_Init ==================== */

void func__Fault_Init(void)
{
    FAULT_MASK_T__G__Mask = FAULT_NONE;
    UINT32_T__G__BatOverSinceTick    = 0u;
    UINT32_T__G__BatAbsentSinceTick  = 0u;
    UINT32_T__G__BatHealthySinceTick = 0u;
}

/**
 * @brief  [EN] Latch bits (OR).
 *         [FA] بیت‌ها را قفل می‌کند (OR).
 * @param  fault_mask_t__bits [EN] Bits to set / بیت‌هایی که باید قفل شود
 */
/* ==================== Fault_Set ==================== */

void func__Fault_Set(fault_mask_t fault_mask_t__bits)
{
    FAULT_MASK_T__G__Mask |= fault_mask_t__bits;
}

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 * @param  fault_mask_t__bits [EN] Bits to clear / بیت‌هایی که باید پاک شود
 */
/* ==================== Fault_Clear ==================== */

void func__Fault_Clear(fault_mask_t fault_mask_t__bits)
{
    FAULT_MASK_T__G__Mask &= (fault_mask_t)~fault_mask_t__bits;
}

/**
 * @brief  [EN] Return current mask.
 *         [FA] ماسک فعلی را برمی‌گرداند.
 * @return fault_mask_t [EN] Current fault mask / ماسک فعلی
 */
/* ==================== Fault_Get ==================== */

fault_mask_t func__Fault_Get(void)
{
    return FAULT_MASK_T__G__Mask;
}

/**
 * @brief  [EN] True if any bit is set.
 *         [FA] اگر هر بیتی روشن باشد true.
 * @return bool [EN] true if any fault latched / اگر خطایی قفل شده true
 */
/* ==================== Fault_Any ==================== */

bool func__Fault_Any(void)
{
    return (FAULT_MASK_T__G__Mask != FAULT_NONE);
}

/**
 * @brief  [EN] Central battery-lost evaluation (see fault.h for the two
 *         cases). Sets FAULT_CHARGER_BAT_LOST after the debounce of either
 *         rule and clears it after the recovery settle; only this bit is
 *         touched. Called every control pass before func__Fault_Get().
 *         [FA] ارزیابی متمرکز قطع باتری؛ فقط همین بیت را Set/Clear می‌کند.
 */
/* ==================== Fault_Evaluate ==================== */

void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__lowMv;
    uint32_t uint32_t__highMv;
    bool     bool__inputOk;
    bool     bool__anyOver;
    bool     bool__anyHalfLow;
    bool     bool__batteryTrulyPresent;
    bool     bool__healthy;
    bool     bool__highHalfInstalled;
    bool     bool__lowHalfInstalled;

    uint32_t__nowTick = osKernelGetTickCount();

    /* [EN] Manual test mode (user order 2026-09-23): battery conditions are
       suspended - freeze every debounce so nothing new latches and nothing
       clears while bench testing without a battery. The charger clears the
       BAT_LOST bit on entering the mode (its alarm goes silent), and the
       detectors restart fresh when the mode exits.
       [FA] مود تست دستی (دستور کاربر ۲۰۲۶-۰۹-۲۳): شرط‌های باتری تعلیق
       می‌شوند - همهٔ دبانس‌ها فریز تا حین تستِ بدون باتری چیزی جدید قفل
       یا پاک نشود. شارژر هنگام ورود به مود بیت BAT_LOST را پاک می‌کند
       (آلارم ساکت) و آشکارسازها بعد از خروج از صفر شروع می‌کنند. */
    if (func__Charger_IsManualTestModeActive() != false)
    {
        UINT32_T__G__BatOverSinceTick = 0u;
        UINT32_T__G__BatAbsentSinceTick = 0u;
        UINT32_T__G__BatHealthySinceTick = 0u;
        return;
    }

    /* [EN] No trustworthy snapshot: freeze every progress (no set, no clear,
       and debounce restarts from zero next valid pass).
       [FA] بدون snapshot معتبر: هیچ تغییری نده و دبانس‌ها را صفر کن. */
    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false))
    {
        UINT32_T__G__BatOverSinceTick    = 0u;
        UINT32_T__G__BatAbsentSinceTick  = 0u;
        UINT32_T__G__BatHealthySinceTick = 0u;
        return;
    }

    uint32_t__lowMv  = measurement_snapshot_t__snap->v_bat_low_mv;
    uint32_t__highMv = measurement_snapshot_t__snap->v_bat_high_mv;

    /* [EN] Per-half participation follows the CHANNEL INSTALL MAP (bench bug
       2026-09-20: with CHG_CHANNEL_1_INSTALLED=0 the low half sits unwired
       below 7 V, rule 2 latched FAULT_CHARGER_BAT_LOST forever and the
       charger looked dead while the UI yellow kept blinking). Mapping
       (charger.c): channel 0 = v_bat_high, channel 1 = v_bat_low - an
       uninstalled channel's half can never be "disconnected", it is simply
       not wired.
       [FA] مشارکت هر نیم‌سل تابع نقشهٔ نصب کانال است (باگ بنچ: با کانال ۱
       غیرفعال، نیم‌سل پایین بی‌سیم زیر ۷V می‌ماند و باتری-لاست ابدی قفل
       می‌شد): کانال ۰ = نیم‌سل بالا، کانال ۱ = نیم‌سل پایین؛ نیم‌سل کانال
       غیرفعال اصلاً «قطع» محسوب نمی‌شود. */
    bool__highHalfInstalled = ((CHG_INSTALLED_CHANNEL_MASK & (1u << 0u)) != 0u);
    bool__lowHalfInstalled  = ((CHG_INSTALLED_CHANNEL_MASK & (1u << 1u)) != 0u);

    /* [EN] Case-2 gate: input present and in range (21..28 V).
       [FA] گِیت حالت دوم: ورودی حاضر و در بازه سالم ۲۱ تا ۲۸ ولت. */
    bool__inputOk = ((measurement_snapshot_t__snap->v_in_mv >= FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv) &&
                     (measurement_snapshot_t__snap->v_in_mv <= FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv));

    /* [EN] Case 1: either half pumped above 14.8 V (flyback signature while
       charging with the battery wire cut). No input gate needed - only
       switching can push a battery node that high.
       [FA] حالت اول: هر نیم‌باتری بالای ۱۴٫۸V (امضای پمپ حین شارژ). */
    /* [EN] Armed ONLY while some channel is actually pumping (bench
       2026-09-20: a parked FLOAT/done phase cannot spike the line, so bench
       transients there must not trip the detector); halves of uninstalled
       channels never participate.
       [FA] فقط وقتی مسلح که واقعاً پمپی در کار باشد؛ نیم‌سل کانال غیرفعال
       مشارکت ندارد. */
    bool__anyOver =
        (func__Charger_IsAnyChannelActive() == true) &&
        (((bool__lowHalfInstalled  == true) &&
          (uint32_t__lowMv  > FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)) ||
         ((bool__highHalfInstalled == true) &&
          (uint32_t__highMv > FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)));

    /* [EN] Case 2, user rewrite 2026-09-19 + threshold split 2026-09-22:
       EITHER half below FAULT_BAT_ABSENT_MV (6 V) while the input is fine =
       battery disconnected. "ALL halves" could never fire during his real
       failure (one lead cut, the other half stays at ~13 V) and with the
       charger parked there is no pump signature either - the result was
       total silence on a cut battery. Detection sits at 6 V while recovery
       keeps FAULT_BATTERY_BACK_MV = 7 V: a real 12 V battery always sits
       far above both, a deeply discharged-but-connected battery dips near
       6 V during charge without tripping, and the set/clear pair gets a
       clean 1 V hysteresis instead of a symmetric boundary that could
       chatter.
       [FA] حالت دوم (بازنویسی کاربر ۲۰۲۶-۰۹-۱۹ + جداسازی آستانه
       ۲۰۲۶-۰۹-۲۲): هرکدام از نیم‌باتری‌ها زیر ۶V با ورودی سالم یعنی قطع
       باتری؛ «هر دو غایب» در خرابی واقعی (یک سیم قطع، نیمِ دیگر ~۱۳V)
       هرگز فایر نمی‌شد و با شارژر پارک‌شده امضای پمپ هم نیست - نتیجه سکوت
       کامل بود. تشخیص روی ۶V و بازیابی روی ۷V (FAULT_BATTERY_BACK_MV):
       باتری واقعی ۱۲V از هر دو بالاتر است، باتری عمیق‌دشارژ حین شارژ تا
       نزدیک ۶V می‌افتد بدون تریپ، و جفت Set/Clear به‌جای مرز متقارنِ
       امکان-لرزش، یک هیسترزیس تمیز ۱V می‌گیرد. */
    bool__anyHalfLow =
        (((bool__lowHalfInstalled  == true) &&
          (uint32_t__lowMv  < FAULT_ALARM_T__G__Alarm.uint32_t__absentMv)) ||
         ((bool__highHalfInstalled == true) &&
          (uint32_t__highMv < FAULT_ALARM_T__G__Alarm.uint32_t__absentMv)));

    /* ---------- Rule 1: pumped overvoltage => latch ---------- */
    if ((bool__anyOver == true) &&
        (func__Fault_DebounceDone(&UINT32_T__G__BatOverSinceTick,
                                  uint32_t__nowTick,
                                  FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs) == true))
    {
        func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    }
    else if (bool__anyOver == false)
    {
        UINT32_T__G__BatOverSinceTick = 0u;
    }
    else
    {
        /* [EN] Debounce still running. [FA] دبانس در جریان است. */
    }

    /* ---------- Rule 2: EITHER half below 6 V with valid input => latch ---------- */
    if ((bool__inputOk == true) && (bool__anyHalfLow == true) &&
        (func__Fault_DebounceDone(&UINT32_T__G__BatAbsentSinceTick,
                                  uint32_t__nowTick,
                                  FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs) == true))
    {
        func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    }
    else if ((bool__inputOk == false) || (bool__anyHalfLow == false))
    {
        UINT32_T__G__BatAbsentSinceTick = 0u;
    }
    else
    {
        /* [EN] Debounce still running. [FA] دبانس در جریان است. */
    }

    /* ---------- Shared recovery: healthy window held 1 s => release ----------
       [EN] 2026-09-19 bench finding: with ONE lead cut, the still-attached
       half sits around 13 V, so the old health test ("just not ALL absent")
       passed, the flag cleared after ~1 s and the alarm died after a single
       burst - and nothing re-arms it anymore, because the charger now waits
       15 s of connection-settle before it can re-bulk and re-pump. Healthy
       now means: no half pumped AND a REAL battery on BOTH halves (at least
       FAULT_BATTERY_BACK_MV = 7 V measured on each): a lead still cut leaves its
       half below 7 V, the flag stays latched, and the red/triple-beep
       reminder repeats until the battery is genuinely back (user expectation:
       "the alarm must keep reminding me until I reconnect").
       [FA] بازبینی شرط سلامت: با یک سیمِ قطع، نیمِ سالم ~۱۳V می‌ماند و تست
       قدیمی («فقط هردو نباشند») آلارم را پس از یک بوق پاک می‌کرد و چون
       شارژر دیگر برای بازمسلح‌کردن بالا نمی‌آید، سکوت می‌ماند. حالا سالم
       یعنی: نه پمپ روی هیچ نیم و نه هیچ نیمِ زیر ۷V (۶V ممکن است حین شارژ باشد)؛ تا باتری واقعاً برنگشته
       آلارم قفل است و یادآوری تکرار می‌شود. */
    bool__batteryTrulyPresent =
        (((bool__lowHalfInstalled  == false) ||
          (uint32_t__lowMv  >= FAULT_ALARM_T__G__Alarm.uint32_t__backMv)) &&
         ((bool__highHalfInstalled == false) ||
          (uint32_t__highMv >= FAULT_ALARM_T__G__Alarm.uint32_t__backMv)));

    bool__healthy = ((bool__anyOver == false) &&
                     (bool__batteryTrulyPresent == true));

    if (bool__healthy == false)
    {
        UINT32_T__G__BatHealthySinceTick = 0u;
    }
    else if (func__Fault_DebounceDone(&UINT32_T__G__BatHealthySinceTick,
                                      uint32_t__nowTick,
                                      FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs) == true)
    {
        /* [EN] Both halves back inside the valid window for the settle time:
           the battery is really connected again. The charger mirrors the
           cleared bit to channel OFF, and its own 15 s connection-settle
           still gates the actual bulk start.
           [FA] هر دو نیم‌باتری ۱ ثانیه در پنجره سالم پایدار - یعنی باتری
           واقعاً برگشته؛ با پاک‌شدن بیت، شارژر کانال را آزاد می‌کند و گیت
           ۱۵ ثانیه‌ای ثبات اتصالِ خودش شروع بالک را کنترل می‌کند. */
        func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
        UINT32_T__G__BatHealthySinceTick = 0u;
    }
    else
    {
        /* [EN] Settle still running. [FA] زمان پایداری در جریان است. */
    }
}