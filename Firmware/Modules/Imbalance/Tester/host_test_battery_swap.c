/**
 * @file    host_test_battery_swap.c
 * @brief   [EN] Integration regression for the production Fault -> TaskControl
 *              input mapping -> Imbalance battery-swap reset path. It uses a
 *              fake CMSIS clock and charger status; no board or HAL.
 *          [FA] تست یکپارچهٔ مسیر Fault تا ورودی TaskControl و ریست تعویض
 *              باتری در Imbalance؛ ساعت و وضعیت شارژر بدلی‌اند.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "fault.h"
#include "charger.h"
#include "../imbalance.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int32_t INT32_T__G__Checks = 0;
static int32_t INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake world / دنیای بدلی ==================== */

static uint32_t UINT32_T__G__FakeTick = 0u;
static bool BOOL__G__Ch0Active = false;
static bool BOOL__G__Ch1Active = false;
static bool BOOL__G__ManualTestActive = false;
static bool BOOL__G__ChargeComplete = false;
static bool BOOL__G__OnBattery = false;
static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;
static imbalance_inputs_t IMBALANCE_INPUTS_T__G__Inputs;
static imbalance_outputs_t IMBALANCE_OUTPUTS_T__G__Outputs;

/* ==================== Fake CMSIS tick / تیک CMSIS بدلی ==================== */
/**
 * @‎brief  [EN] Return the fake monotonic kernel tick / [FA]‎ تیک یکنواخت بدلی را می‌دهد.
 * @‎param  void [EN] No parameters / [FA]‎ پارامتر ندارد.
 * @‎return uint32_t [EN] Fake tick count, 1 ms per tick / [FA]‎ شمار تیک بدلی، هر تیک ۱ms.
 */
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}

/* ==================== Fake CMSIS lock / قفل CMSIS بدلی ==================== */
/**
 * @‎brief  [EN] Stub the single-thread host kernel lock / [FA]‎ قفل کرنل تک‌نخی هاست را بدل می‌کند.
 * @‎param  void [EN] No parameters / [FA]‎ پارامتر ندارد.
 * @‎return int32_t [EN] Placeholder previous state, always 0 / [FA]‎ وضعیت قبلی بدلی، همیشه صفر.
 */
int32_t osKernelLock(void)
{
    return 0;
}

/* ==================== Fake CMSIS restore / بازگردانی CMSIS بدلی ==================== */
/**
 * @‎brief  [EN] Restore the placeholder lock state / [FA]‎ وضعیت قفل بدلی را برمی‌گرداند.
 * @‎param  int32_t__state [EN] Previous placeholder state / [FA]‎ وضعیت قبلی بدلی.
 * @‎return int32_t [EN] The same state / [FA]‎ همان وضعیت.
 */
int32_t osKernelRestoreLock(int32_t int32_t__state)
{
    return int32_t__state;
}

/* ==================== Fake RTOS time / زمان RTOS بدلی ==================== */
/**
 * @‎brief  [EN] Convert ms to ticks in a 1 kHz host clock / [FA]‎ در ساعت هاست ۱kHz میلی‌ثانیه را به تیک تبدیل می‌کند.
 * @‎param  uint32_t__milliseconds [EN] Duration in ms / [FA]‎ مدت به میلی‌ثانیه.
 * @‎return uint32_t [EN] Same duration as ticks / [FA]‎ همان مدت به تیک.
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    return uint32_t__milliseconds;
}

/* ==================== Fake charger channel / کانال شارژر بدلی ==================== */
/**
 * @‎brief  [EN] Return the active state of a synthetic channel / [FA]‎ وضعیت فعال‌بودن کانال ساختگی را می‌دهد.
 * @‎param  uint8_t__channelIndex [EN] Channel index 0 or 1 / [FA]‎ شناسهٔ کانال، صفر یا یک.
 * @return bool [EN] Channel state; ‎false for other indices / [FA]‎ وضعیت کانال؛ برای شناسهٔ دیگر false.
 */
bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return BOOL__G__Ch0Active;
    }
    if (uint8_t__channelIndex == 1u)
    {
        return BOOL__G__Ch1Active;
    }
    return false;
}

/* ==================== Fake charger state / وضعیت شارژر بدلی ==================== */
/**
 * @‎brief  [EN] Return whether either synthetic channel is active / [FA]‎ فعال‌بودن هر کانال ساختگی را می‌سنجد.
 * @‎param  void [EN] No parameters / [FA]‎ پارامتر ندارد.
 * @‎return bool [EN] true if channel 0 or 1 is active / [FA]‎ اگر کانال صفر یا یک فعال باشد true.
 */
bool func__Charger_IsAnyChannelActive(void)
{
    return ((BOOL__G__Ch0Active == true) || (BOOL__G__Ch1Active == true));
}

/* ==================== Fake manual-test state / وضعیت تست دستی بدلی ==================== */
/**
 * @‎brief  [EN] Return the synthetic manual-test mode / [FA]‎ وضعیت تست دستی ساختگی را می‌دهد.
 * @‎param  void [EN] No parameters / [FA]‎ پارامتر ندارد.
 * @‎return bool [EN] true while manual test is active / [FA]‎ هنگام فعال‌بودن تست دستی true.
 */
bool func__Charger_IsManualTestModeActive(void)
{
    return BOOL__G__ManualTestActive;
}

/* ==================== Reset test world / آماده‌سازی محیط تست ==================== */
/**
 * @brief  [EN] Restore a healthy, idle test world and reset both production
 *              modules. The supplied time is in ms and must be nonzero so
 *              the Fault debounce sentinel is unambiguous.
 *         [FA] دنیای سالم و بی‌کار را آماده و هر دو ماژول محصول را مقداردهی
 *              می‌کند؛ زمان ورودی میلی‌ثانیه و ناصفر است.
 * @param  uint32_t__nowMs [EN] Fake monotonic time in ms / زمان یکنواخت بدلی، میلی‌ثانیه
 * @return void [EN] No return value / خروجی ندارد
 */
static void func__ResetWorld(uint32_t uint32_t__nowMs)
{
    UINT32_T__G__FakeTick = uint32_t__nowMs;
    BOOL__G__Ch0Active = false;
    BOOL__G__Ch1Active = false;
    BOOL__G__ManualTestActive = false;
    BOOL__G__ChargeComplete = false;
    BOOL__G__OnBattery = false;

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = 12000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = 12000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 12000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;

    func__Fault_Init();
    func__Imbalance_Init();
}

/* ==================== Set measurements / تنظیم اندازه‌گیری ساختگی ==================== */
/**
 * @brief  [EN] Set the synthetic battery/input measurements for the next
 *              control pass. All voltages are in mV.
 *         [FA] اندازه‌گیری ساختگی باتری و ورودی را برای پاس بعد می‌گذارد؛
 *              همهٔ ولتاژها میلی‌ولت‌اند.
 * @‎param  uint32_t__highMv [EN] Upper-half voltage, 0..UINT32_MAX mV‎ / ولتاژ نیم بالا
 * @‎param  uint32_t__lowMv [EN] Lower-half voltage, 0..UINT32_MAX mV‎ / ولتاژ نیم پایین
 * @‎param  uint32_t__inputMv [EN] Input voltage, 0..UINT32_MAX mV‎ / ولتاژ ورودی
 * @‎param  bool__inputPresent [EN] Input-presence GPIO state‎ / وضعیت پایهٔ حضور ورودی
 * @return void [EN] No return value / خروجی ندارد
 */
static void func__SetMeasurements(uint32_t uint32_t__highMv,
                                  uint32_t uint32_t__lowMv,
                                  uint32_t uint32_t__inputMv,
                                  bool bool__inputPresent)
{
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = uint32_t__highMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = uint32_t__lowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = uint32_t__highMv + uint32_t__lowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = uint32_t__lowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = uint32_t__inputMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = bool__inputPresent;
}

