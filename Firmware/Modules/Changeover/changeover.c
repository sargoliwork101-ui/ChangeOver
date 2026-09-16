/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine with three-second battery protection.
 *          [FA] ماشین حالت مسیر ورودی و باتری با حفاظت سه‌ثانیه‌ای باتری.
 */

#include "changeover.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include "rtos_time.h"

/* ==================== Changeover state / حالت Changeover ==================== */
static app_state_t APP_STATE_T__G__State = APP_STATE_BOOT;
static bool BOOL__G__ChangeoverBatteryProtectionAsserted = false;

/* ==================== Persistence timers / تایمرهای پایدار ماندن شرط ==================== */
static bool BOOL__G__ChangeoverUiCutTimerActive = false;
static bool BOOL__G__ChangeoverHardCutTimerActive = false;
static bool BOOL__G__ChangeoverReconnectTimerActive = false;
static uint32_t TICKTYPE_T__G__ChangeoverUiCutStartTick = 0u;
static uint32_t TICKTYPE_T__G__ChangeoverHardCutStartTick = 0u;
static uint32_t TICKTYPE_T__G__ChangeoverReconnectStartTick = 0u;

/* ==================== Reset timers / بازنشانی تایمرها ==================== */
/**
 * @brief  [EN] Clear all pending persistence timers without changing state or pin.
 *         [FA] همهٔ تایمرهای در انتظار را بدون تغییر state یا پایه پاک می‌کند.
 */
static void func__Changeover_ResetTimers(void)
{
    BOOL__G__ChangeoverUiCutTimerActive = false;
    BOOL__G__ChangeoverHardCutTimerActive = false;
    BOOL__G__ChangeoverReconnectTimerActive = false;
    TICKTYPE_T__G__ChangeoverUiCutStartTick = 0u;
    TICKTYPE_T__G__ChangeoverHardCutStartTick = 0u;
    TICKTYPE_T__G__ChangeoverReconnectStartTick = 0u;
}

/* ==================== Timed condition / شرط زمان‌دار ==================== */
/**
 * @brief  [EN] Return true only after a condition stays true for the requested ticks.
 *         [FA] فقط پس از پایدار ماندن شرط به‌اندازهٔ تیک درخواستی true برمی‌گرداند.
 * @param  bool__condition [EN] Current condition / شرط فعلی
 * @param  bool__timerActive [EN] Timer active flag / فلگ فعال بودن تایمر
 * @param  ticktype__startTick [EN] Timer start tick / تیک شروع تایمر
 * @param  ticktype__nowTick [EN] Current tick / تیک فعلی
 * @param  uint32_t__requiredTicks [EN] Required persistence ticks / تیک لازم
 * @return bool [EN] Condition persisted / پایدار ماندن شرط
 */
static bool func__Changeover_TimedCondition(
    bool bool__condition,
    bool *bool__timerActive,
    uint32_t *ticktype__startTick,
    uint32_t ticktype__nowTick,
    uint32_t uint32_t__requiredTicks)
{
    if (bool__condition == false)
    {
        *bool__timerActive = false;
        *ticktype__startTick = 0u;
        return false;
    }

    if (*bool__timerActive == false)
    {
        *bool__timerActive = true;
        *ticktype__startTick = ticktype__nowTick;
        return (uint32_t__requiredTicks == 0u);
    }

    return ((ticktype__nowTick - *ticktype__startTick) >= uint32_t__requiredTicks);
}

/* ==================== Changeover Init / مقداردهی اولیه Changeover ==================== */
/**
 * @brief  [EN] Start in BOOT and keep the BSP safe startup pin unchanged.
 *         [FA] از BOOT شروع می‌کند و پایهٔ امن راه‌اندازی BSP را تغییر نمی‌دهد.
 */
void func__Changeover_Init(void)
{
    APP_STATE_T__G__State = APP_STATE_BOOT;
    BOOL__G__ChangeoverBatteryProtectionAsserted = false;
    func__Changeover_ResetTimers();
}

/* ==================== Changeover Evaluate / ارزیابی Changeover ==================== */
/**
 * @brief  [EN] Evaluate one valid snapshot and update the logical battery path.
 *         [FA] یک snapshot معتبر را ارزیابی و مسیر منطقی باتری را به‌روز می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Measurement snapshot / snapshot اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Current fault bits / بیت‌های خطای فعلی
 * @param  bool__uiBatteryAlarmIssued [EN] Valid UI low-battery alarm level / سطح آلارم باتری کم UI
 * @return app_state_t [EN] Current state / حالت فعلی
 */
