/**
 * @file    ui_led.c
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in ui_led.h.
 *          CMSIS-RTOS2 simple readable, non-linear formulas, markers above each function and variable in h and c.
 *          [FA] سناریوهای LED ماژول UI - ثابت‌های LED در هدر خودش، هر تابع و متغیر با جدا کننده و کامنت.
 *
 * @note    [EN] ui_led.h provides defaults; runtime-tunable values are read from const APP_CONFIG. Naming __ after type, func__ prefix.
 *          CMSIS-RTOS2: osDelay allowed, HAL_Delay forbidden. Formulas non-linear broken into steps.
 *          [FA] ui_led.h پیش‌فرض‌ها را می‌دهد؛ مقدارهای قابل تنظیم زمان اجرا از APP_CONFIG ثابت خوانده می‌شوند. نام‌گذاری با __، پیشوند func__، فرمول غیرخطی.
 */

#include "ui_led.h"
#include "ui_buzzer.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include "rtos_time.h"

#include <stdbool.h>

/* ==================== UI Global Battery Alarm Flag / فلگ سراسری آلارم باتری UI ==================== */

/**
 * @brief  [EN] Global flag owned by the UI: true while a valid low-battery alarm is active.
 *         It is continuous (level), not a pulse. Set in Init to false, cleared on invalid
 *         snapshot, set when v_bat24_mv < 21000 and cleared when >=21200.
 *         [FA] فلگ سراسری در مالکیت UI: هنگام آلارم معتبر باتری کم true است.
 */
volatile bool BOOL__G__UiBatteryAlarmIssued = false;

/* ==================== Battery Voltage To Percent / تبدیل ولتاژ باتری به درصد ==================== */

/**
 * @brief  [EN] Battery voltage to percent 0..100. Non-linear formula broken into 4 steps: range, offset, scaled, percent.
 *         [FA] ولتاژ باتری به درصد - فرمول غیرخطی ۴ گام: بازه، فاصله، مقیاس، درصد.
 * @param  uint32_t__batteryMv [EN] Battery voltage in mV, 0..40000mV, 21000=0% 28000=100% / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__voltageRangeMv;
    uint32_t uint32_t__voltageOffsetMv;
    uint32_t uint32_t__scaledOffset;
    uint8_t uint8_t__batteryPercent;

    if (uint32_t__batteryMv <= APP_CONFIG.ui_bat_v_min_mv)
    {
        return 0u;
    }

    if (uint32_t__batteryMv >= APP_CONFIG.ui_bat_v_max_mv)
    {
        return UI_PERCENT_FULL;
    }

    /* [EN] Step 1: range = Vmax - Vmin
       [FA] گام ۱: بازه ولتاژ */
    uint32_t__voltageRangeMv = APP_CONFIG.ui_bat_v_max_mv - APP_CONFIG.ui_bat_v_min_mv;

    /* [EN] Step 2: offset = Vbat - Vmin
       [FA] گام ۲: فاصله از کف */
    uint32_t__voltageOffsetMv = uint32_t__batteryMv - APP_CONFIG.ui_bat_v_min_mv;

    if (uint32_t__voltageRangeMv == 0u)
    {
        return 0u;
    }

    /* [EN] Step 3: scaled = offset * 100
       [FA] گام ۳: مقیاس به درصد */
    uint32_t__scaledOffset = uint32_t__voltageOffsetMv * UI_PERCENT_SCALE;

    /* [EN] Step 4: percent = scaled / range
       [FA] گام ۴: تقسیم برای درصد */
    uint8_t__batteryPercent = (uint8_t)(uint32_t__scaledOffset / uint32_t__voltageRangeMv);

    if (uint8_t__batteryPercent > UI_PERCENT_FULL)
    {
        uint8_t__batteryPercent = UI_PERCENT_FULL;
    }

    return uint8_t__batteryPercent;
}

/* ==================== Green LED / LED سبز ==================== */

/**
 * @brief  [EN] Drive green LED on/off. Low-level wrapper around BSP GPIO.
 *         [FA] ال‌ای‌دی سبز را روشن/خاموش می‌کند - سطح پایین.
 * @param  bool__greenOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__green(bool bool__greenOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_GREEN, bool__greenOn);
}

/* ==================== Red LED / LED قرمز ==================== */

