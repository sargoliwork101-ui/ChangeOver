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
    bool__inputOk = ((measurement_snapshot_t__snap->v_in_mv >= FAULT_INPUT_PRESENT_MIN_MV) &&
                     (measurement_snapshot_t__snap->v_in_mv <= FAULT_INPUT_PRESENT_MAX_MV));

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
          (uint32_t__lowMv  > FAULT_BAT_DISCONNECT_MV)) ||
         ((bool__highHalfInstalled == true) &&
          (uint32_t__highMv > FAULT_BAT_DISCONNECT_MV)));

    /* [EN] Case 2, user rewrite 2026-09-19: EITHER half below
       FAULT_BATTERY_BACK_MV (7 V) while the input is fine = battery
       disconnected. "ALL halves" could never fire during his real failure
       (one lead cut, the other half stays at ~13 V) and with the charger
       parked there is no pump signature either - the result was total
       silence on a cut battery. 7 V matches the recovery threshold so the
       set/clear pair is symmetric; a real 12 V battery always sits far
       above, charge-time included.
       [FA] حالت دوم (بازنویسی دستور کاربر): هرکدام از نیم‌باتری‌ها زیر ۷V با
       ورودی سالم یعنی قطع باتری؛ «هر دو غایب» در خرابی واقعی (یک سیم قطع،
       نیمِ دیگر ~۱۳V) هرگز فایر نمی‌شد و با شارژر پارک‌شده امضای پمپ هم
       نیست - نتیجه سکوت کامل بود. با ۷V جفت Set/Clear متقارن است. */
    bool__anyHalfLow =
        (((bool__lowHalfInstalled  == true) &&
          (uint32_t__lowMv  < FAULT_BATTERY_BACK_MV)) ||
         ((bool__highHalfInstalled == true) &&
          (uint32_t__highMv < FAULT_BATTERY_BACK_MV)));

    /* ---------- Rule 1: pumped overvoltage => latch ---------- */
    if ((bool__anyOver == true) &&
        (func__Fault_DebounceDone(&UINT32_T__G__BatOverSinceTick,
                                  uint32_t__nowTick,
                                  FAULT_BAT_DISCONNECT_DEBOUNCE_MS) == true))
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

    /* ---------- Rule 2: EITHER half below 7 V with valid input => latch ---------- */
    if ((bool__inputOk == true) && (bool__anyHalfLow == true) &&
        (func__Fault_DebounceDone(&UINT32_T__G__BatAbsentSinceTick,
                                  uint32_t__nowTick,
                                  FAULT_BAT_ABSENT_DEBOUNCE_MS) == true))
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
          (uint32_t__lowMv  >= FAULT_BATTERY_BACK_MV)) &&
         ((bool__highHalfInstalled == false) ||
          (uint32_t__highMv >= FAULT_BATTERY_BACK_MV)));

    bool__healthy = ((bool__anyOver == false) &&
                     (bool__batteryTrulyPresent == true));

    if (bool__healthy == false)
    {
        UINT32_T__G__BatHealthySinceTick = 0u;
    }
    else if (func__Fault_DebounceDone(&UINT32_T__G__BatHealthySinceTick,
                                      uint32_t__nowTick,
                                      FAULT_BAT_RECOVER_MS) == true)
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