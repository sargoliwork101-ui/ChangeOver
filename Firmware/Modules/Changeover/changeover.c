/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine - timed cut/reconnect with 3000ms filtering.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - قطع/وصل با فیلتر ۳ ثانیه.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, BOOL__G__UiBatteryAlarmIssued.
 *              No board_pins.h, no HAL, no PB7, no float/queue/task.
 *              Time conversion ONLY via rtos_time.h (MillisecondsToTicks); tick=1ms assumption is forbidden.
 *              PB11 (BSP_GPIO_PROTECT_BATTERY) and PB5 (BSP_GPIO_BATTERY_SWITCH)
 *              are the two battery-path controls. PB7 relay remains Charger-owned.
 *          [FA] فقط از داده‌های مجاز بالا استفاده می‌کند و PB11 و PB5 را برای مسیر باتری می‌زند؛ PB7 در اختیار Charger است.
 */

#include "changeover.h"
#include "bsp_gpio.h"
#include "rtos_time.h"
#include "cmsis_os2.h"

#include <stdbool.h>
#include <stdint.h>

/* ==================== External UI Flag / فلگ خارجی UI ==================== */

/**
 * @brief  [EN] UI-owned global battery alarm flag. Changeover reads only.
 *         [FA] فلگ سراسری در مالکیت UI؛ Changeover فقط می‌خواند.
 */
extern volatile bool BOOL__G__UiBatteryAlarmIssued;

/* ==================== Static State / وضعیت داخلی ==================== */

/**
 * @brief  [EN] Current system state.
 *         [FA] حالت فعلی سیستم.
 */
static app_state_t APP_STATE_T__G__State = APP_STATE_BOOT;

/**
 * @brief  [EN] Logical PB11 protect state: true = battery path cut (protect asserted).
 *         [FA] وضعیت منطقی PB11: true یعنی مسیر باتری قطع شده است.
 */
static bool BOOL__G__ChangeoverProtectAsserted = false;

/**
 * @brief  [EN] Logical PB5 battery-switch state: true = switch ON (battery connected).
 *         [FA] وضعیت منطقی PB5: true یعنی سوئیچ باتری روشن و مسیر باتری وصل است.
 */
static bool BOOL__G__ChangeoverBatterySwitchOn = false;

/* ==================== Battery path helper / کمک مسیر باتری ==================== */

/**
 * @brief  [EN] Apply the two-pin battery path: ON = PB5 ON + PB11 deasserted,
 *              OFF = PB5 OFF + PB11 asserted. Keeps internal shadow state in sync.
 *         [FA] مسیر دوپایه باتری را اعمال می‌کند: روشن = PB5 روشن + PB11 آزاد،
 *              خاموش = PB5 خاموش + PB11 فعال. وضعیت سایه داخلی را همگام نگه می‌دارد.
 * @param  bool__batteryOn [EN] true = battery path ON / مسیر باتری روشن
 */
static void func__Changeover_ApplyBatteryPath(bool bool__batteryOn)
{
    if (bool__batteryOn == true)
    {
        if (BOOL__G__ChangeoverBatterySwitchOn == false)
        {
            func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true);
            BOOL__G__ChangeoverBatterySwitchOn = true;
        }
        if (BOOL__G__ChangeoverProtectAsserted == true)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
            BOOL__G__ChangeoverProtectAsserted = false;
        }
    }
    else
    {
        if (BOOL__G__ChangeoverBatterySwitchOn == true)
        {
            func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false);
            BOOL__G__ChangeoverBatterySwitchOn = false;
        }
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
            BOOL__G__ChangeoverProtectAsserted = true;
        }
    }
}

/**
 * @brief  [EN] Tick at which the current cut condition became continuously true.
 *         [FA] تیکی که در آن شرط قطع به‌صورت پیوسته true شد.
 */
static uint32_t TICK_T__G__CutStartTick = 0u;

/**
 * @brief  [EN] Whether a cut condition is currently being timed.
 *         [FA] آیا شرط قطع در حال زمان‌گیری است.
 */
static bool BOOL__G__CutTimerActive = false;

/**
 * @brief  [EN] Tick at which the reconnect condition became continuously true.
 *         [FA] تیکی که در آن شرط وصل مجدد به‌صورت پیوسته true شد.
 */