/**
 * @brief  [EN] Drive red LED on/off. Low-level.
 *         [FA] ال‌ای‌دی قرمز را روشن/خاموش می‌کند.
 * @param  bool__redOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__red(bool bool__redOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_RED, bool__redOn);
}

/* ==================== Yellow LED / LED زرد ==================== */

/**
 * @brief  [EN] Drive yellow LED on/off. Low-level.
 *         [FA] ال‌ای‌دی زرد را روشن/خاموش می‌کند.
 * @param  bool__yellowOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__yellow(bool bool__yellowOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_YELLOW, bool__yellowOn);
}

/* ==================== All Off Safe / خاموشی امن همه خروجی‌ها ==================== */

/**
 * @brief  [EN] Drive all LEDs off and request the buzzer service to enter its safe-off state.
 *         [FA] همه ال‌ای‌دی‌ها را خاموش می‌کند و سرویس بوق را به حالت خاموش امن می‌برد.
 */
static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
}

/* ==================== Input connection state / وضعیت اتصال ورودی ==================== */

/**
 * @brief  [EN] Stateful input-connected result used by the 20V/21V hysteresis.
 *         It starts disconnected as the safe default.
 *         [FA] نتیجه دارای وضعیت تشخیص اتصال ورودی با هیسترزیس ۲۰/۲۱ ولت.
 *         مقدار اولیه برای حالت امن، قطع است.
 */
static bool BOOL__G__UiInputPresent = false;

/* ==================== Input overvoltage state / وضعیت اضافه‌ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Stateful input overvoltage error flag.
 *         The flag remains active between 27V and 28V after an overvoltage event.
 *         [FA] پرچم دارای وضعیت خطای اضافه‌ولتاژ ورودی.
 *         بعد از رخداد خطا، بین ۲۷ و ۲۸ ولت فعال باقی می‌ماند.
 */
static bool BOOL__G__UiInputOverVoltage = false;

/**
 * @brief  [EN] CMSIS-RTOS2 tick at which the current input overvoltage display started.
 *         [FA] تیک RTOS در زمان شروع نمایش خطای اضافه‌ولتاژ ورودی.
 */
static uint32_t TICKTYPE_T__G__UiInputOverVoltageStartTick = 0;

/* ==================== BatteryRun critical beep state / وضعیت بوق بحرانی BatteryRun ==================== */

/**
 * @brief  [EN] TRUE while the one-time critical BatteryRun beep is active.
 *         [FA] هنگام فعال‌بودن بوق بحرانی تک‌باره BatteryRun مقدار TRUE دارد.
 */
static bool BOOL__G__UiBatteryCriticalBeepActive = false;

/**
 * @brief  [EN] TRUE after the one-time critical BatteryRun beep has completed.
 *         [FA] بعد از پایان بوق بحرانی تک‌باره BatteryRun مقدار TRUE دارد.
 */
static bool BOOL__G__UiBatteryCriticalBeepCompleted = false;

/**
 * @brief  [EN] RTOS tick at which the critical BatteryRun beep started.
 *         [FA] تیک RTOS در زمان شروع بوق بحرانی BatteryRun.
 */
static uint32_t TICKTYPE_T__G__UiBatteryCriticalBeepStartTick = 0;

/* ==================== BatteryRun green blink state / وضعیت چشمک سبز BatteryRun ==================== */

/**
 * @brief  [EN] Current green LED phase in the non-blocking BatteryRun blink.
 *         [FA] فاز فعلی LED سبز در چشمک غیرمسدودکننده BatteryRun.
 */
static bool BOOL__G__UiBatteryGreenOn = false;

/**
 * @brief  [EN] TRUE after the BatteryRun green blink phase has been initialized.
 *         [FA] بعد از مقداردهی فاز چشمک سبز BatteryRun مقدار TRUE دارد.
 */
static bool BOOL__G__UiBatteryGreenBlinkInitialized = false;

/**
 * @brief  [EN] RTOS tick at which the current BatteryRun green phase started.
 *         [FA] تیک RTOS در زمان شروع فاز فعلی LED سبز BatteryRun.
 */
static uint32_t TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = 0;

/**
 * @brief  [EN] Stored BatteryRun green ON duration used to restart phase timing when percentage changes.
 *         [FA] مدت ذخیره‌شده روشن‌بودن سبز BatteryRun برای شروع مجدد فاز هنگام تغییر درصد.
 */
