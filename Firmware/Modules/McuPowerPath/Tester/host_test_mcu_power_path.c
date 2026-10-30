/**
 * @file    host_test_mcu_power_path.c
 * @brief   [EN] Host unit test for the McuPowerPath module. The PRODUCTION
 *              mcu_power_path.c is compiled as-is and driven through a fake
 *              clock, fake GPIO and a fake measurement snapshot.
 *          [FA] تست هاست ماژول McuPowerPath: کد محصول بدون تغییر، با ساعت،
 *              GPIO و snapshot بدلی رانده می‌شود.
 *
 * @note    [EN] What this module decides: whether the MCU runs from the
 *              battery or from the input. The healthy window is the shared
 *              battery or from the input. The voltage decision is the one
 *              shared Fault input Schmitt permission (panel q33/q34 window,
 *              q169/q170 re-arm margins). With the factory 21000/28000 window
 *              and 500/500 margins, the battery is dropped only after the
 *              shared permission stays true for 5 s AND the presence pin still
 *              agrees; it is restored immediately when the permission drops
 *              (outside 21000..28000) or when the presence IRQ fires. The
 *              switch line is ACTIVE LOW: writing true means "battery on".
 *          [FA] تصمیم این ماژول: تغذیهٔ MCU از باتری یا از ورودی. تصمیم ولتاژی
 *              همان مجوز مشترک اشمیت ورودی فالت است (پنجرهٔ ‎q33/q34‎ و
 *              حاشیه‌های برگشت ‎q169/q170‎). با پنجرهٔ کارخانه ‎21000/28000‎ و
 *              حاشیه‌های ۵۰۰/۵۰۰، باتری فقط وقتی برداشته می‌شود که مجوز مشترک
 *              پنج ثانیه درست بماند و پایهٔ حضور هم تأیید کند؛ با افتادن مجوز
 *              (بیرون ‎21000..28000‎) یا وقفهٔ حضور فوراً برمی‌گردد. خط کلید
 *              ‎active-low‎ است: true یعنی «باتری روشن».
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "mcu_power_path.h"
#include "bsp_gpio.h"
#include "measurement.h"
#include "fault.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake world / دنیای بدلی ==================== */

static uint32_t UINT32_T__G__FakeTick     = 0u;
static bool     BOOL__G__BatterySwitch    = true;   /* [EN] true = battery on */
static bool     BOOL__G__PresencePin      = true;
static bool     BOOL__G__SnapshotOk       = true;
static uint32_t UINT32_T__G__InputMv      = 24000u;
static uint32_t UINT32_T__G__SwitchWrites = 0u;
static uint32_t UINT32_T__G__WindowMinMv  = MCU_POWER_INPUT_DEFAULT_MIN_MV;
static uint32_t UINT32_T__G__WindowMaxMv  = MCU_POWER_INPUT_DEFAULT_MAX_MV;
static uint32_t UINT32_T__G__HystLowMv    = 500u;
static uint32_t UINT32_T__G__HystHighMv   = 500u;
static bool     BOOL__G__InputArmed       = false;

/* [EN] Host stub of the shared Fault window. The production module reads the
       live q33/q34 pair through this API; the test drives it directly.
   [FA] بدل هاست پنجرهٔ مشترک Fault. ماژول محصول جفت زندهٔ ‎q33/q34‎ را از این
       API می‌خواند؛ تست آن را مستقیم می‌راند. */
bool func__Fault_GetInputWindow(uint32_t *uint32_t__minMv, uint32_t *uint32_t__maxMv)
{
    if ((uint32_t__minMv == NULL) || (uint32_t__maxMv == NULL))
    {
        return false;
    }
    *uint32_t__minMv = UINT32_T__G__WindowMinMv;
    *uint32_t__maxMv = UINT32_T__G__WindowMaxMv;
    return true;
}