/* ==================== Run production path / اجرای مسیر محصول ==================== */
/**
 * @brief  [EN] Mirror the production TaskControl mapping for the Fault and
 *              Imbalance modules, then run one production evaluation pass.
 *         [FA] نگاشت واقعی TaskControl را برای Fault و Imbalance آینه می‌کند
 *              و یک پاس از هر دو منطق محصول را اجرا می‌کند.
 * @‎return bool [EN] true iff an Imbalance NVM-backed value changed‎ / اگر مقدار ماندگار عوض شد true
 */
static bool func__ControlPass(void)
{
    fault_mask_t fault_mask_t__faults;

    func__Fault_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    fault_mask_t__faults = func__Fault_Get();

    IMBALANCE_INPUTS_T__G__Inputs.uint32_t__vHighMv =
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv;
    IMBALANCE_INPUTS_T__G__Inputs.uint32_t__vLowMv =
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv;
    IMBALANCE_INPUTS_T__G__Inputs.bool__inputPresent =
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present;
    IMBALANCE_INPUTS_T__G__Inputs.bool__valid =
        ((MEASUREMENT_SNAPSHOT_T__G__Snap.valid != false) &&
         ((fault_mask_t__faults & FAULT_ADC) == 0u));
    IMBALANCE_INPUTS_T__G__Inputs.bool__batAbsent =
        func__Fault_IsBatteryAbsent();
    IMBALANCE_INPUTS_T__G__Inputs.bool__charging =
        func__Charger_IsAnyChannelActive();
    IMBALANCE_INPUTS_T__G__Inputs.bool__chargeComplete = BOOL__G__ChargeComplete;
    IMBALANCE_INPUTS_T__G__Inputs.bool__chargingCh1 =
        func__Charger_IsChannelActive(0u);
    IMBALANCE_INPUTS_T__G__Inputs.bool__chargingCh2 =
        func__Charger_IsChannelActive(1u);
    IMBALANCE_INPUTS_T__G__Inputs.bool__onBattery = BOOL__G__OnBattery;

    return func__Imbalance_Evaluate(&IMBALANCE_INPUTS_T__G__Inputs,
                                    UINT32_T__G__FakeTick,
                                    &IMBALANCE_OUTPUTS_T__G__Outputs);
}

/* ==================== Advance fake time / جلو‌بردن ساعت بدلی ==================== */
/**
 * @brief  [EN] Advance the fake clock and execute one control pass every
 *              10 ms. Duration is a nonnegative multiple of 10 ms.
 *         [FA] ساعت بدلی را جلو می‌برد و هر ۱۰ms یک پاس کنترل اجرا می‌کند؛
 *              مدت مضربی نامنفی از ۱۰ms است.
 * @param  uint32_t__durationMs [EN] Elapsed fake time in ms / زمان سپری‌شده، میلی‌ثانیه
 * @return bool [EN] true iff the final pass changed persisted state / اگر پاس آخر مقدار ماندگار را عوض کرد true
 */
static bool func__AdvanceControl(uint32_t uint32_t__durationMs)
{
    uint32_t uint32_t__elapsedMs;
    bool bool__persistChanged = false;

    for (uint32_t__elapsedMs = 0u;
         uint32_t__elapsedMs < uint32_t__durationMs;
         uint32_t__elapsedMs += 10u)
    {
        UINT32_T__G__FakeTick += 10u;
        bool__persistChanged = func__ControlPass();
    }
    return bool__persistChanged;
}

/* ==================== Seed persistent state / آماده‌سازی وضعیت ماندگار ==================== */
/**
 * @brief  [EN] Load the three NVM-backed Imbalance values used by the test.
 *         [FA] سه مقدار ماندگار عدم‌توازن را برای تست بار می‌کند.
 * @‎param  uint32_t__events [EN] Event count, 0..255‎ / شمارندهٔ رویداد
 * @‎param  uint32_t__cycles [EN] Post-latch cycles, 0..255‎ / سیکل‌های پس از قفل
 * @param  uint32_t__latched [EN] Latch state, 0 or 1 / وضعیت قفل
 * @return void [EN] No return value / خروجی ندارد
 */