static uint32_t UINT32_T__G__UiBatteryGreenOnMs = 0u;

/**
 * @brief  [EN] Stored BatteryRun green OFF duration used to restart phase timing when percentage changes.
 *         [FA] مدت ذخیره‌شده خاموش‌بودن سبز BatteryRun برای شروع مجدد فاز هنگام تغییر درصد.
 */
static uint32_t UINT32_T__G__UiBatteryGreenOffMs = 0u;

/* ==================== BatteryRun critical beep reset / بازنشانی بوق بحرانی BatteryRun ==================== */

/**
 * @brief  [EN] Reset the one-time critical BatteryRun beep state.
 *         [FA] وضعیت بوق بحرانی تک‌باره BatteryRun را بازنشانی می‌کند.
 */
static void func__Ui_ResetBatteryCriticalBeep(void)
{
    BOOL__G__UiBatteryCriticalBeepActive = false;
    BOOL__G__UiBatteryCriticalBeepCompleted = false;
    TICKTYPE_T__G__UiBatteryCriticalBeepStartTick = 0;
}

/* ==================== BatteryRun green blink reset / بازنشانی چشمک سبز BatteryRun ==================== */

/**
 * @brief  [EN] Reset non-blocking BatteryRun green blink timing.
 *         [FA] زمان‌بندی چشمک غیرمسدودکننده سبز BatteryRun را بازنشانی می‌کند.
 */
static void func__Ui_ResetBatteryRunGreenBlink(void)
{
    BOOL__G__UiBatteryGreenOn = false;
    BOOL__G__UiBatteryGreenBlinkInitialized = false;
    TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = 0;
    UINT32_T__G__UiBatteryGreenOnMs = 0u;
    UINT32_T__G__UiBatteryGreenOffMs = 0u;
}

/* ==================== BatteryRun green blink update / به‌روزرسانی چشمک سبز BatteryRun ==================== */

/**
 * @brief  [EN] Update the non-blocking BatteryRun green blink and service its phase timing.
 *         [FA] چشمک غیرمسدودکننده سبز BatteryRun و زمان‌بندی فاز آن را به‌روز می‌کند.
 * @param  uint32_t__greenOnMs [EN] Green ON duration / مدت روشن‌بودن سبز
 * @param  uint32_t__greenOffMs [EN] Green OFF duration / مدت خاموش‌بودن سبز
 */
static void func__Ui_UpdateBatteryRunGreenBlink(uint32_t uint32_t__greenOnMs, uint32_t uint32_t__greenOffMs)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__currentPhaseMs;

    ticktype__nowTick = osKernelGetTickCount();

    if ((BOOL__G__UiBatteryGreenBlinkInitialized == false) ||
        (UINT32_T__G__UiBatteryGreenOnMs != uint32_t__greenOnMs) ||
        (UINT32_T__G__UiBatteryGreenOffMs != uint32_t__greenOffMs))
    {
        BOOL__G__UiBatteryGreenBlinkInitialized = true;
        BOOL__G__UiBatteryGreenOn = true;
        TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        UINT32_T__G__UiBatteryGreenOnMs = uint32_t__greenOnMs;
        UINT32_T__G__UiBatteryGreenOffMs = uint32_t__greenOffMs;
    }
    else
    {
        uint32_t__currentPhaseMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__UiBatteryGreenPhaseStartTick);

        if ((BOOL__G__UiBatteryGreenOn == true) &&
            (uint32_t__currentPhaseMs >= uint32_t__greenOnMs))
        {
            BOOL__G__UiBatteryGreenOn = false;
            TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        }
        else if ((BOOL__G__UiBatteryGreenOn == false) &&
                 (uint32_t__currentPhaseMs >= uint32_t__greenOffMs))
        {
            BOOL__G__UiBatteryGreenOn = true;
            TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        }
        else
        {
            /* [EN] Keep the current LED phase until its configured duration expires.
               [FA] فاز فعلی LED را تا پایان مدت تنظیم‌شده حفظ کن. */
        }
    }

    func__green(BOOL__G__UiBatteryGreenOn);
}

/* ==================== Input state update / به‌روزرسانی وضعیت ورودی ==================== */

