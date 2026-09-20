/**
 * @file    fault.c
 * @brief   [EN] Central latched fault bits + battery-lost detection. The
 *               Fault module OWNS battery-lost evaluation (user directive):
 *               pump rule (>14.8 V while charging) and absent rule (either
 *               half <7 V with valid input) both latch FAULT_CHARGER_BAT_LOST
 *               here; the charger only mirrors the bit.
 *          [FA] بیت‌های خطای قفل‌شدهٔ مرکزی + تشخیص قطع باتری. هر دو قاعده
 *               (پمپ بالای ۱۴٫۸V، نیم‌سل زیر ۷V با ورودی معتبر) بیت را اینجا
 *               قفل می‌کنند؛ شارژر فقط آینه است.
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

static fault_mask_t FAULT_MASK_T__G__Mask = FAULT_NONE;

/* [EN] Battery-lost episode timers, in kernel ticks; 0 = timer idle. Kept
   file-local so the state machine below is the single writer.
   [FA] تایمرهای اپیزود قطع باتری به tick کرنل؛ صفر = غیرفعال. */
static uint32_t UINT32_T__G__PumpSinceTick;
static uint32_t UINT32_T__G__AbsentSinceTick;
static uint32_t UINT32_T__G__RecoverSinceTick;

/* ==================== Duration ticks ==================== */

/**
 * @brief  [EN] Milliseconds to kernel ticks via rtos_time.h (the only allowed
 *              converter per project rules), with a zero-guard so a short
 *              debounce never degenerates to 0 ticks on a slow tick kernel.
 *         [FA] تبدیل ms به تیک فقط از rtos_time.h (قانون پروژه) + نگهبان صفر.
 * @param  uint32_t__durationMs [EN] Duration in ms (10..3600000) / مدت بر حسب ms
 * @return uint32_t [EN] Kernel ticks, at least 1 / تعداد tick کرنل، حداقل ۱
 */
static uint32_t func__Fault_DurationTicks(uint32_t uint32_t__durationMs)
{
    uint32_t uint32_t__ticks;

    uint32_t__ticks = func__Rtos_MillisecondsToTicks(uint32_t__durationMs);
    if (uint32_t__ticks == 0u)
    {
        uint32_t__ticks = 1u;
    }

    return uint32_t__ticks;
}

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
/* ==================== Fault_Init ==================== */