static uint32_t TICK_T__G__ReconnectStartTick = 0u;

/**
 * @brief  [EN] Whether a reconnect condition is currently being timed.
 *         [FA] آیا شرط وصل مجدد در حال زمان‌گیری است.
 */
static bool BOOL__G__ReconnectTimerActive = false;

/* ==================== Changeover_Init / مقداردهی اولیه ==================== */

/**
 * @brief  [EN] Start in BOOT, timers inactive, protect deasserted (safe).
 *         [FA] از حالت BOOT شروع می‌کند.
 */
void func__Changeover_Init(void)
{
    APP_STATE_T__G__State = APP_STATE_BOOT;
    BOOL__G__ChangeoverProtectAsserted = false;
    BOOL__G__ChangeoverBatterySwitchOn = false;
    TICK_T__G__CutStartTick = 0u;
    BOOL__G__CutTimerActive = false;
    TICK_T__G__ReconnectStartTick = 0u;
    BOOL__G__ReconnectTimerActive = false;
    /* [EN] Force safe battery-off at boot: PB5 OFF + PB11 deasserted (path off via PB5).
       BSP already set PB5 High (off) and PB11 High (deasserted), but keep shadow in sync.
       [FA] در BOOT مسیر باتری خاموش: PB5 خاموش + PB11 آزاد (قطع از طریق PB5). */
    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false);
    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
}

