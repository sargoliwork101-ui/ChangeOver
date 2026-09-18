/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine - timed cut/reconnect with 3000ms filtering.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - قطع/وصل با فیلتر ۳ ثانیه.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, BOOL__G__UiBatteryAlarmIssued.
 *              No board_pins.h, no HAL, no physical pin names, no float/queue/task.
 *              Time conversion ONLY via rtos_time.h (MillisecondsToTicks); tick=1ms assumption is forbidden.
 *              It owns two logical power-path outputs: BSP_GPIO_PROTECT_BATTERY
 *              (battery cut) and BSP_GPIO_BATTERY_SWITCH (MCU PSU battery feed).
 *          [FA] فقط از داده‌های مجاز بالا استفاده می‌کند و فقط دو خروجی منطقی
 *              مسیر توان را می‌راند: BSP_GPIO_PROTECT_BATTERY (قطع باتری) و
 *              BSP_GPIO_BATTERY_SWITCH (تغذیه PSU MCU از باتری).
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
 * @brief  [EN] Logical PB5 battery power switch state: true = MCU control PSU
 *              connected to the battery so the MCU stays alive without input.
 *         [FA] وضعیت منطقی کلید توان باتری PB5: true یعنی PSU کنترل MCU به باتری
 *              وصل است تا MCU بدون ورودی زنده بماند.
 */
static bool BOOL__G__BatterySwitchOn = false;

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

/* ==================== Changeover Apply Battery Switch ==================== */

/**
 * @brief  [EN] Own the MCU battery power switch (logical BSP_GPIO_BATTERY_SWITCH)
 *              from the evaluated system state: ON while the load and the MCU
 *              run from the battery (BATTERY) or while a cut battery is being
 *              monitored for reconnect (SAFE), OFF while the input supplies the
 *              system (INPUT) so the battery cannot feed the input net, and
 *              held unchanged in BOOT/IDLE/FAULT where no valid decision exists.
 *              Only writes the pin when the requested level differs.
 *         [FA] مالک کلید توان باتری MCU (منطقی BSP_GPIO_BATTERY_SWITCH) بر اساس
 *              حالت ارزیابی‌شده سیستم: روشن وقتی بار و MCU از باتری کار می‌کنند
 *              (BATTERY) یا وقتی باتری قطع‌شده برای وصل مجدد پایش می‌شود (SAFE)،
 *              خاموش وقتی ورودی سیستم را تغذیه می‌کند (INPUT) تا باتری نتواند
 *              به نت ورودی تزریق کند، و بدون تغییر در BOOT/IDLE/FAULT که تصمیم
 *              معتبری وجود ندارد. فقط وقتی سطح درخواستی متفاوت است پایه را
 *              می‌نویسد.
 * @param  app_state_t__nextState [EN] State evaluated in this cycle /
 *                                    حالت ارزیابی‌شده در این سیکل
 */
static void func__Changeover_ApplyBatterySwitch(app_state_t app_state_t__nextState)
{
    bool bool__switchOn;

    if (app_state_t__nextState == APP_STATE_BATTERY)
    {
        bool__switchOn = true;
    }
    else if (app_state_t__nextState == APP_STATE_SAFE)
    {
        /* [EN] Keep the MCU alive after a cut so a returning input can be
           detected and the battery reconnected; without it the protect state
           would be released by the unpowered MCU pins.
           [FA] MCU پس از قطع زنده می‌ماند تا ورودی برگشتی تشخیص داده شود و
           باتری وصل مجدد شود؛ بدون آن پایه‌های MCU بی‌تغذیه وضعیت حفاظت را
           رها می‌کردند. */
        bool__switchOn = true;
    }
    else if (app_state_t__nextState == APP_STATE_INPUT)
    {
        bool__switchOn = false;
    }
    else
    {
        /* [EN] BOOT/IDLE/FAULT: no valid power-path decision, hold the pin.
           [FA] BOOT/IDLE/FAULT: تصمیم معتبر مسیر توان نیست، پایه حفظ می‌شود. */
        return;
    }

    if (bool__switchOn != BOOL__G__BatterySwitchOn)
    {
        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, bool__switchOn);
        BOOL__G__BatterySwitchOn = bool__switchOn;
    }
}