/**
 * @brief  [EN] Update input presence and input overvoltage state with hysteresis.
 *         Connected: input >= 21V. Disconnected: input <= 20V.
 *         Overvoltage enters above 28V and clears at or below 27V.
 *         [FA] وضعیت اتصال و خطای اضافه‌ولتاژ ورودی را با هیسترزیس به‌روز می‌کند.
 *         وصل: ورودی حداقل ۲۱ ولت. قطع: ورودی حداکثر ۲۰ ولت.
 *         خطا: بالاتر از ۲۸ ولت فعال و در ۲۷ ولت یا پایین‌تر پاک می‌شود.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage in mV / ولتاژ ورودی بر حسب میلی‌ولت
 */
static void func__Ui_UpdateInputState(uint32_t uint32_t__inputVoltageMv)
{
    if (BOOL__G__UiInputOverVoltage == false)
    {
        if (uint32_t__inputVoltageMv > UI_INPUT_OVERVOLTAGE_THRESHOLD_MV)
        {
            BOOL__G__UiInputOverVoltage = true;
            TICKTYPE_T__G__UiInputOverVoltageStartTick = osKernelGetTickCount();
        }
    }
    else if (uint32_t__inputVoltageMv <= UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV)
    {
        BOOL__G__UiInputOverVoltage = false;
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    else
    {
        /* [EN] Keep the overvoltage error in the 27V..28V hysteresis band.
           [FA] خطای اضافه‌ولتاژ را در بازه هیسترزیس ۲۷ تا ۲۸ ولت حفظ کن. */
    }

    if (BOOL__G__UiInputPresent == false)
    {
        if (uint32_t__inputVoltageMv >= UI_INPUT_CONNECTED_THRESHOLD_MV)
        {
            BOOL__G__UiInputPresent = true;
        }
    }
    else if (uint32_t__inputVoltageMv <= UI_INPUT_DISCONNECTED_THRESHOLD_MV)
    {
        BOOL__G__UiInputPresent = false;
    }
    else
    {
        /* [EN] Keep the previous connected state in the 20V..21V band.
           [FA] وضعیت قبلی اتصال را در بازه ۲۰ تا ۲۱ ولت حفظ کن. */
    }
}

/* ==================== Scenario Input Overvoltage / سناریوی اضافه‌ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Display input overvoltage: green steady, yellow off, red at 50% duty,
 *         and one 1-second buzzer pulse every 10 seconds. This tick is non-blocking.
 *         [FA] نمایش اضافه‌ولتاژ ورودی: سبز ثابت، زرد خاموش، قرمز با دیوتی ۵۰ درصد،
 *         و یک بوق یک‌ثانیه‌ای هر ۱۰ ثانیه. این تیک غیرمسدودکننده است.
 */
static void func__Ui_ScenarioInputOverVoltage_Tick(void)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__phaseMs;
    uint32_t uint32_t__redOnMs;
    uint64_t uint64_t__redDutyProduct;
    bool bool__redOn;

    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();

    ticktype__nowTick = osKernelGetTickCount();
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__UiInputOverVoltageStartTick);
    uint32_t__phaseMs = uint32_t__elapsedMs % UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS;

    uint64_t__redDutyProduct = (uint64_t)UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS * UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT;
    uint32_t__redOnMs = (uint32_t)(uint64_t__redDutyProduct / UI_PERCENT_SCALE);
    bool__redOn = (uint32_t__phaseMs < uint32_t__redOnMs);

    func__green(true);
    func__yellow(false);
    func__red(bool__redOn);

    (void)func__Ui_Buzzer_Tick(
        UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS,
        (uint8_t)UI_INPUT_OVERVOLTAGE_BEEP_DUTY_PERCENT,
        (uint8_t)UI_INPUT_OVERVOLTAGE_BEEP_COUNT,
        UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS);
}

/* ==================== Scenario InputOk / سناریوی ورودی عادی ==================== */

/**
 * @brief  [EN] InputOk scenario: green steady, red/yellow off, and buzzer off.
 *         [FA] سناریو ورودی وصل: سبز ثابت، قرمز و زرد خاموش و بوق خاموش.
 */
void func__Ui_ScenarioInputOk(void)
{
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();

    func__green(true);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);

    /* [EN] RTOS delay in task, not HAL_Delay - other tasks still run, MCU not locked, simple & readable
       [FA] تاخیر CMSIS-RTOS2 در تسک - میکرو قفل نمی‌شود، ساده و خوانا */
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_input_ok_poll_ms);
}