static void func__SetPersistedState(uint32_t uint32_t__events,
                                    uint32_t uint32_t__cycles,
                                    uint32_t uint32_t__latched)
{
    CHECK(func__Imbalance_SetParam(IMBAL_SLOT_EVENTS_ID, uint32_t__events, NULL));
    CHECK(func__Imbalance_SetParam(IMBAL_SLOT_CYCLES_ID, uint32_t__cycles, NULL));
    CHECK(func__Imbalance_SetParam(IMBAL_SLOT_LATCH_ID, uint32_t__latched, NULL));
}

/* ==================== Check persistent state / بررسی وضعیت ماندگار ==================== */
/**
 * @brief  [EN] Assert all three NVM-backed state values.
 *         [FA] هر سه مقدار وضعیت ماندگار را بررسی می‌کند.
 * @‎param  uint32_t__events [EN] Expected event count, 0..255‎ / شمارندهٔ مورد انتظار
 * @‎param  uint32_t__cycles [EN] Expected cycle count, 0..255‎ / سیکل مورد انتظار
 * @param  uint32_t__latched [EN] Expected latch state, 0 or 1 / قفل مورد انتظار
 * @return void [EN] No return value / خروجی ندارد
 */
static void func__CheckPersistedState(uint32_t uint32_t__events,
                                      uint32_t uint32_t__cycles,
                                      uint32_t uint32_t__latched)
{
    uint32_t uint32_t__value = 0u;

    CHECK(func__Imbalance_GetParam(IMBAL_SLOT_EVENTS_ID, &uint32_t__value));
    CHECK(uint32_t__value == uint32_t__events);
    CHECK(func__Imbalance_GetParam(IMBAL_SLOT_CYCLES_ID, &uint32_t__value));
    CHECK(uint32_t__value == uint32_t__cycles);
    CHECK(func__Imbalance_GetParam(IMBAL_SLOT_LATCH_ID, &uint32_t__value));
    CHECK(uint32_t__value == uint32_t__latched);
}

/* ==================== Battery-swap integration test / تست یکپارچهٔ تعویض باتری ==================== */
/**
 * @brief  [EN] Run the Fault-to-Imbalance battery-swap regression suite.
 *         [FA] آزمون رگرسیون مسیر Fault تا Imbalance را اجرا می‌کند.
 * @‎param  void [EN] No parameters / [FA]‎ پارامتر ندارد.
 * @‎return int [EN] Zero on success, nonzero on any failed check / [FA]‎ صفر در موفقیت و غیرصفر در شکست.
 */
