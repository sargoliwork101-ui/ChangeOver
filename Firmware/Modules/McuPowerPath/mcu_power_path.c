/**
 * @file    mcu_power_path.c
 * @brief   [EN] MCU self-supply path via Q1 (PB5) - owns battery switch independent from Changeover.
 *              PB5 (active-low): true->Low=on, false->High=off. PB11 remains Changeover-only.
 *              Healthy input window is the shared panel pair q33/q34 (Fault).
 *              Internal margins: qualify inside [q33+QM, q34-QM] for 5000ms → Q1 off (High);
 *              outside [q33+RM, q34-RM] → Q1 on (Low); between → cancel pending timer, preserve Q1;
 *              PB4 falling edge immediate Low in ISR.
 */
#include "mcu_power_path.h"
/* ==================== Includes ==================== */
#include "modules_enable.h"
#include "bsp_gpio.h"
#include "measurement.h"
#include "rtos_time.h"
#include "cmsis_os2.h"
#if MODULE_FAULT
#include "fault.h"
#endif
/* ==================== Constants ==================== */
/* Uses MCU_POWER_INPUT_STABLE_MS and the internal qualify/reconnect margins from header. */
/* ==================== Internal state ==================== */
/* ISR-safe: accessed from both ISR (OnInputIrq) and task (Run). */
static volatile bool     BOOL__G__McuPowerTimerActive = false;
static volatile uint32_t UINT32_T__G__McuPowerStartTick  = 0u;
static volatile bool     BOOL__G__BatteryConnected     = true;
/* ==================== McuPowerPath Init ==================== */
/* ==================== McuPowerPath_Init ==================== */
/**
 * @brief  [EN] Put the MCU self-supply path into its safe boot state: the
 *              battery switch is driven to CONNECTED and the qualification
 *              timer is cleared. Connected is the only correct power-on
 *              state, because the board has to be able to run from the
 *              battery before anyone has proved that the DC input is
 *              present and healthy. Call this once before the product
 *              tasks start. It deliberately never touches PB11, which
 *              belongs to Changeover alone.
 */
void func__McuPowerPath_Init(void)
{
    /*
 * [EN] Battery path must be connected at boot so the MCU can run without DC input.
 */
    BOOL__G__BatteryConnected     = true;
    BOOL__G__McuPowerTimerActive = false;
    UINT32_T__G__McuPowerStartTick   = 0u;
    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* active-low -> Low = on */
}
/* ==================== McuPowerPath OnInputIrq ==================== */
/**
 * @brief  [EN] Interrupt-context handler for the PB4 input-present edges.
 *              When PB4 says the DC input is gone, the battery path is
 *              reconnected immediately and any pending disconnect timer is
 *              cancelled - waiting for the next periodic pass could brown
 *              the MCU out. The body is restricted to one GPIO write and
 *              two volatile flags: no RTOS call, no ADC, no mutex and no
 *              delay, so it is safe from an ISR. A rising edge does NOT
 *              disconnect here; that decision needs the 5 s qualification
 *              and the hysteresis band, which only the periodic Run owns.
 */
void func__McuPowerPath_OnInputIrq(void)
{
    /*
 * [EN] PB4 is BSP_GPIO_INPUT_24V_PRESENT; level low means input absent.
 */
    bool bool__inputPresent = func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);
    if (bool__inputPresent == false)
    {
        /*
 * [EN] Emergency reconnect: battery must be on immediately, timer cancelled.
         *      Order matters: PB5 Low first, then flag, so even if task preempts after this,
         *      the hardware is already safe.
 */
        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* Low = battery on */
        BOOL__G__BatteryConnected     = true;
        BOOL__G__McuPowerTimerActive = false;
        /*
 * [EN] Start tick left as-is; will be re-armed on next qualify.
 */
    }
    /*
 * [EN] On rising edge (input present) do NOT disconnect here; let Run qualify 5 s and apply hysteresis.
 */
}
/* ==================== McuPowerPath Run ==================== */
/**
 * @brief  [EN] Periodic qualification (~10 ms) on the shared Fault input
 *         Schmitt permission. While the shared permission is true the 5 s
 *         timer runs and then Q1 opens; when it is false Q1 closes again.
 *         Invalid snapshot preserves Q1 and cancels the timer. PB4 falling
 *         is still handled immediately in the ISR.
 */