/* [EN] Host stub of the shared Fault Schmitt permission. It mirrors the
       production rule: outside the raw window drop at once; re-arm only
       inside [min+H_low, max-H_high], each margin capped at span/4.
   [FA] بدل هاست مجوز مشترک اشمیت فالت. همان قاعدهٔ محصول: بیرون پنجرهٔ خام
       فوراً بیفت؛ فقط داخل [‎min+H_low, max-H_high]‎ مسلح شو، هر حاشیه حداکثر
       یک‌چهارم فاصله. */
bool func__Fault_GetInputPermission(bool *bool__snapshotValid, bool *bool__permitted)
{
    uint32_t uint32_t__capMv;
    uint32_t uint32_t__hl;
    uint32_t uint32_t__hh;

    if ((bool__snapshotValid == NULL) || (bool__permitted == NULL))
    {
        return false;
    }
    *bool__snapshotValid = BOOL__G__SnapshotOk;
    if (BOOL__G__SnapshotOk == false)
    {
        BOOL__G__InputArmed = false;
        *bool__permitted = false;
        return true;
    }
    uint32_t__capMv = (UINT32_T__G__WindowMaxMv - UINT32_T__G__WindowMinMv) / 4u;
    if (uint32_t__capMv > 3000u)
    {
        uint32_t__capMv = 3000u;
    }
    uint32_t__hl = UINT32_T__G__HystLowMv;
    uint32_t__hh = UINT32_T__G__HystHighMv;
    if (uint32_t__hl > uint32_t__capMv) { uint32_t__hl = uint32_t__capMv; }
    if (uint32_t__hh > uint32_t__capMv) { uint32_t__hh = uint32_t__capMv; }
    if ((UINT32_T__G__InputMv < UINT32_T__G__WindowMinMv) ||
        (UINT32_T__G__InputMv > UINT32_T__G__WindowMaxMv))
    {
        BOOL__G__InputArmed = false;
    }
    else if (BOOL__G__InputArmed == false)
    {
        BOOL__G__InputArmed =
            ((UINT32_T__G__InputMv >= (UINT32_T__G__WindowMinMv + uint32_t__hl)) &&
             (UINT32_T__G__InputMv <= (UINT32_T__G__WindowMaxMv - uint32_t__hh)));
    }
    else
    {
        /* [EN] Armed: stay armed inside the raw window. [FA] مسلح: داخل پنجره خام مسلح بمان. */
    }
    *bool__permitted = BOOL__G__InputArmed;
    return true;
}

uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}

uint32_t func__Rtos_TicksToMilliseconds(uint32_t uint32_t__ticks)
{
    return uint32_t__ticks;             /* [EN] 1 kHz kernel / کرنل ۱kHz */
}

void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted)
{
    /* [EN] This module may only drive the MCU battery switch.
       [FA] این ماژول فقط اجازهٔ راندن کلید باتری MCU را دارد. */
    CHECK(bsp_gpio_id_t__id == BSP_GPIO_BATTERY_SWITCH);
    BOOL__G__BatterySwitch = bool__asserted;
    UINT32_T__G__SwitchWrites++;
}

bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id)
{
    CHECK(bsp_gpio_id_t__id == BSP_GPIO_INPUT_24V_PRESENT);
    return BOOL__G__PresencePin;
}

/* ==================== BspGpio_DisconnectBatteryPathIfInputPresent ==================== */
/**
 * [EN] Host stub of the atomic BSP pair: same contract (only cut when the
 *      presence pin is high), no interrupts to mask off-target.
 * [FA] بدل هاست برای جفت اتمیک BSP: همان قرارداد (فقط با حضور ورودی قطع
 *      می‌کند) بدون وقفه‌ای برای ماسک‌کردن.
 */
bool func__BspGpio_DisconnectBatteryPathIfInputPresent(void)
{
    if (BOOL__G__PresencePin == false)
    {
        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true);
        return false;
    }

    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false);
    return true;
}