int main(void)
{
    uint32_t uint32_t__value = 0u;
    bool bool__changed;

    printf("== Battery swap integration host test ==\n");

    /* ---- 1. Real disconnect path with no mains; stale cycle must not relatch ---- */
    func__ResetWorld(1000u);
    func__SetPersistedState(1u, 0u, 1u);
    CHECK(func__Imbalance_SetParam(IMBAL_PARAM_MAX_EVENTS, 1u, &uint32_t__value));
    CHECK(func__Imbalance_SetParam(IMBAL_PARAM_IN_CHARGE_WAIT_MS, 1000u, &uint32_t__value));
    CHECK(func__Imbalance_SetParam(IMBAL_PARAM_STABILITY_MS, 1000u, &uint32_t__value));

    func__SetMeasurements(13000u, 12000u, 24000u, true);
    BOOL__G__Ch0Active = true;
    BOOL__G__Ch1Active = true;
    BOOL__G__ChargeComplete = false;
    (void)func__ControlPass();
    (void)func__AdvanceControl(1000u);
    (void)func__AdvanceControl(1000u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__episode == true);
    func__CheckPersistedState(1u, 1u, 1u);

    /* [EN] With mains absent, a fitted half below 3 V signals physical removal
       for reset only; it must not set the shared charger-fault bit.
       [FA] بدون ورودی، نیمِ نصب‌شده زیر ۳V فقط نشانهٔ ریست است و بیت خطای
       مشترک شارژر را روشن نمی‌کند. */
    func__SetMeasurements(12000u, FAULT_BAT_HARD_ABSENT_MV - 1u, 0u, false);
    BOOL__G__ChargeComplete = true; /* stale FLOAT must be ignored */
    bool__changed = func__ControlPass();
    CHECK(bool__changed == false);
    CHECK(func__Fault_IsBatteryAbsent() == true);
    CHECK(func__Fault_Get() == FAULT_NONE);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__episode == false);
    func__CheckPersistedState(1u, 1u, 1u);

    CHECK(func__AdvanceControl(2990u) == false);
    func__CheckPersistedState(1u, 1u, 1u);
    CHECK(func__AdvanceControl(10u) == true);
    func__CheckPersistedState(0u, 0u, 0u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__latched == false);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__blockOutput == false);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__chargingAllowed == true);

    CHECK(func__AdvanceControl(10u) == false);
    func__CheckPersistedState(0u, 0u, 0u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.uint32_t__events == 0u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__latched == false);

    /* [EN] Reconnection drops the live signal immediately. A stale FLOAT with
       no new charge must not resurrect the cleared full-cycle state.
       [FA] با اتصال دوباره نشانه فوراً می‌افتد؛ FLOAT کهنه بدون شارژ تازه
       نباید وضعیت چرخه را برگرداند. */
    func__SetMeasurements(12000u, 12000u, 0u, false);
    BOOL__G__Ch0Active = false;
    BOOL__G__Ch1Active = false;
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    func__CheckPersistedState(0u, 0u, 0u);

    /* ---- 2. A short absence that recovers before 3 s preserves the latch ---- */
    func__ResetWorld(20000u);
    func__SetPersistedState(2u, 3u, 1u);
    func__SetMeasurements(12000u, 5000u, 24000u, true);
    (void)func__ControlPass();
    CHECK(func__Fault_IsBatteryAbsent() == true);
    CHECK(func__AdvanceControl(2000u) == false);
    CHECK((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != 0u);
    func__CheckPersistedState(2u, 3u, 1u);

    func__SetMeasurements(12500u, 12500u, 24000u, true);
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    CHECK((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != 0u);
    func__CheckPersistedState(2u, 3u, 1u);
    CHECK(func__AdvanceControl(990u) == false);
    CHECK((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != 0u);
    CHECK(func__AdvanceControl(10u) == false);
    CHECK(func__Fault_Get() == FAULT_NONE);
    func__CheckPersistedState(2u, 3u, 1u);

    /* ---- 3. No mains + 5 V is not the hard-open signature ---- */
    func__ResetWorld(30000u);
    func__SetPersistedState(3u, 2u, 1u);
    func__SetMeasurements(12000u, 5000u, 0u, false);
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    CHECK(func__Fault_Get() == FAULT_NONE);
    CHECK(func__AdvanceControl(5000u) == false);
    func__CheckPersistedState(3u, 2u, 1u);

    /* ---- 4. Invalid measurements break the 3 s continuous proof ---- */
    func__ResetWorld(40000u);
    func__SetPersistedState(4u, 2u, 1u);
    func__SetMeasurements(12000u, FAULT_BAT_HARD_ABSENT_MV - 1u, 0u, false);
    CHECK(func__ControlPass() == false);
    CHECK(func__AdvanceControl(1990u) == false);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__AdvanceControl(1000u) == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    func__CheckPersistedState(4u, 2u, 1u);

    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == true);
    CHECK(func__AdvanceControl(2990u) == false);
    func__CheckPersistedState(4u, 2u, 1u);
    CHECK(func__AdvanceControl(10u) == true);
    func__CheckPersistedState(0u, 0u, 0u);

    printf("checks: %" PRId32 ", fails: %" PRId32 "\n",
           INT32_T__G__Checks,
           INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