/* ==================== Changeover_Evaluate / ارزیابی ==================== */

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__durationTicks;
    bool bool__cutCondition;
    bool bool__reconnectCondition;

    uint32_t__nowTick = osKernelGetTickCount();
    uint32_t__durationTicks = func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS);

    /* [EN] Snapshot check FIRST before any decision: if NULL or invalid,
          preserve state and PB11, reset pending timers, fault does NOT change state,
          and invalid time is not counted.
       [FA] بررسی snapshot قبل از هر تصمیم: اگر نامعتبر، هیچ تصمیمی نگیر و state و PB11 حفظ شود. */
    if (measurement_snapshot_t__snap == NULL)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    if (measurement_snapshot_t__snap->valid == false)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] Snapshot is valid from here - use ONLY allowed fields.
          Now handle fault: fault -> FAULT, no pin change.
       [FA] از اینجا snapshot معتبر است - فقط فیلدهای مجاز؛ اگر fault غیرصفر بود FAULT. */
    if (fault_mask_t__faults != FAULT_NONE)
    {
        APP_STATE_T__G__State = APP_STATE_FAULT;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] Guard: if Rtos tick conversion yields zero ticks (e.g., tick freq 0),
          do not perform immediate cut/reconnect; preserve state and PB11, reset timers.
       [FA] نگهبان: اگر تبدیل میلی‌ثانیه به تیک صفر شد، هیچ قطع/وصلی فوری انجام نشود. */
    if (uint32_t__durationTicks == 0u)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] Evaluate cut conditions (require continuous 3000ms):
          - gated cut: v <21000 AND UI alarm true
          - independent cut: v <20800 independent of UI flag
       [FA] شرایط قطع ارزیابی می‌شوند. */
    bool__cutCondition = false;
    if (measurement_snapshot_t__snap->v_bat24_mv < CHANGEOVER_BAT_CRITICAL_CUT_MV)
    {
        bool__cutCondition = true;
    }
    else if ((measurement_snapshot_t__snap->v_bat24_mv < CHANGEOVER_BAT_LOW_ALARM_CUT_MV) &&
             (BOOL__G__UiBatteryAlarmIssued == true))
    {
        bool__cutCondition = true;
    }
    else
    {
        bool__cutCondition = false;
    }

    if (bool__cutCondition == true)
    {
        if (BOOL__G__CutTimerActive == false)
        {
            BOOL__G__CutTimerActive = true;
            TICK_T__G__CutStartTick = uint32_t__nowTick;
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;

            uint32_t__elapsedTicks = uint32_t__nowTick - TICK_T__G__CutStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                func__Changeover_ApplyBatteryPath(false);
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
            else
            {
                /* [EN] Cut pending but not yet elapsed: keep current state
                      as before cut (BATTERY/INPUT will be set below if not SAFE).
                   [FA] قطع در انتظار: حالت هنوز SAFE نشده. */
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    if (measurement_snapshot_t__snap->input_present == true)
                    {
                        APP_STATE_T__G__State = APP_STATE_INPUT;
                    }
                    else
                    {
                        APP_STATE_T__G__State = APP_STATE_BATTERY;
                    }
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                }
            }
        }
        /* [EN] While cut condition holds, reconnect timer must not run.
           [FA] هنگام شرط قطع، تایمر وصل مجدد نباید فعال باشد. */
        BOOL__G__ReconnectTimerActive = false;

        /* [EN] If cut timer just started (first tick), report current valid state
              (BATTERY/INPUT) until duration completes.
           [FA] در شروع تایمر قطع، حالت معتبر فعلی گزارش شود. */
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            if (APP_STATE_T__G__State == APP_STATE_BOOT)
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            else if (APP_STATE_T__G__State == APP_STATE_SAFE)
            {
                /* [EN] Keep SAFE if already cut.
                   [FA] اگر قبلاً SAFE شده، حفظ شود. */
            }
            else
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            /* [EN] If protect already asserted, stay SAFE (already handled above).
               [FA] اگر قبلاً قطع شده، SAFE بماند. */
            if (BOOL__G__ChangeoverProtectAsserted == true)
            {
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
            else
            {
                /* [EN] While cut is pending, keep battery ON until SAFE is reached.
                   [FA] تا زمان رسیدن به SAFE، مسیر باتری روشن بماند. */
                func__Changeover_ApplyBatteryPath(true);
            }
        }
        else
        {
            func__Changeover_ApplyBatteryPath(false);
        }

        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__CutTimerActive = false;
    }

    /* [EN] Evaluate reconnect: input_present true AND v >=21200 for 3000ms continuous.
       [FA] وصل مجدد فقط وقتی ورودی حاضر و ولتاژ باتری >=21200 به مدت 3 ثانیه. */
    bool__reconnectCondition = false;
    if ((measurement_snapshot_t__snap->input_present == true) &&
        (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV))
    {
        bool__reconnectCondition = true;
    }
    else
    {
        bool__reconnectCondition = false;
    }

    if (bool__reconnectCondition == true)
    {
        if (BOOL__G__ReconnectTimerActive == false)
        {
            BOOL__G__ReconnectTimerActive = true;
            TICK_T__G__ReconnectStartTick = uint32_t__nowTick;
            /* [EN] While waiting for reconnect duration, keep SAFE if already cut,
                  otherwise report INPUT.
               [FA] در انتظار reconnect، اگر قبلاً قطع شده SAFE بماند. */
            if (BOOL__G__ChangeoverProtectAsserted == true)
            {
                APP_STATE_T__G__State = APP_STATE_SAFE;
                func__Changeover_ApplyBatteryPath(false);
            }
            else
            {
                APP_STATE_T__G__State = APP_STATE_INPUT;
                func__Changeover_ApplyBatteryPath(true);
            }
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;

            uint32_t__elapsedTicks = uint32_t__nowTick - TICK_T__G__ReconnectStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                func__Changeover_ApplyBatteryPath(true);
                APP_STATE_T__G__State = APP_STATE_INPUT;
            }
            else
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                    func__Changeover_ApplyBatteryPath(false);
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                    func__Changeover_ApplyBatteryPath(true);
                }
            }
        }
        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__ReconnectTimerActive = false;
    }

    /* [EN] No fault, snapshot valid, no pending cut/reconnect:
          Map state directly from input_present, but preserve SAFE until reconnect.
       [FA] بدون fault و با snapshot معتبر و بدون قطع/وصل در انتظار: از input_present نگاشت شود. */
    if (BOOL__G__ChangeoverProtectAsserted == true)
    {
        APP_STATE_T__G__State = APP_STATE_SAFE;
        func__Changeover_ApplyBatteryPath(false);
    }
    else
    {
        if (measurement_snapshot_t__snap->input_present == true)
        {
            APP_STATE_T__G__State = APP_STATE_INPUT;
        }
        else
        {
            APP_STATE_T__G__State = APP_STATE_BATTERY;
        }
        func__Changeover_ApplyBatteryPath(true);
    }

    return APP_STATE_T__G__State;
}
