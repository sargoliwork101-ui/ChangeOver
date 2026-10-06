/**
 * @file    host_test_mcu_power_path.c
 * @brief   [EN] Host unit test for the McuPowerPath module. The PRODUCTION
 *              mcu_power_path.c is compiled as-is and driven through a fake
 *              clock, fake GPIO and a fake measurement snapshot.
 *          [FA] تست هاست ماژول McuPowerPath: کد محصول بدون تغییر، با ساعت،
 *              GPIO و snapshot بدلی رانده می‌شود.
 *
 * @note    [EN] What this module decides: whether the MCU runs from the
 *              battery or from the input. The battery is dropped only after
 *              the input has stayed at or above 22000 mV for 5 s AND the
 *              presence pin still agrees; it is restored immediately when
 *              the input falls under 21500 mV or the presence IRQ fires.
 *              The switch line is ACTIVE LOW: writing true means "battery
 *              on".
 *          [FA] تصمیم این ماژول: تغذیهٔ MCU از باتری یا از ورودی. باتری فقط
 *              وقتی برداشته می‌شود که ورودی ۵ ثانیه ≥ ۲۲۰۰۰ بماند و پایهٔ
 *              حضور هم تأیید کند؛ با افت زیر ۲۱۵۰۰ یا وقفهٔ حضور، فوراً
 *              برمی‌گردد. خط کلید ‎active-low‎ است: نوشتن true یعنی «باتری روشن».
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "mcu_power_path.h"
#include "bsp_gpio.h"
#include "measurement.h"

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

    /* ---- 3. the input sagging under 21500 mV restores the battery at once ---- */
    UINT32_T__G__InputMv = MCU_POWER_INPUT_RECONNECT_MV - 1u;
    func__RunMs(10u);
    CHECK(BOOL__G__BatterySwitch == true);

    /* ---- 4. the hysteresis band (21500..21999) decides NOTHING ----
       [EN] Inside the band the module neither drops nor restores, and the
            5 s timer must not run - otherwise a slowly drifting input would
            eventually drop the battery from inside the dead band.
       [FA] داخل نوار هیسترزیس نه قطع و نه وصل، و تایمر ۵ ثانیه هم نباید
            بدود؛ وگرنه ورودیِ آهسته‌رو از داخل نوار مرده باتری را برمی‌داشت. */
    func__Reset(20000u);
    UINT32_T__G__InputMv = MCU_POWER_INPUT_RECONNECT_MV + 100u;
    UINT32_T__G__SwitchWrites = 0u;
    func__RunMs(30000u);
    CHECK(BOOL__G__BatterySwitch == true);
    CHECK(UINT32_T__G__SwitchWrites == 0u);

    /* [EN] Leaving the band upwards still needs a FULL fresh 5 s.
       [FA] خروج از نوار به بالا هم به ۵ ثانیهٔ کامل تازه نیاز دارد. */
    UINT32_T__G__InputMv = MCU_POWER_INPUT_QUALIFY_MV;
    func__RunMs(MCU_POWER_INPUT_STABLE_MS - 100u);
    CHECK(BOOL__G__BatterySwitch == true);
    func__RunMs(200u);
    CHECK(BOOL__G__BatterySwitch == false);

    /* ---- 5. a dip in the middle of the 5 s restarts the clock ---- */
    func__Reset(40000u);
    func__RunMs(3000u);
    UINT32_T__G__InputMv = MCU_POWER_INPUT_RECONNECT_MV - 1u;
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

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