/* ==================== Changeover_Init / مقداردهی اولیه ==================== */

/**
 * @brief  [EN] Start in BOOT with timers inactive and mirror the BSP safe
 *              state: the board port drives PROTECT_BATTERY to its safe level
 *              (battery cut) before any task runs, so the software flag must
 *              start asserted too. Keeping flag and pin in agreement prevents
 *              a later reconnect step from skipping the deassert write and
 *              leaving the battery path cut forever. The battery power switch
 *              starts off, exactly as the BSP safe state leaves it.
 *         [FA] از حالت BOOT با تایمرهای غیرفعال شروع می‌کند و وضعیت امن BSP را
 *              آینه می‌کند: پورت برد پیش از اجرای هر تسک PROTECT_BATTERY را روی
 *              سطح امن (قطع باتری) می‌گذارد، پس پرچم نرم‌افزار هم باید از ابتدا
 *              فعال باشد. هم‌بودن پرچم و پایه جلوگیری می‌کند که یک وصل مجدد
 *              نوشتنِ غیرفعال‌سازی را جا بیندازد و مسیر باتری برای همیشه قطع
 *              بماند. کلید توان باتری هم مانند وضعیت امن BSP خاموش شروع می‌کند.
 */
void func__Changeover_Init(void)
{
    APP_STATE_T__G__State = APP_STATE_BOOT;
    BOOL__G__ChangeoverProtectAsserted = true;
    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
    BOOL__G__BatterySwitchOn = false;
    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false);
    TICK_T__G__CutStartTick = 0u;
    BOOL__G__CutTimerActive = false;
    TICK_T__G__ReconnectStartTick = 0u;
    BOOL__G__ReconnectTimerActive = false;
}

/* ==================== Changeover_Evaluate / ارزیابی ==================== */

/**
 * @brief  [EN] Internal core: evaluate the next system state from snapshot and
 *              faults and drive the protect output. The public wrapper adds the
 *              battery power switch policy on top of the returned state.
 *         [FA] هستهٔ داخلی: حالت بعدی سیستم را از نمونه و خطا حساب می‌کند و
 *              خروجی حفاظت را می‌راند. پوشانندهٔ عمومی سیاست کلید توان باتری
 *              را روی حالت برگشتی اضافه می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
static app_state_t func__Changeover_EvaluateCore(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults)
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
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
                    BOOL__G__ChangeoverProtectAsserted = true;
                }
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
            }
            else
            {
                APP_STATE_T__G__State = APP_STATE_INPUT;
            }
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;

            uint32_t__elapsedTicks = uint32_t__nowTick - TICK_T__G__ReconnectStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
                    BOOL__G__ChangeoverProtectAsserted = false;
                }
                APP_STATE_T__G__State = APP_STATE_INPUT;
            }
            else
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
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

    return APP_STATE_T__G__State;
}

/* ==================== Changeover_Evaluate / ارزیابی ==================== */

/**
 * @brief  [EN] Evaluate the next system state from snapshot and faults, drive
 *              the protect output inside the core, then apply the battery power
 *              switch policy for the returned state: ON in BATTERY and SAFE so
 *              the MCU stays alive from the battery, OFF in INPUT so the battery
 *              cannot feed the input net, held in BOOT/IDLE/FAULT.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا ارزیابی می‌کند، خروجی حفاظت در
 *              هستهٔ داخلی رانده می‌شود و سپس سیاست کلید توان باتری برای حالت
 *              برگشتی اعمال می‌شود: روشن در BATTERY و SAFE تا MCU از باتری زنده
 *              بماند، خاموش در INPUT تا باتری به نت ورودی تزریق نکند، و بدون
 *              تغییر در BOOT/IDLE/FAULT.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults)
{
    app_state_t app_state_t__nextState;

    app_state_t__nextState =
        func__Changeover_EvaluateCore(measurement_snapshot_t__snap, fault_mask_t__faults);

    func__Changeover_ApplyBatterySwitch(app_state_t__nextState);

    return app_state_t__nextState;
}
