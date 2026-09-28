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

static volatile fault_mask_t FAULT_MASK_T__G__Mask = FAULT_NONE;

/* [EN] Battery-lost debounce timers: start tick of the current sustained
   condition, 0 = condition not running.
   [FA] تایمرهای دبانس قطع باتری؛ صفر یعنی شرط در جریان نیست. */
static uint32_t UINT32_T__G__BatOverSinceTick    = 0u;
static uint32_t UINT32_T__G__BatAbsentSinceTick  = 0u;
static uint32_t UINT32_T__G__BatHealthySinceTick = 0u;

/* ==================== Runtime alarm thresholds (v1.15) ==================== */
/* [EN] The eight FAULT_* numbers as one live struct - ids 27..34,
   flash-persisted, clamped as a set on every write; boot = macro defaults.
   [FA] هشت عدد FAULT_* در یک struct زنده - شناسه‌های ۲۷..۳۴، ماندگار در
   فلش، گیرهٔ مجموعه‌ای؛ بوت = پیش‌فرض ماکروها. */
/* [EN] volatile: written by the EspLink task, read by the control task
   (full-program audit 2026-09-26). [FA] بین دو تسک بدون قفل پس volatile. */
static volatile fault_alarm_t FAULT_ALARM_T__G__Alarm =
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

/* [EN] Layout contract for the indexed Set/GetAlarmParam below (flash diet
   2026-09-27): wire ids 27..34 dense, one packed uint32_t per id in the
   same order (host test pins every wire id).
   [FA] قرارداد چیدمان Set/Get نمایه‌ای: شناسه‌های ۲۷..۳۴ پشت‌سرهم، یک
   کلمه به همان ترتیب. */
_Static_assert(FAULT_ALARM_PARAM_DISCONNECT_MV == 27u,
               "fault alarm id base must be 27");
_Static_assert(FAULT_ALARM_PARAM_INPUT_MAX_MV == 34u,
               "fault alarm id top must be 34");
_Static_assert(sizeof(fault_alarm_t) == (8u * sizeof(uint32_t)),
               "fault_alarm_t must pack exactly 8 words");
_Static_assert(offsetof(fault_alarm_t, uint32_t__disconnectMv) == 0u,
               "first field must be the id-27 word");
_Static_assert(offsetof(fault_alarm_t, uint32_t__inputMaxMv) ==
                   (7u * sizeof(uint32_t)),
               "last field must be the id-34 word");