void func__Fault_Init(void)
{
    FAULT_MASK_T__G__Mask = FAULT_NONE;
    UINT32_T__G__PumpSinceTick = 0u;
    UINT32_T__G__AbsentSinceTick = 0u;
    UINT32_T__G__RecoverSinceTick = 0u;
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

/* ==================== Fault_Evaluate ==================== */

void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint32_t uint32_t__nowTick;
    bool bool__inputOk;
    bool bool__pumpHigh;
    bool bool__anyHalfLow;
    bool bool__batteryTrulyPresent;

    /* [EN] Never decide from a stale / absent snapshot: warmup and DMA gaps
       must not flap the latched bit either way.
       [FA] از snapshot کهنه/نامعتبر تصمیم نمی‌گیریم؛ بیت قفل‌شده نباید با
       warm-up یا گپ DMA بالا پایین بپرد. */
    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false))
    {
        return;
    }

    uint32_t__nowTick = osKernelGetTickCount();

    bool__inputOk =
        (measurement_snapshot_t__snap->input_present == true) &&
        (measurement_snapshot_t__snap->v_in_mv >= FAULT_INPUT_PRESENT_MIN_MV) &&
        (measurement_snapshot_t__snap->v_in_mv <= FAULT_INPUT_PRESENT_MAX_MV);

    /* [EN] Per-half participation follows the CHANNEL INSTALL MAP (bench
       bug 2026-09-20: with CHG_CHANNEL_1_INSTALLED=0 the low half sits
       unwired below 7 V, rule 2 latched FAULT_CHARGER_BAT_LOST forever and
       the charger looked dead while the UI yellow kept blinking). Mapping
       (charger.c): channel 0 = v_bat_high, channel 1 = v_bat_low - an
       uninstalled channel's half can never be "disconnected", it is simply
       not wired.
       [FA] مشارکت هر نیم‌سل تابع نقشهٔ نصب کانال است (باگ بنچ: با کانال ۱
       غیرفعال، نیم‌سل پایین بی‌سیم زیر ۷V می‌ماند و باتری-لاست ابدی قفل
       می‌شد): کانال ۰ = نیم‌سل بالا، کانال ۱ = نیم‌سل پایین؛ نیم‌سل کانال
       غیرفعال اصلاً «قطع» محسوب نمی‌شود. */
    {
        bool bool__highHalfInstalled;
        bool bool__lowHalfInstalled;

        bool__highHalfInstalled =
            ((CHG_INSTALLED_CHANNEL_MASK & (1u << 0u)) != 0u);
        bool__lowHalfInstalled =
            ((CHG_INSTALLED_CHANNEL_MASK & (1u << 1u)) != 0u);

    /* [EN] Rule 1 (pump): a charging half above 14.8 V means the battery
       side is open - a healthy regulated half never exceeds ~14.6 V (the
       overshoot fast-down kicks in there), 150 ms of this is a definite cut.
       Armed ONLY while some channel is actually pumping (a parked
       FLOAT/done phase cannot spike the line, so bench transients there
       must not trip the detector).
       [FA] قاعدهٔ ۱ (پمپ): نیم‌سل بالای ۱۴٫۸V یعنی سمت باتری باز است؛ نیم‌سل
       سالم هرگز از ~۱۴٫۶V بالاتر نمی‌رود. فقط وقتی مسلح که واقعاً پمپی در
       کار باشد - فلوت پارک‌شده غیرفعالش می‌کند. */
    bool__pumpHigh =
        (bool__inputOk == true) &&
        (func__Charger_IsAnyChannelActive() == true) &&
        (((bool__highHalfInstalled == true) &&
          (measurement_snapshot_t__snap->v_bat_high_mv > FAULT_BAT_DISCONNECT_MV)) ||
         ((bool__lowHalfInstalled == true) &&
          (measurement_snapshot_t__snap->v_bat_low_mv > FAULT_BAT_DISCONNECT_MV)));

    if (bool__pumpHigh == false)
    {
        UINT32_T__G__PumpSinceTick = 0u;
    }
    else if (UINT32_T__G__PumpSinceTick == 0u)
    {
        UINT32_T__G__PumpSinceTick = uint32_t__nowTick;
    }
    else
    {
        /* [EN] Pump already running. / پمپ قبلاً شروع شده. */
    }

    if ((UINT32_T__G__PumpSinceTick != 0u) &&
        ((uint32_t)(uint32_t__nowTick - UINT32_T__G__PumpSinceTick) >=
         func__Fault_DurationTicks(FAULT_BAT_DISCONNECT_DEBOUNCE_MS)))
    {
        UINT32_T__G__PumpSinceTick = 0u;
        func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    }

    /* [EN] Rule 2 (absent): with a valid input, EITHER half below 7 V means
       the battery is disconnected (rewritten per user 2026-09-19: the old
       ALL-below-6-V rule stayed silent on a single cut lead).
       [FA] قاعدهٔ ۲ (غیبت): با ورودی معتبر، «هرکدام» از نیم‌سل‌ها زیر ۷V
       یعنی باتری قطع است - قاعدهٔ قدیمی ALL-زیر-۶V روی یک سیم بریده سکوت
       می‌کرد. */
    bool__anyHalfLow =
        (bool__inputOk == true) &&
        (((bool__highHalfInstalled == true) &&
          (measurement_snapshot_t__snap->v_bat_high_mv < FAULT_BATTERY_BACK_MV)) ||
         ((bool__lowHalfInstalled == true) &&
          (measurement_snapshot_t__snap->v_bat_low_mv < FAULT_BATTERY_BACK_MV)));

    if (bool__anyHalfLow == false)
    {
        UINT32_T__G__AbsentSinceTick = 0u;
    }
    else if (UINT32_T__G__AbsentSinceTick == 0u)
    {
        UINT32_T__G__AbsentSinceTick = uint32_t__nowTick;
    }
    else
    {
        /* [EN] Absent timer already running. / تایمر غیبت قبلاً شروع شده. */
    }

    if ((UINT32_T__G__AbsentSinceTick != 0u) &&
        ((uint32_t)(uint32_t__nowTick - UINT32_T__G__AbsentSinceTick) >=
         func__Fault_DurationTicks(FAULT_BAT_ABSENT_DEBOUNCE_MS)))
    {
        UINT32_T__G__AbsentSinceTick = 0u;
        func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    }

    /* [EN] Recovery: BOTH halves at/above 7 V with no pump condition,
       continuously for FAULT_BAT_RECOVER_MS - a single good frame must NOT
       clear the alarm while a cut lead still holds its half low (one-burst
       bug), and an active pump keeps the latch up by definition.
       [FA] بازیابی: «هر دو» نیم‌سل ≥۷V بدون شرط پمپ، پیوسته به‌مدت
       FAULT_BAT_RECOVER_MS - یک فریم خوب نباید آلارم را ببندد وقتی سیم بریده
       هنوز نیم‌سلش را پایین نگه داشته. */
    bool__batteryTrulyPresent =
        (bool__pumpHigh == false) &&
        ((bool__highHalfInstalled == false) ||
         (measurement_snapshot_t__snap->v_bat_high_mv >= FAULT_BATTERY_BACK_MV)) &&
        ((bool__lowHalfInstalled == false) ||
         (measurement_snapshot_t__snap->v_bat_low_mv >= FAULT_BATTERY_BACK_MV));
    }

    if (bool__batteryTrulyPresent == false)
    {
        UINT32_T__G__RecoverSinceTick = 0u;
    }
    else if (UINT32_T__G__RecoverSinceTick == 0u)
    {
        UINT32_T__G__RecoverSinceTick = uint32_t__nowTick;
    }
    else
    {
        /* [EN] Recover timer already running. / تایمر بازیابی قبلاً شروع شده. */
    }

    if ((UINT32_T__G__RecoverSinceTick != 0u) &&
        ((uint32_t)(uint32_t__nowTick - UINT32_T__G__RecoverSinceTick) >=
         func__Fault_DurationTicks(FAULT_BAT_RECOVER_MS)))
    {
        UINT32_T__G__RecoverSinceTick = 0u;
        func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
    }
}
