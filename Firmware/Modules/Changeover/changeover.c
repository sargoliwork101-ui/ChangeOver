/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine - timed cut/reconnect with 3000ms filtering.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - قطع/وصل با فیلتر ۳ ثانیه.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, BOOL__G__UiBatteryAlarmIssued.
 *              No board_pins.h, no HAL, no PB5/PB7, no float/queue/task.
 *              Time conversion ONLY via rtos_time.h; tick=1ms assumption is forbidden.
 *              PB11 (BSP_GPIO_PROTECT_BATTERY) is the ONLY logical pin used.
 *          [FA] فقط از داده‌های مجاز بالا استفاده می‌کند و فقط PB11 منطقی را می‌زند.
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
    TICK_T__G__CutStartTick = 0u;
    BOOL__G__CutTimerActive = false;
    TICK_T__G__ReconnectStartTick = 0u;
    BOOL__G__ReconnectTimerActive = false;
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
    uint32_t uint32_t__elapsedMs;
    bool bool__snapshotValid;
    bool bool__cutCondition;
    bool bool__reconnectCondition;

    uint32_t__nowTick = osKernelGetTickCount();

    /* [EN] Fault has highest priority: go to FAULT, no pin change, timers reset.
       [FA] خطا بالاترین اولویت: به FAULT برو، پایه تغییر نکند. */
    if (fault_mask_t__faults != FAULT_NONE)
    {
        APP_STATE_T__G__State = APP_STATE_FAULT;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] Snapshot invalid: no decision, preserve state and PB11, timers reset so invalid time not counted.
       [FA] snapshot نامعتبر: هیچ تصمیمی نگیر، state و PB11 حفظ شود، زمان نامعتبر محاسبه نشود. */
    if (measurement_snapshot_t__snap == NULL)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    bool__snapshotValid = measurement_snapshot_t__snap->valid;
    if (bool__snapshotValid == false)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] Snapshot is valid from here - use ONLY allowed fields.
       [FA] از اینجا snapshot معتبر است - فقط فیلدهای مجاز استفاده می‌شوند. */

    /* [EN] Evaluate cut conditions:
          - gated cut: v <21000 AND UI alarm true for 3000ms
          - independent cut: v <20800 for 3000ms (no UI flag needed)
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
            uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick - TICK_T__G__CutStartTick);
            if (uint32_t__elapsedMs >= CHANGEOVER_DURATION_MS)
            {
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
                    BOOL__G__ChangeoverProtectAsserted = true;
                }
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
        }
        /* [EN] While cut condition holds, reconnect timer must not run.
           [FA] هنگام شرط قطع، تایمر وصل مجدد نباید فعال باشد. */
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__CutTimerActive = false;
    }

    /* [EN] Evaluate reconnect: input_present true AND v >=21200 for 3000ms.
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
        }
        else
        {
            uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick - TICK_T__G__ReconnectStartTick);
            if (uint32_t__elapsedMs >= CHANGEOVER_DURATION_MS)
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
                    BOOL__G__ChangeoverProtectAsserted = false;
                }
                APP_STATE_T__G__State = APP_STATE_IDLE;
            }
        }
    }
    else
    {
        BOOL__G__ReconnectTimerActive = false;
    }

    /* [EN] No fault, snapshot valid, no cut/reconnect triggered: keep current state
          or default to IDLE if still in BOOT. This preserves BOOT->IDLE transition.
       [FA] بدون خطا و با snapshot معتبر و بدون تریگر قطع/وصل: حالت حفظ شود. */
    if (APP_STATE_T__G__State == APP_STATE_BOOT)
    {
        APP_STATE_T__G__State = APP_STATE_IDLE;
    }

    return APP_STATE_T__G__State;
}