bool func__Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t__out)
{
    if (measurement_snapshot_t__out == NULL)
    {
        return false;
    }

    measurement_snapshot_t__out->v_in_mv       = UINT32_T__G__InputMv;
    measurement_snapshot_t__out->v_bat24_mv    = 25000u;
    measurement_snapshot_t__out->v_bat12_mv    = 12500u;
    measurement_snapshot_t__out->v_bat_low_mv  = 12500u;
    measurement_snapshot_t__out->v_bat_high_mv = 12500u;
    measurement_snapshot_t__out->i_ch1_ma      = 0u;
    measurement_snapshot_t__out->i_ch2_ma      = 0u;
    measurement_snapshot_t__out->input_present = BOOL__G__PresencePin;
    measurement_snapshot_t__out->valid         = BOOL__G__SnapshotOk;

    return BOOL__G__SnapshotOk;
}

/* ==================== Test driver / رانندهٔ تست ==================== */

/**
 * @brief  [EN] Run the module for the given time, one pass per 10 ms.
 *         [FA] ماژول را به‌اندازهٔ زمان داده‌شده، هر ۱۰ms یک پاس، اجرا می‌کند.
 */
static void func__RunMs(uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__elapsed;

    for (uint32_t__elapsed = 0u;
         uint32_t__elapsed < uint32_t__milliseconds;
         uint32_t__elapsed += 10u)
    {
        UINT32_T__G__FakeTick += 10u;
        func__McuPowerPath_Run();
    }
}

static void func__Reset(uint32_t uint32_t__startTick)
{
    UINT32_T__G__FakeTick     = uint32_t__startTick;
    BOOL__G__PresencePin      = true;
    BOOL__G__SnapshotOk       = true;
    UINT32_T__G__InputMv      = 24000u;
    UINT32_T__G__SwitchWrites = 0u;
    UINT32_T__G__WindowMinMv  = MCU_POWER_INPUT_DEFAULT_MIN_MV;
    UINT32_T__G__WindowMaxMv  = MCU_POWER_INPUT_DEFAULT_MAX_MV;
    UINT32_T__G__HystLowMv    = 500u;
    UINT32_T__G__HystHighMv   = 500u;
    BOOL__G__InputArmed       = false;

    func__McuPowerPath_Init();
}