/* ==================== Scenario Charging Tick / تیک سناریوی شارژ ==================== */

/**
 * @brief  [EN] Charging scenario tick: green steady, yellow shows remaining to full non-linear.
 *         Formula: remainingPercent = 100-pct, periodPerPercent = period/100, yellowOnMs = remaining*periodPer, yellowOffMs = period-yellowOn.
 *         [FA] سناریو شارژ: سبز ثابت، زرد مانده تا فول غیرخطی.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__batteryPercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__yellowOnMs;
    uint32_t uint32_t__yellowOffMs;

    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    if (uint8_t__batteryPercent >= UI_PERCENT_FULL)
    {
        func__yellow(false);
        func__green(true);
        func__red(false);
        func__Rtos_DelayMilliseconds(APP_CONFIG.ui_charging_blink_period_ms);
        return;
    }

    if (uint8_t__batteryPercent == 0u)
    {
        func__yellow(true);
        func__green(true);
        func__red(false);
        func__Rtos_DelayMilliseconds(APP_CONFIG.ui_charging_blink_period_ms);
        return;
    }

    /* [EN] Non-linear formula: break into steps for readability
       [FA] فرمول غیرخطی: گام به گام برای خوانایی */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__batteryPercent;
    uint32_t__periodPerPercent = APP_CONFIG.ui_charging_blink_period_ms / UI_PERCENT_SCALE;
    uint32_t__yellowOnMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__yellowOnMs < APP_CONFIG.ui_charging_yellow_min_off_ms)
    {
        uint32_t__yellowOnMs = APP_CONFIG.ui_charging_yellow_min_off_ms;
    }
    if (uint32_t__yellowOnMs > APP_CONFIG.ui_charging_blink_period_ms)
    {
        uint32_t__yellowOnMs = APP_CONFIG.ui_charging_blink_period_ms;
    }

    uint32_t__yellowOffMs = APP_CONFIG.ui_charging_blink_period_ms - uint32_t__yellowOnMs;

    func__green(true);
    func__red(false);

    func__yellow(true);
    func__Rtos_DelayMilliseconds(uint32_t__yellowOnMs);
    func__yellow(false);
    func__Rtos_DelayMilliseconds(uint32_t__yellowOffMs);
}

/* ==================== Scenario BatteryRun Tick / تیک سناریوی دشارژ ==================== */