app_state_t func__Changeover_Evaluate(
    const measurement_snapshot_t *measurement_snapshot_t__snap,
    fault_mask_t fault_mask_t__faults,
    bool bool__uiBatteryAlarmIssued)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__transitionTicks;
    bool bool__uiCutPersisted;
    bool bool__hardCutPersisted;
    bool bool__reconnectPersisted;

    /* [EN] Invalid data is not a decision. Pending time is discarded so
       invalid time cannot satisfy a continuous protection condition.
       [FA] دادهٔ نامعتبر تصمیم نیست. زمان در انتظار پاک می‌شود تا زمان
       نامعتبر شرط حفاظت پیوسته را تکمیل نکند. */
    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false))
    {
        func__Changeover_ResetTimers();
        return APP_STATE_T__G__State;
    }

    /* [EN] Fault state has priority after snapshot validity and never drives
       the protection output. [FA] پس از اعتبار snapshot، حالت خطا اولویت
       دارد و هرگز خروجی حفاظت را تحریک نمی‌کند. */
    if (fault_mask_t__faults != FAULT_NONE)
    {
        func__Changeover_ResetTimers();
        APP_STATE_T__G__State = APP_STATE_FAULT;
        return APP_STATE_T__G__State;
    }

    uint32_t__transitionTicks =
        func__Rtos_MillisecondsToTicks(CHANGEOVER_TRANSITION_PERSISTENCE_MS);
    if (uint32_t__transitionTicks == 0u)
    {
        func__Changeover_ResetTimers();
        return APP_STATE_T__G__State;
    }

    ticktype__nowTick = osKernelGetTickCount();

    if (BOOL__G__ChangeoverBatteryProtectionAsserted == true)
    {
        /* [EN] Reconnect is permitted only with both valid input presence and
           the 21.2V battery threshold for the full persistence interval.
           [FA] وصل مجدد فقط با حضور ورودی معتبر و آستانهٔ ۲۱.۲ ولت باتری
           برای کل بازهٔ پایداری مجاز است. */
        bool__reconnectPersisted = func__Changeover_TimedCondition(
            (measurement_snapshot_t__snap->input_present == true) &&
                (measurement_snapshot_t__snap->v_bat24_mv >=
                 CHANGEOVER_BATTERY_RECONNECT_THRESHOLD_MV),
            &BOOL__G__ChangeoverReconnectTimerActive,
            &TICKTYPE_T__G__ChangeoverReconnectStartTick,
            ticktype__nowTick,
            uint32_t__transitionTicks);

        BOOL__G__ChangeoverUiCutTimerActive = false;
        BOOL__G__ChangeoverHardCutTimerActive = false;
        TICKTYPE_T__G__ChangeoverUiCutStartTick = 0u;
        TICKTYPE_T__G__ChangeoverHardCutStartTick = 0u;

        if (bool__reconnectPersisted == true)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
            BOOL__G__ChangeoverBatteryProtectionAsserted = false;
            func__Changeover_ResetTimers();
            APP_STATE_T__G__State = APP_STATE_INPUT;
        }
        else
        {
            APP_STATE_T__G__State = APP_STATE_SAFE;
        }

        return APP_STATE_T__G__State;
    }

    bool__uiCutPersisted = func__Changeover_TimedCondition(
        (measurement_snapshot_t__snap->v_bat24_mv <
         CHANGEOVER_BATTERY_CUT_WITH_UI_THRESHOLD_MV) &&
            (bool__uiBatteryAlarmIssued == true),
        &BOOL__G__ChangeoverUiCutTimerActive,
        &TICKTYPE_T__G__ChangeoverUiCutStartTick,
        ticktype__nowTick,
        uint32_t__transitionTicks);

    bool__hardCutPersisted = func__Changeover_TimedCondition(
        (measurement_snapshot_t__snap->v_bat24_mv <
         CHANGEOVER_BATTERY_CUT_HARD_THRESHOLD_MV),
        &BOOL__G__ChangeoverHardCutTimerActive,
        &TICKTYPE_T__G__ChangeoverHardCutStartTick,
        ticktype__nowTick,
        uint32_t__transitionTicks);

    BOOL__G__ChangeoverReconnectTimerActive = false;
    TICKTYPE_T__G__ChangeoverReconnectStartTick = 0u;

    if ((bool__uiCutPersisted == true) || (bool__hardCutPersisted == true))
    {
        func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
        BOOL__G__ChangeoverBatteryProtectionAsserted = true;
        func__Changeover_ResetTimers();
        APP_STATE_T__G__State = APP_STATE_SAFE;
    }
    else if (measurement_snapshot_t__snap->input_present == true)
    {
        APP_STATE_T__G__State = APP_STATE_INPUT;
    }
    else
    {
        /* [EN] Input absent with a valid battery is a normal Battery state.
           [FA] نبود ورودی با باتری معتبر یک حالت عادی Battery است. */
        APP_STATE_T__G__State = APP_STATE_BATTERY;
    }

    return APP_STATE_T__G__State;
}