void func__McuPowerPath_Run(void)
{
#if !MODULE_MCU_POWER_PATH
    return;
#else
    /*
 * [EN] Determine DC input band via measured bus voltage with hysteresis.
 */
    bool bool__inputQualify = false;
    bool bool__inputLow     = false;
    uint32_t uint32_t__nowTick;
#if MODULE_MEASUREMENT
    bool bool__snapshotUsable;
#if MODULE_FAULT
    {
        bool bool__permissionValid = false;
        bool bool__permitted = false;
        /*
 * [EN] The shared Fault Schmitt flag is the only voltage decision.
 * The 5 s qualification below still runs, but it runs on exactly the
 * same permission the charger and UI use. Invalid snapshot preserves
 * Q1; a released permission reconnects the battery.
 */
        bool__snapshotUsable =
            ((func__Fault_GetInputPermission(&bool__permissionValid, &bool__permitted) == true) &&
             (bool__permissionValid != false));
        if (bool__snapshotUsable != false)
        {
            bool__inputQualify = bool__permitted;
            bool__inputLow     = (bool__permitted == false);
        }
    }
#else
    {
        measurement_snapshot_t measurement_snapshot_t__snap;
        static bool BOOL__S__InputArmed = false;
        uint32_t uint32_t__spanMv = MCU_POWER_INPUT_DEFAULT_MAX_MV - MCU_POWER_INPUT_DEFAULT_MIN_MV;
        uint32_t uint32_t__marginMv = MCU_POWER_INPUT_QUALIFY_MARGIN_MV;
        if (uint32_t__marginMv > (uint32_t__spanMv / 4u))
        {
            uint32_t__marginMv = uint32_t__spanMv / 4u;
        }
        bool__snapshotUsable =
            ((func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) != false) &&
             (measurement_snapshot_t__snap.valid != false));
        if (bool__snapshotUsable != false)
        {
            if ((measurement_snapshot_t__snap.v_in_mv < MCU_POWER_INPUT_DEFAULT_MIN_MV) ||
                (measurement_snapshot_t__snap.v_in_mv > MCU_POWER_INPUT_DEFAULT_MAX_MV))
            {
                BOOL__S__InputArmed = false;
            }
            else if (BOOL__S__InputArmed == false)
            {
                BOOL__S__InputArmed =
                    ((measurement_snapshot_t__snap.v_in_mv >= (MCU_POWER_INPUT_DEFAULT_MIN_MV + uint32_t__marginMv)) &&
                     (measurement_snapshot_t__snap.v_in_mv <= (MCU_POWER_INPUT_DEFAULT_MAX_MV - uint32_t__marginMv)));
            }
            else
            {
            }
            bool__inputQualify = BOOL__S__InputArmed;
            bool__inputLow     = (BOOL__S__InputArmed == false);
        }
    }
#endif
    if (bool__snapshotUsable == false)
    {
        /*
 * [EN] Snapshot invalid: cannot qualify nor reconnect by voltage; preserve Q1, cancel pending timer.
         *      Timer is not counted while invalid (like Changeover snapshot-first).
 */
        bool__inputQualify = false;
        bool__inputLow     = false;
    }
#else
    /*
 * [EN] Fallback when measurement is off: use PB4 level directly (no voltage hysteresis).
     *      PB4 High → qualify band; PB4 Low → low band (should already be handled in ISR).
 */
    if (func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT) != false)
    {
        bool__inputQualify = true;
    }
    else
    {
        bool__inputLow = true;
    }
#endif
    uint32_t__nowTick = osKernelGetTickCount();
    /* ==================== Hysteresis band handling ==================== */
    if (bool__inputQualify != false)
    {
        /*
 * [EN] Inside the shared qualify band: qualify for 5s disconnect.
 */
        if (BOOL__G__BatteryConnected == false)
        {
            /*
 * [EN] Already disconnected after prior qualification; stay off,
             *      no timer (the internal dead-band prevents edge chatter).
 */
            BOOL__G__McuPowerTimerActive = false;
            return;
        }
        if (BOOL__G__McuPowerTimerActive == false)
        {
            /*
 * [EN] Arrival of qualifying input starts 5 s qualification.
             *      Also covers boot-with-input-present (no missing rising edge).
 */
            BOOL__G__McuPowerTimerActive = true;
            UINT32_T__G__McuPowerStartTick  = uint32_t__nowTick;
        }
        else
        {
            uint32_t uint32_t__elapsedTicks = uint32_t__nowTick - UINT32_T__G__McuPowerStartTick;
            uint32_t uint32_t__elapsedMs    = func__Rtos_TicksToMilliseconds(uint32_t__elapsedTicks);
            if (uint32_t__elapsedMs >= MCU_POWER_INPUT_STABLE_MS)
            {
                /*
 * [EN] Stable 5 s achieved -> disconnect MCU battery path (Q1 off = PB5 High).
                 *      Must not touch PB11/Q17.
 */
                /*
 * [EN] Live PB4 veto: the snapshot can be stale - if the
                 *      input died in the very pass the 5 s elapsed, cutting
                 *      now would brown out the MCU. PB4 is real-time
                 *      hardware: absent => take the reconnect path instead.
 */
                /*
 * [EN] Presence read + pin write must be one atomic step: the
 * PB4 EXTI handler reconnects the battery the moment the input
 * dies, and a plain read-then-write left a window where that
 * emergency reconnect was overwritten by this stale pass -
 * the MCU could then run on no supply at all. The BSP owns the
 * short masked pair; it returns false when it kept the battery
 * on because the input had already gone.
 */
                BOOL__G__BatteryConnected     = false;
                BOOL__G__McuPowerTimerActive = false;
                if (func__BspGpio_DisconnectBatteryPathIfInputPresent() == false)
                {
                    BOOL__G__BatteryConnected = true;
                }
            }
        }
    }
    else if (bool__inputLow != false)
    {
        /*
 * [EN] Outside the shared reconnect band: reconnect battery (also
         *      covers falling PB4 via ISR, but ADC path needs it too).
         *      Cancel pending disconnect timer.
 */
        if (BOOL__G__McuPowerTimerActive != false)
        {
            BOOL__G__McuPowerTimerActive = false;
        }
        if (BOOL__G__BatteryConnected == false)
        {
            func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* Low = battery on */
            BOOL__G__BatteryConnected = true;
        }
        /*
 * [EN] If already connected, just keep it on.
 */
    }
    else
    {
        /*
 * [EN] Hysteresis dead-band near either shared edge, or invalid
         *      snapshot: cancel pending timer, preserve Q1. Do NOT reconnect
         *      merely because voltage moved inside the dead-band.
 */
        if (BOOL__G__McuPowerTimerActive != false)
        {
            BOOL__G__McuPowerTimerActive = false;
        }
        /*
 * [EN] Battery voltage change alone does not affect Q1 in this band.
 */
    }
#endif
}