int main(void)
{
    printf("== McuPowerPath host test ==\n");

    /* ---- 1. Init parks the MCU on the battery (the safe side) ---- */
    func__Reset(1000u);
    CHECK(BOOL__G__BatterySwitch == true);
    CHECK(UINT32_T__G__SwitchWrites == 1u);

    /* ---- 2. a good input must hold for the full 5 s before the drop ---- */
    func__RunMs(MCU_POWER_INPUT_STABLE_MS - 100u);
    CHECK(BOOL__G__BatterySwitch == true);
    func__RunMs(200u);
    CHECK(BOOL__G__BatterySwitch == false);     /* [EN] battery released */

    /* [EN] Once released, further passes must not keep re-writing the pin.
       [FA] پس از برداشتن، پایه نباید بی‌دلیل دوباره نوشته شود. */
    UINT32_T__G__SwitchWrites = 0u;
    func__RunMs(10000u);
    CHECK(UINT32_T__G__SwitchWrites == 0u);
    CHECK(BOOL__G__BatterySwitch == false);

    /* ---- 3. the input sagging under the raw lower edge restores the battery at once ---- */
    UINT32_T__G__InputMv = (MCU_POWER_INPUT_DEFAULT_MIN_MV - 1u);
    func__RunMs(10u);
    CHECK(BOOL__G__BatterySwitch == true);

    /* ---- 4. inside the raw window but below the re-arm margin: no drop ----
       [EN] The shared permission is still false, so the 5 s timer must not
            run and the battery stays connected.
       [FA] مجوز مشترک هنوز نادرست است، پس تایمر ۵ ثانیه نباید بدود و باتری
            وصل می‌ماند. */
    func__Reset(20000u);
    UINT32_T__G__InputMv = (MCU_POWER_INPUT_DEFAULT_MIN_MV + 200u);
    UINT32_T__G__SwitchWrites = 0u;
    func__RunMs(30000u);
    CHECK(BOOL__G__BatterySwitch == true);
    CHECK(UINT32_T__G__SwitchWrites == 0u);

    /* [EN] Leaving the band upwards still needs a FULL fresh 5 s.
       [FA] خروج از نوار به بالا هم به ۵ ثانیهٔ کامل تازه نیاز دارد. */
    UINT32_T__G__InputMv = (MCU_POWER_INPUT_DEFAULT_MIN_MV + 500u);
    func__RunMs(MCU_POWER_INPUT_STABLE_MS - 100u);
    CHECK(BOOL__G__BatterySwitch == true);
    func__RunMs(200u);
    CHECK(BOOL__G__BatterySwitch == false);

    /* ---- 5. a dip in the middle of the 5 s restarts the clock ---- */
    func__Reset(40000u);
    func__RunMs(3000u);
    UINT32_T__G__InputMv = (MCU_POWER_INPUT_DEFAULT_MIN_MV - 1u);
    func__RunMs(100u);                                   /* [EN] timer killed */
    UINT32_T__G__InputMv = 24000u;
    func__RunMs(MCU_POWER_INPUT_STABLE_MS - 100u);
    CHECK(BOOL__G__BatterySwitch == true);               /* [EN] not 5 s yet */
    func__RunMs(200u);
    CHECK(BOOL__G__BatterySwitch == false);

    /* ---- 6. an unusable snapshot freezes the decision ----
       [EN] No reading means no verdict: the module must neither drop the
            battery nor count time.
       [FA] نبود اندازه‌گیری یعنی نبود قضاوت: نه برداشتن باتری، نه شمارش زمان. */
    func__Reset(60000u);
    BOOL__G__SnapshotOk = false;
    UINT32_T__G__SwitchWrites = 0u;
    func__RunMs(30000u);
    CHECK(BOOL__G__BatterySwitch == true);
    CHECK(UINT32_T__G__SwitchWrites == 0u);

    /* ---- 7. the presence pin has a veto at the moment of the drop ----
       [EN] Voltage says "good" for 5 s but the hardware presence pin says
            "gone": the battery must stay on.
       [FA] ولتاژ ۵ ثانیه «خوب» می‌گوید ولی پایهٔ حضور «نیست» می‌گوید: باتری
            باید روشن بماند. */
    func__Reset(80000u);
    BOOL__G__PresencePin = false;
    func__RunMs(MCU_POWER_INPUT_STABLE_MS + 500u);
    CHECK(BOOL__G__BatterySwitch == true);

    /* ---- 8. the input-lost interrupt restores the battery immediately ---- */
    func__Reset(100000u);
    func__RunMs(MCU_POWER_INPUT_STABLE_MS + 100u);
    CHECK(BOOL__G__BatterySwitch == false);
    BOOL__G__PresencePin = false;
    func__McuPowerPath_OnInputIrq();
    CHECK(BOOL__G__BatterySwitch == true);

    /* [EN] The same interrupt while the input is still present must NOT
            touch the switch (it is an input-lost handler, not a toggle).
       [FA] همان وقفه وقتی ورودی هنوز هست نباید کلید را عوض کند. */
    func__Reset(120000u);
    func__RunMs(MCU_POWER_INPUT_STABLE_MS + 100u);
    CHECK(BOOL__G__BatterySwitch == false);
    BOOL__G__PresencePin = true;
    UINT32_T__G__SwitchWrites = 0u;
    func__McuPowerPath_OnInputIrq();
    CHECK(UINT32_T__G__SwitchWrites == 0u);
    CHECK(BOOL__G__BatterySwitch == false);

    /* ---- 9. the 49.7-day tick wrap must not break the 5 s window ---- */
    func__Reset(0xFFFFF000u);
    func__RunMs(MCU_POWER_INPUT_STABLE_MS - 100u);
    CHECK(BOOL__G__BatterySwitch == true);
    func__RunMs(200u);
    CHECK(BOOL__G__BatterySwitch == false);
    CHECK(UINT32_T__G__FakeTick < 0xFFFFF000u);          /* [EN] really wrapped */

    /* ---- 10. the shared upper edge reconnects the battery too ----
       [EN] Factory window 21000..28000 with 500/500 re-arm margins:
            permission drops above 28000; re-arm needs <=27500.
       [FA] پنجرهٔ کارخانه ‎21000..28000‎ با حاشیه‌های برگشت ۵۰۰/۵۰۰:
            مجوز بالای ۲۸۰۰۰ می‌افتد؛ برگشت به <=۲۷۵۰۰ نیاز دارد. */
    func__Reset(140000u);
    func__RunMs(MCU_POWER_INPUT_STABLE_MS + 100u);
    CHECK(BOOL__G__BatterySwitch == false);
    UINT32_T__G__InputMv = MCU_POWER_INPUT_DEFAULT_MAX_MV + 1u;
    func__RunMs(10u);
    CHECK(BOOL__G__BatterySwitch == true);

    /* [EN] Inside the raw window but above the upper re-arm margin: the
           shared permission is false, so the battery stays connected and the
           timer does not run; clearing the margin needs a fresh 5 s.
       [FA] داخل پنجرهٔ خام ولی بالای حاشیهٔ برگشت بالا: مجوز مشترک نادرست
           است، پس باتری وصل می‌ماند و تایمر نمی‌دود؛ ردکردن حاشیه به ۵ ثانیهٔ
           کامل تازه نیاز دارد. */
    func__Reset(150000u);
    UINT32_T__G__InputMv = MCU_POWER_INPUT_DEFAULT_MAX_MV - 200u;
    UINT32_T__G__SwitchWrites = 0u;
    func__RunMs(30000u);
    CHECK(BOOL__G__BatterySwitch == true);
    CHECK(UINT32_T__G__SwitchWrites == 0u);
    UINT32_T__G__InputMv = MCU_POWER_INPUT_DEFAULT_MAX_MV - 500u;
    func__RunMs(MCU_POWER_INPUT_STABLE_MS - 100u);
    CHECK(BOOL__G__BatterySwitch == true);
    func__RunMs(200u);
    CHECK(BOOL__G__BatterySwitch == false);

    /* ---- 11. a live panel window moves both edges ---- */
    func__Reset(160000u);
    UINT32_T__G__WindowMinMv = 22000u;
    UINT32_T__G__WindowMaxMv = 26000u;
    /* [EN] span=4000, margins 500/500: re-arm 22500..25500,
           drop outside 22000..26000. 24000 qualifies.
       [FA] فاصله=۴۰۰۰، حاشیه‌های ۵۰۰/۵۰۰: برگشت ‎22500..25500‎،
           افتادن بیرون ‎22000..26000. 24000‎ احراز می‌شود. */
    func__RunMs(MCU_POWER_INPUT_STABLE_MS + 100u);
    CHECK(BOOL__G__BatterySwitch == false);
    UINT32_T__G__InputMv = 26001u;
    func__RunMs(10u);
    CHECK(BOOL__G__BatterySwitch == true);
    UINT32_T__G__InputMv = 21999u;
    func__RunMs(10u);
    CHECK(BOOL__G__BatterySwitch == true);

    /* ---- 12. a narrow panel window clamps the margins, never inverts ---- */
    func__Reset(170000u);
    UINT32_T__G__WindowMinMv = 24000u;
    UINT32_T__G__WindowMaxMv = 25000u;
    /* [EN] span=1000 -> each margin caps at 250: re-arm 24250..24750,
           drop outside 24000..25000.
       [FA] فاصله=۱۰۰۰ ← هر حاشیه حداکثر ۲۵۰: برگشت ‎24250..24750‎،
           افتادن بیرون ‎24000..25000‎. */
    UINT32_T__G__InputMv = 24500u;
    func__RunMs(MCU_POWER_INPUT_STABLE_MS + 100u);
    CHECK(BOOL__G__BatterySwitch == false);
    UINT32_T__G__InputMv = 25001u;
    func__RunMs(10u);
    CHECK(BOOL__G__BatterySwitch == true);
    UINT32_T__G__InputMv = 24200u;
    UINT32_T__G__SwitchWrites = 0u;
    func__RunMs(30000u);
    CHECK(BOOL__G__BatterySwitch == true);
    CHECK(UINT32_T__G__SwitchWrites == 0u);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