/**
 * @brief  [EN] BatteryRun scenario: green blink follows the linear 21V..28V battery percentage;
 *         buzzer warnings use the four requested percentage bands.
 *         Below 1%, all LEDs turn off and one ten-second buzzer is latched until the battery recovers.
 *         [FA] سناریو دشارژ: سبز بر اساس درصد خطی ۲۱ تا ۲۸ ولت چشمک می‌زند؛
 *         بوق بر اساس چهار بازه درصدی درخواستی اجرا می‌شود.
 *         زیر ۱٪ همه LEDها خاموش و یک بوق ده‌ثانیه‌ای تا برگشت باتری فقط یک‌بار اجرا می‌شود.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__batteryPercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__greenOffMs;
    uint32_t uint32_t__greenOnMs;
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__criticalElapsedMs;

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    if (uint8_t__batteryPercent < UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT)
    {
        func__Ui_ResetBatteryRunGreenBlink();
        func__green(false);
        func__red(false);
        func__yellow(false);

        if (BOOL__G__UiBatteryCriticalBeepCompleted == true)
        {
            (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
            return;
        }

        if (BOOL__G__UiBatteryCriticalBeepActive == false)
        {
            BOOL__G__UiBatteryCriticalBeepActive = true;
            TICKTYPE_T__G__UiBatteryCriticalBeepStartTick = osKernelGetTickCount();
        }

        ticktype__nowTick = osKernelGetTickCount();
        uint32_t__criticalElapsedMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__UiBatteryCriticalBeepStartTick);

        if (uint32_t__criticalElapsedMs >= UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS)
        {
            (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
            BOOL__G__UiBatteryCriticalBeepActive = false;
            BOOL__G__UiBatteryCriticalBeepCompleted = true;
            return;
        }

        (void)func__Ui_Buzzer_Tick(
            UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS,
            (uint8_t)UI_BATTERY_RUN_BEEP_CRITICAL_DUTY_PERCENT,
            (uint8_t)UI_BATTERY_RUN_BEEP_CRITICAL_COUNT,
            UI_BATTERY_RUN_BEEP_GAP_MS);
        return;
    }

    func__Ui_ResetBatteryCriticalBeep();

    /* [EN] Non-linear: green blink OFF = remaining * period/100, with min.
       [FA] فرمول غیرخطی سبز چشمک: خاموشی برابر مانده درصد ضربدر دوره است. */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__batteryPercent;
    uint32_t__periodPerPercent = APP_CONFIG.ui_blink_period_ms / UI_PERCENT_SCALE;
    uint32_t__greenOffMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__greenOffMs < APP_CONFIG.ui_green_min_off_ms)
    {
        uint32_t__greenOffMs = APP_CONFIG.ui_green_min_off_ms;
    }

    uint32_t__greenOnMs = APP_CONFIG.ui_blink_period_ms - uint32_t__greenOffMs;

    if (uint8_t__batteryPercent >= UI_BATTERY_RUN_BEEP_START_PERCENT)
    {
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    else if (uint8_t__batteryPercent >= UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT)
    {
        (void)func__Ui_Buzzer_Tick(
            UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS,
            (uint8_t)UI_BATTERY_RUN_BEEP_STANDARD_DUTY_PERCENT,
            (uint8_t)UI_BATTERY_RUN_BEEP_STANDARD_COUNT,
            UI_BATTERY_RUN_BEEP_GAP_MS);
    }
    else if (uint8_t__batteryPercent >= UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT)
    {
        (void)func__Ui_Buzzer_Tick(
            UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS,
            (uint8_t)UI_BATTERY_RUN_BEEP_DOUBLE_DUTY_PERCENT,
            (uint8_t)UI_BATTERY_RUN_BEEP_DOUBLE_COUNT,
            UI_BATTERY_RUN_BEEP_GAP_MS);
    }
    else
    {
        (void)func__Ui_Buzzer_Tick(
            UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS,
            (uint8_t)UI_BATTERY_RUN_BEEP_TRIPLE_DUTY_PERCENT,
            (uint8_t)UI_BATTERY_RUN_BEEP_TRIPLE_COUNT,
            UI_BATTERY_RUN_BEEP_GAP_MS);
    }

    func__red(false);
    func__yellow(false);
    func__Ui_UpdateBatteryRunGreenBlink(uint32_t__greenOnMs, uint32_t__greenOffMs);
}

/* ==================== Ui Tick / تیک اصلی UI ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario from the real Measurement snapshot.
 *         The snapshot is obtained via func__Measurement_GetSnapshot(); valid is checked
 *         before any decision. If invalid, UI enters safe-off, alarm flag is cleared and
 *         no stale/manual values are used. Battery production source is snapshot.v_bat24_mv only.
 *         [FA] تیک اصلی UI - تصمیم سناریو را از snapshot واقعی Measurement می‌گیرد.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL / اشاره‌گر snapshot
 */