bool func__Fault_SetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue)
{
    /* [EN] Writer-side scheduler lock (v1.16 audit C11): the comm task
       writes, the control task (fault eval) preempts mid-clamp and would
       read a torn threshold set for one pass. Pre-kernel the plain path
       runs (NVM replay).
       [FA] قفل زمان‌بند سمت نویسنده: تسک ارتباط می‌نویسد و ارزیابی فالت
       وسط گیره پیشی می‌گیرد و یک پاس آستانهٔ پاره می‌خواند. */
    int32_t int32_t__savedKernelLock = osKernelLock();

    /* [EN] Indexed store (flash diet): ids 27..34 are dense and
       fault_alarm_t packs the same fields in the same order (asserts above).
       [FA] ذخیرهٔ نمایه‌ای: شناسه‌های ۲۷..۳۴ پشت‌سرهم و فیلدها به همان
       ترتیب‌اند (assert های بالا). */
    if ((uint8_t__paramId < FAULT_ALARM_PARAM_DISCONNECT_MV) ||
        (uint8_t__paramId > FAULT_ALARM_PARAM_INPUT_MAX_MV))
    {
        if (int32_t__savedKernelLock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        return false;
    }
    ((volatile uint32_t *)&FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)
        [uint8_t__paramId - FAULT_ALARM_PARAM_DISCONNECT_MV] = uint32_t__value;

    func__Fault_ClampAlarms();
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Fault_GetAlarmParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Fault_GetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value)
{
    /* [EN] Indexed read: same dense-id/struct contract as the setter.
       [FA] خواندن نمایه‌ای: همان قرارداد شناسه/ساختار. */
    if ((uint8_t__paramId < FAULT_ALARM_PARAM_DISCONNECT_MV) ||
        (uint8_t__paramId > FAULT_ALARM_PARAM_INPUT_MAX_MV))
    {
        return false;
    }
    *uint32_t__value =
        ((volatile uint32_t *)&FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)
        [uint8_t__paramId - FAULT_ALARM_PARAM_DISCONNECT_MV];
    return true;
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
    /* [EN] Scheduler lock (v1.16 audit F4): the RMW is shared by the
       control and protection tasks - a Set racing a Clear loses one
       update. Pre-kernel the lock call fails and the plain RMW runs
       single-threaded (same pattern as measurement.c).
       [FA] قفل زمان‌بند: RMW بین تسک کنترل و حفاظت مشترک است - بدون آن Set
       همزمان با Clear یک به‌روزرسانی را گم می‌کند. */
    int32_t int32_t__savedKernelLock = osKernelLock();

    FAULT_MASK_T__G__Mask |= fault_mask_t__bits;

    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
}

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 * @param  fault_mask_t__bits [EN] Bits to clear / بیت‌هایی که باید پاک شود
 */
/* ==================== Fault_Clear ==================== */

void func__Fault_Clear(fault_mask_t fault_mask_t__bits)
{
    /* [EN] Scheduler lock: same lost-update closure as Fault_Set.
       [FA] قفل زمان‌بند: همان بستن گم‌شدن به‌روزرسانی. */
    int32_t int32_t__savedKernelLock = osKernelLock();

    FAULT_MASK_T__G__Mask &= (fault_mask_t)~fault_mask_t__bits;

    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
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

    /* [EN] Manual test mode: battery conditions suspended - freeze every
       debounce (nothing new latches, nothing clears); the charger clears
       BAT_LOST on entering the mode and detectors restart on exit.
       [FA] مود تست دستی: شرط‌های باتری تعلیق - دبانس‌ها فریز؛ شارژر هنگام
       ورود بیت BAT_LOST را پاک می‌کند و آشکارسازها بعد از خروج از صفر. */
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

    /* [EN] Per-half participation follows the channel install map
       (channel 0 = high half, channel 1 = low half); an uninstalled half
       is simply not wired and can never count as "disconnected".
       [FA] مشارکت هر نیم تابع نقشهٔ نصب کانال است (کانال ۰ = نیم بالا،
       کانال ۱ = نیم پایین)؛ نیمِ کانال غیرنصب «قطع» محسوب نمی‌شود. */
    bool__highHalfInstalled = ((CHG_INSTALLED_CHANNEL_MASK & (1u << 0u)) != 0u);
    bool__lowHalfInstalled  = ((CHG_INSTALLED_CHANNEL_MASK & (1u << 1u)) != 0u);

    /* [EN] Case-2 gate: input present and in range (21..28 V).
       [FA] گِیت حالت دوم: ورودی حاضر و در بازه سالم ۲۱ تا ۲۸ ولت. */
    bool__inputOk = ((measurement_snapshot_t__snap->v_in_mv >= FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv) &&
                     (measurement_snapshot_t__snap->v_in_mv <= FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv));

    /* [EN] Case 1: a half pumped above the disconnect threshold (flyback
       signature of a cut battery wire while charging). v1.21 (user order
       2026-09-28, kills the repeating false 3-beep cycle during charge):
       armed PER HALF by THAT half's own pumping channel - a parked
       channel has no pump, so its half cannot fly up to the threshold
       and must not be judged. The derived vhigh = V24 - V12 in
       particular moves with the OTHER channel's load current, so judging
       it while only ch2 pumps latched phantom battery-lost alarms.
       [FA] حالت ۱: نیمی که شارژر خودش روی آن پمپ می‌کند بالای آستانهٔ
       قطع رفته (امضای پمپ سیم قطع حین شارژ). v1.21 (دستور کاربر
       ۲۰۲۶-۰۹-۲۸، رفع چرخهٔ کاذب سه‌بوق حین شارژ): مسلح‌شدن به‌ازای هر
       نیم با کانال پمپ‌کنندهٔ خودش - کانال پارک‌شده پمپی ندارد پس نیمش
       نمی‌تواند بالا پرود و قضاوت نمی‌شود. به‌ویژه vhigh مشتق‌شده =
       V24 − V12 با جریان بار کانال دیگر حرکت می‌کند و قضاوتش حین پمپِ
       فقط ch2 آلارم قطع‌باتریِ خیالی قفل می‌کرد. */
    bool__anyOver =
        (((bool__lowHalfInstalled  == true) &&
          (func__Charger_IsChannelActive(1u) == true) &&
          (uint32_t__lowMv  > FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)) ||
         ((bool__highHalfInstalled == true) &&
          (func__Charger_IsChannelActive(0u) == true) &&
          (uint32_t__highMv > FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)));

    /* [EN] Case 2: EITHER half below the absent threshold (6 V) with a
       valid input = battery disconnected. Recovery keeps 7 V (1 V
       hysteresis: a deeply discharged-but-connected battery dips near 6 V
       without tripping).
       [FA] حالت ۲: هر نیم زیر ۶V با ورودی سالم = قطع باتری؛ بازیابی روی
       ۷V (هیسترزیس ۱V تا دشارژ عمیقِ وصل بدون تریپ رد شود). */
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

    /* ---------- Shared recovery: healthy window held => release ----------
       [EN] Healthy = no half pumped AND both installed halves >= back
       threshold (7 V, not 6 V); held for the recover time => clear the bit,
       otherwise the reminder keeps repeating.
       [FA] سلامت = نه پمپ و هر دو نیمِ نصب‌شده >= ۷V (نه ۶V) به‌مدت زمان
       بازیابی ← پاک‌شدن بیت؛ تا آن‌وقت یادآوری تکرار می‌شود. */
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
        /* [EN] Battery really reconnected; the charger mirrors the cleared
           bit to OFF and its own 15 s settle still gates the bulk start.
           [FA] باتری واقعاً برگشته؛ شارژر بیت پاک‌شده را به OFF آینه می‌کند
           و گیت ۱۵s ثباتِ خودش شروع بالک را کنترل می‌کند. */
        func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
        UINT32_T__G__BatHealthySinceTick = 0u;
    }
    else
    {
        /* [EN] Settle still running. [FA] زمان پایداری در جریان است. */
    }
}