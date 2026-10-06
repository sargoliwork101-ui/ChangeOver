/**
 * @file    host_test_fault.c
 * @brief   [EN] Host unit test for the Fault module. The PRODUCTION fault.c
 *              is compiled as-is and driven through a fake kernel clock and
 *              a fake charger. No board, no RTOS, no HAL.
 *          [FA] تست هاست ماژول Fault: همان فایل محصول با ساعت بدلی کرنل و
 *              شارژر بدلی رانده می‌شود. بدون برد، RTOS و HAL.
 *
 * @note    [EN] Covers the mask API, the two battery-lost detectors with
 *              their debounces, the recovery path, the manual-test freeze,
 *              the alarm-parameter clamps and the tick wrap.
 *          [FA] پوشش: API ماسک، دو آشکارساز قطع باتری با دبانس، مسیر
 *              بازیابی، فریز مود تست دستی، گیرهٔ پارامترهای آلارم و سرریز تیک.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "fault.h"
#include "charger.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake world / دنیای بدلی ==================== */

static uint32_t UINT32_T__G__FakeTick       = 0u;
static bool     BOOL__G__Ch0Active          = false;
static bool     BOOL__G__Ch1Active          = false;
static bool     BOOL__G__ManualTestActive   = false;

uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}

/* [EN] The module only uses the lock to make a read-modify-write atomic;
       on the host there is a single thread, so a token is enough.
   [FA] ماژول فقط برای اتمیک‌کردن خواندن-تغییر-نوشتن قفل می‌گیرد؛ روی هاست
       یک نخ بیشتر نیست، پس یک ژتون کافی است. */
int32_t osKernelLock(void);
int32_t osKernelLock(void)
{
    return 0;
}

int32_t osKernelRestoreLock(int32_t int32_t__state);
int32_t osKernelRestoreLock(int32_t int32_t__state)
{
    return int32_t__state;
}

uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    return uint32_t__milliseconds;      /* [EN] 1 kHz kernel / کرنل ۱kHz */
}

bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex)
{
    return (uint8_t__channelIndex == 0u) ? BOOL__G__Ch0Active : BOOL__G__Ch1Active;
}

bool func__Charger_IsManualTestModeActive(void)
{
    return BOOL__G__ManualTestActive;
}

/* [EN] The fault module asks the charger for the absorb/OV numbers that bound
       the disconnect threshold. Report the compiled profile values.
   [FA] ماژول فالت اعداد ‎absorb/OV‎ را از شارژر می‌پرسد تا حد قطع را مرزبندی
       کند؛ همان مقادیر پروفایل کامپایل برگردانده می‌شود. */
bool func__Charger_GetProfileParam(uint8_t uint8_t__paramId, uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL)
    {
        return false;
    }
    *uint32_t__value = 14400u;
    (void)uint8_t__paramId;
    return true;
}

bool func__Charger_GetAlarmParam(uint8_t uint8_t__paramId, uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL)
    {
        return false;
    }
    *uint32_t__value = 15000u;
    (void)uint8_t__paramId;
    return true;
}

/* ==================== Test driver / رانندهٔ تست ==================== */

static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

static void func__Advance(uint32_t uint32_t__milliseconds)
{
    UINT32_T__G__FakeTick += uint32_t__milliseconds;
}

/**
 * @brief  [EN] Healthy world: mains in range, both halves at 12.5 V, no
 *              channel pumping, manual test off, fresh fault module.
 *         [FA] دنیای سالم: ورودی در بازه، هر دو نیم ۱۲٫۵ ولت، بدون پمپ،
 *              مود تست خاموش، ماژول فالت نو.
 */
static void func__Reset(uint32_t uint32_t__startTick)
{
    UINT32_T__G__FakeTick     = uint32_t__startTick;
    BOOL__G__Ch0Active        = false;
    BOOL__G__Ch1Active        = false;
    BOOL__G__ManualTestActive = false;

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv       = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv    = 25000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv    = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv  = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid         = true;

    func__Fault_Init();
}

/**
 * @brief  [EN] Run N evaluation passes, 10 ms apart like the control task.
 *         [FA] N گام ارزیابی با فاصلهٔ ۱۰ms مثل تسک کنترل اجرا می‌کند.
 */