void func__Ui_Tick(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__batteryVoltageMv;
    uint32_t uint32_t__batteryClampedMv;
    uint8_t uint8_t__batteryPercent;
    bool bool__snapshotValid;

    if (measurement_snapshot_t__snap == NULL)
    {
        func__all_off();
        BOOL__G__UiBatteryAlarmIssued = false;
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetBatteryCriticalBeep();
        func__Ui_ResetBatteryRunGreenBlink();
        return;
    }

    bool__snapshotValid = measurement_snapshot_t__snap->valid;

    if (bool__snapshotValid == false)
    {
        func__all_off();
        BOOL__G__UiBatteryAlarmIssued = false;
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetBatteryCriticalBeep();
        func__Ui_ResetBatteryRunGreenBlink();
        return;
    }

    uint32_t__inputVoltageMv = measurement_snapshot_t__snap->v_in_mv;
    uint32_t__batteryVoltageMv = measurement_snapshot_t__snap->v_bat24_mv;

    /* [EN] Update global low-battery alarm flag continuously with hysteresis.
       Threshold <21000 sets true, >=21200 clears false, otherwise hold.
       [FA] فلگ سراسری آلارم باتری کم را به‌صورت پیوسته با هیسترزیس به‌روز کن. */
    if (uint32_t__batteryVoltageMv < UI_LOW_BATTERY_ALARM_THRESHOLD_MV)
    {
        BOOL__G__UiBatteryAlarmIssued = true;
    }
    else if (uint32_t__batteryVoltageMv >= UI_LOW_BATTERY_ALARM_CLEAR_MV)
    {
        BOOL__G__UiBatteryAlarmIssued = false;
    }
    else
    {
        /* [EN] Keep previous flag in hysteresis band 21000..21200.
           [FA] فلگ قبلی را در بازه هیسترزیس حفظ کن. */
    }

    func__Ui_UpdateInputState(uint32_t__inputVoltageMv);

    if (BOOL__G__UiInputOverVoltage == true)
    {
        func__Ui_ScenarioInputOverVoltage_Tick();
        return;
    }

    uint32_t__batteryClampedMv = uint32_t__batteryVoltageMv;
    if (uint32_t__batteryClampedMv > APP_CONFIG.ui_bat_v_max_mv)
    {
        uint32_t__batteryClampedMv = APP_CONFIG.ui_bat_v_max_mv;
    }

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryClampedMv);

    if (BOOL__G__UiInputPresent == true)
    {
        if (uint8_t__batteryPercent < UI_PERCENT_FULL)
        {
            func__Ui_ScenarioCharging_Tick(uint32_t__batteryClampedMv);
        }
        else
        {
            func__Ui_ScenarioInputOk();
        }
    }
    else
    {
        func__Ui_ScenarioBatteryRun_Tick(uint32_t__batteryClampedMv);
    }
}

/* ==================== Ui Init / مقداردهی اولیه UI ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void)
{
    func__all_off();
    BOOL__G__UiBatteryAlarmIssued = false;
    BOOL__G__UiInputPresent = false;
    BOOL__G__UiInputOverVoltage = false;
    TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
}

/* ==================== Board Test Start / شروع تست برد ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous 150ms-style buzzer check.
 *         The buzzer uses the new periodic API with a safe period and then is explicitly turned off.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز و بوق کوتاه قبلی.
 *         بوق با API دوره‌ای جدید و دوره امن اجرا و سپس صریحاً خاموش می‌شود.
 */
void func__Ui_BoardTest_Start(void)
{
    uint32_t uint32_t__beepPeriodMs;
    uint32_t uint32_t__beepDutyPercent;
    uint64_t uint64_t__beepDutyProduct;
    int32_t int32_t__buzzerResult;

    func__all_off();
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();

    func__red(true);
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_selftest_led_ms);
    func__red(false);

    func__yellow(true);
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_selftest_led_ms);
    func__yellow(false);

    func__green(true);
    func__Rtos_DelayMilliseconds(APP_CONFIG.ui_selftest_led_ms);
    func__green(false);

    if (APP_CONFIG.ui_boot_beep_ms == 0u)
    {
        return;
    }

    uint32_t__beepPeriodMs = APP_CONFIG.ui_boot_beep_ms;
    if (uint32_t__beepPeriodMs < UI_BUZZER_MIN_PERIOD_MS)
    {
        uint32_t__beepPeriodMs = UI_BUZZER_MIN_PERIOD_MS;
    }

    uint32_t__beepDutyPercent = UI_BUZZER_DUTY_MAX_PERCENT;
    if (APP_CONFIG.ui_boot_beep_ms < uint32_t__beepPeriodMs)
    {
        uint64_t__beepDutyProduct = (uint64_t)APP_CONFIG.ui_boot_beep_ms * UI_BUZZER_PERCENT_SCALE;
        uint32_t__beepDutyPercent = (uint32_t)(uint64_t__beepDutyProduct / uint32_t__beepPeriodMs);
        if ((uint64_t__beepDutyProduct % uint32_t__beepPeriodMs) != 0u)
        {
            uint32_t__beepDutyPercent++;
        }
        if (uint32_t__beepDutyPercent == 0u)
        {
            uint32_t__beepDutyPercent = 1u;
        }
    }

    int32_t__buzzerResult = func__Ui_Buzzer_Tick(
        uint32_t__beepPeriodMs,
        (uint8_t)uint32_t__beepDutyPercent,
        1u,
        0u);

    if (int32_t__buzzerResult != UI_BUZZER_INVALID_RESULT)
    {
        func__Rtos_DelayMilliseconds(APP_CONFIG.ui_boot_beep_ms);
    }

    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
}