static void func__RunMs(uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__elapsed;

    for (uint32_t__elapsed = 0u;
         uint32_t__elapsed < uint32_t__milliseconds;
         uint32_t__elapsed += 10u)
    {
        func__Advance(10u);
        func__Fault_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    }
}

int main(void)
{
    uint32_t uint32_t__value;

    printf("== Fault host test ==\n");

    /* ---- 1. mask API: set, read, clear, Any ---- */
    func__Reset(1000u);
    CHECK(func__Fault_Get() == FAULT_NONE);
    CHECK(func__Fault_Any() == false);

    func__Fault_Set(FAULT_ADC);
    CHECK(func__Fault_Get() == FAULT_ADC);
    CHECK(func__Fault_Any() == true);

    func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    CHECK(func__Fault_Get() == (FAULT_ADC | FAULT_CHARGER_BAT_LOST));

    func__Fault_Clear(FAULT_ADC);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
    CHECK(func__Fault_Any() == false);

    /* [EN] Setting zero or clearing an unset bit must be harmless.
       [FA] ست‌کردن صفر یا پاک‌کردن بیت خاموش باید بی‌اثر باشد. */
    func__Fault_Set(FAULT_NONE);
    func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* ---- 2. no snapshot / invalid snapshot: nothing may latch ---- */
    func__Reset(2000u);
    func__Fault_Evaluate(NULL);
    CHECK(func__Fault_Get() == FAULT_NONE);

    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = 0u;      /* [EN] would trip if judged */
    func__RunMs(5000u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* ---- 3. detector 1: a PUMPED half flying over the disconnect level ----
       [EN] The rule is armed per half by that half's own charging channel.
            With no channel pumping, even a half far above the threshold must
            NOT latch (this is the v1.21 false-3-beep fix).
       [FA] قانون به‌ازای هر نیم با کانال شارژ خودش مسلح می‌شود. بدون پمپ،
            حتی نیمِ بسیار بالاتر از آستانه هم نباید قفل کند. */
    func__Reset(3000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_DISCONNECT_MV + 500u;
    func__RunMs(3000u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* [EN] Now the LOW half's own channel (index 1) pumps: after the 150 ms
            debounce the bit latches - but not before it.
       [FA] حالا کانال خودِ نیم پایین (شاخص ۱) پمپ می‌کند: بیت پس از دبانس
            ۱۵۰ms قفل می‌شود، نه زودتر. */
    BOOL__G__Ch1Active = true;
    func__RunMs(100u);
    CHECK(func__Fault_Get() == FAULT_NONE);
    func__RunMs(100u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    /* [EN] The wrong channel must not arm the other half.
       [FA] کانال اشتباه نباید نیم دیگر را مسلح کند. */
    func__Reset(10000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = FAULT_BAT_DISCONNECT_MV + 500u;
    BOOL__G__Ch1Active = true;              /* [EN] low-half channel only */
    func__RunMs(3000u);
    CHECK(func__Fault_Get() == FAULT_NONE);
    BOOL__G__Ch0Active = true;              /* [EN] the high half's own channel */
    func__RunMs(500u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    /* ---- 4. detector 2: a half under 6 V while the input is healthy ---- */
    func__Reset(20000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_ABSENT_MV - 1u;
    func__RunMs(900u);
    CHECK(func__Fault_Get() == FAULT_NONE);          /* [EN] 1 s debounce not done */
    func__RunMs(200u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    /* [EN] Same dip WITHOUT a healthy input must never latch: no mains means
            no way to tell a missing battery from a flat one.
       [FA] همان افت بدون ورودی سالم هرگز قفل نمی‌کند. */
    func__Reset(30000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = FAULT_INPUT_PRESENT_MIN_MV - 1u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_ABSENT_MV - 1u;
    func__RunMs(5000u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* [EN] An input ABOVE the window is just as invalid as one below it.
       [FA] ورودی بالاتر از بازه هم مثل پایین‌تر نامعتبر است. */
    func__Reset(35000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = FAULT_INPUT_PRESENT_MAX_MV + 1u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_ABSENT_MV - 1u;
    func__RunMs(5000u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* ---- 5. recovery: 7 V back-threshold with 1 V hysteresis ----
       [EN] A battery sitting between 6 V and 7 V has stopped tripping but is
            not healthy yet, so the latched bit must stay.
       [FA] باتری بین ۶ و ۷ ولت دیگر تریپ نمی‌زند ولی هنوز سالم نیست، پس بیت
            قفل‌شده باید بماند. */
    func__Reset(40000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_ABSENT_MV - 1u;
    func__RunMs(1200u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BATTERY_BACK_MV - 1u;
    func__RunMs(5000u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BATTERY_BACK_MV;
    func__RunMs(900u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);   /* [EN] recover time */
    func__RunMs(200u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* ---- 6. manual test mode freezes the battery verdicts ---- */
    func__Reset(50000u);
    BOOL__G__ManualTestActive = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_ABSENT_MV - 1u;
    func__RunMs(5000u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* [EN] And it also freezes CLEARING: a bit latched before the mode was
            entered survives until the mode ends.
       [FA] پاک‌شدن هم فریز می‌شود: بیتی که پیش از ورود قفل شده تا پایان مود
            باقی می‌ماند. */
    BOOL__G__ManualTestActive = false;
    func__RunMs(1200u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);
    BOOL__G__ManualTestActive = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = 12500u;
    func__RunMs(5000u);
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);
    BOOL__G__ManualTestActive = false;
    func__RunMs(1200u);
    CHECK(func__Fault_Get() == FAULT_NONE);

    /* ---- 7. alarm parameters: range clamps and read-back ---- */
    func__Reset(60000u);
    CHECK(func__Fault_GetAlarmParam(FAULT_ALARM_PARAM_DISCONNECT_MV, &uint32_t__value));
    CHECK(uint32_t__value == FAULT_BAT_DISCONNECT_MV);

    CHECK(func__Fault_SetAlarmParam(FAULT_ALARM_PARAM_DISCONNECT_DEB_MS, 0u, &uint32_t__value));
    CHECK(uint32_t__value >= 50u);                 /* [EN] clamped up to the floor */
    CHECK(func__Fault_SetAlarmParam(FAULT_ALARM_PARAM_DISCONNECT_DEB_MS, 99999u, &uint32_t__value));
    CHECK(uint32_t__value <= 1000u);               /* [EN] clamped down to the ceiling */
    CHECK(func__Fault_SetAlarmParam(FAULT_ALARM_PARAM_DISCONNECT_DEB_MS,
                                    FAULT_BAT_DISCONNECT_DEBOUNCE_MS, &uint32_t__value));
    CHECK(uint32_t__value == FAULT_BAT_DISCONNECT_DEBOUNCE_MS);

    /* [EN] absent/back must keep at least 500 mV of hysteresis whichever
            side is written first.
       [FA] نگه‌داشتن دست‌کم ۵۰۰mV هیسترزیس، از هر طرف که نوشته شود. */
    CHECK(func__Fault_SetAlarmParam(FAULT_ALARM_PARAM_ABSENT_MV, 7900u, &uint32_t__value));
    CHECK(func__Fault_GetAlarmParam(FAULT_ALARM_PARAM_BACK_MV, &uint32_t__value));
    CHECK(uint32_t__value >= 500u);
    CHECK(func__Fault_SetAlarmParam(FAULT_ALARM_PARAM_ABSENT_MV, FAULT_BAT_ABSENT_MV, &uint32_t__value));
    CHECK(func__Fault_SetAlarmParam(FAULT_ALARM_PARAM_BACK_MV, FAULT_BATTERY_BACK_MV, &uint32_t__value));

    /* [EN] An id this module does not own must be refused, not silently
            swallowed (the panel relies on the false to try the next owner).
       [FA] شناسه‌ای که مال این ماژول نیست باید رد شود، نه بی‌صدا بلعیده. */
    CHECK(func__Fault_SetAlarmParam(200u, 1u, &uint32_t__value) == false);
    CHECK(func__Fault_GetAlarmParam(200u, &uint32_t__value) == false);

    /* ---- 8. the 49.7-day tick wrap must not break a debounce ----
       [EN] Start the absent debounce just before the counter wraps.
       [FA] دبانس درست پیش از سرریز شمارنده شروع می‌شود. */
    func__Reset(0xFFFFFC00u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = FAULT_BAT_ABSENT_MV - 1u;
    func__RunMs(2000u);
    CHECK(UINT32_T__G__FakeTick < 0xFFFFFC00u);          /* [EN] really wrapped */
    CHECK(func__Fault_Get() == FAULT_CHARGER_BAT_LOST);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
