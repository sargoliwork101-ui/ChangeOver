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
#include "modules_enable.h"

#if MODULE_FAULT
#include "fault.h"
#endif

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

/* ==================== Battery percent hysteresis state / وضعیت هیسترزیس درصد باتری ==================== */

/**
 * @brief  [EN] Stable battery percent after hysteresis (0..100). Used for BatteryRun blink timing and buzzer bands.
 *         Raw percent is from func__Ui_BatteryVoltageToPercent; stable changes only with 2% hysteresis and special 0/1 handling.
 *         [FA] درصد پایدار باتری پس از هیسترزیس (۰..۱۰۰). برای زمان‌بندی چشمک و بازه‌های بوق BatteryRun استفاده می‌شود.
 */
static uint8_t UINT8_T__G__UiBatteryStablePercent = 0u;

/**
 * @brief  [EN] TRUE after stable percent has been initialized from the first valid raw percent.
 *         [FA] بعد از مقداردهی اولیه درصد پایدار از اولین درصد خام معتبر، TRUE می‌شود.
 */
static bool BOOL__G__UiBatteryStableInitialized = false;

/* ==================== BatteryRun stable percent detailed / درصد پایدار BatteryRun ==================== */

/**
 * @brief  [EN] Alias stable percent for BatteryRun (raw/hysteresis 2% + 0/1).
 *         [FA] درصد پایدار BatteryRun (هیسترزیس 2٪ + ۰/۱).
 */
#define UINT8_T__G__UiBatteryRunStablePercent UINT8_T__G__UiBatteryStablePercent
#define BOOL__G__UiBatteryRunStableInitialized BOOL__G__UiBatteryStableInitialized

/* ==================== Charging stable percent / درصد پایدار شارژ ==================== */

/**
 * @brief  [EN] Stable battery percent for Charging yellow timing (hysteresis 5%).
 *         Raw only updates this when |raw-stable|>=5.
 *         [FA] درصد پایدار برای زمان‌بندی زرد شارژ (هیسترزیس ۵٪).
 */
static uint8_t UINT8_T__G__UiChargingStablePercent = 0u;

/**
 * @brief  [EN] TRUE after Charging stable percent initialized.
 *         [FA] بعد از مقداردهی اولیه درصد پایدار شارژ.
 */
static bool BOOL__G__UiChargingStableInitialized = false;

/* ==================== Charging full hysteresis state / وضعیت هیسترزیس فول شارژ ==================== */

/**
 * @brief  [EN] TRUE while Charging has entered full (InputOk) and stays until raw<95.
 *         [FA] وقتی Charging وارد فول (InputOk) شده تا raw کمتر از ۹۵ شود TRUE می‌ماند.
 */
static bool BOOL__G__UiChargingFullActive = false;

/* ==================== Charging yellow blink state / وضعیت چشمک زرد شارژ ==================== */

/**
 * @brief  [EN] Current yellow LED phase in the non-blocking Charging blink.
 *         [FA] فاز فعلی LED زرد در چشمک غیرمسدودکننده شارژ.
 */
static bool BOOL__G__UiChargingYellowOn = false;

/**
 * @brief  [EN] TRUE after the Charging yellow blink phase has been initialized.
 *         [FA] بعد از مقداردهی فاز چشمک زرد شارژ، TRUE می‌شود.
 */
static bool BOOL__G__UiChargingYellowBlinkInitialized = false;

/**
 * @brief  [EN] RTOS tick at which the current Charging yellow phase started.
 *         [FA] تیک RTOS در زمان شروع فاز فعلی LED زرد شارژ.
 */
static uint32_t TICK_T__G__UiChargingYellowPhaseStartTick = 0u;

/**
 * @brief  [EN] Stored Charging yellow ON duration.
 *         [FA] مدت ذخیره‌شده روشن‌بودن زرد شارژ.
 */
static uint32_t UINT32_T__G__UiChargingYellowOnMs = 0u;

/**
 * @brief  [EN] Stored Charging yellow OFF duration.
 *         [FA] مدت ذخیره‌شده خاموش‌بودن زرد شارژ.
 */
static uint32_t UINT32_T__G__UiChargingYellowOffMs = 0u;

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

/* ==================== Battery stable percent helpers / کمک‌های درصد پایدار باتری ==================== */

/**
 * @brief  [EN] Reset the BatteryRun stable percent hysteresis state. Called on UI init, invalid snapshot, or explicit reset.
 *         [FA] وضعیت هیسترزیس درصد پایدار را بازنشانی می‌کند.
 */
static void func__Ui_ResetBatteryStablePercent(void)
{
    UINT8_T__G__UiBatteryStablePercent = 0u;
    BOOL__G__UiBatteryStableInitialized = false;
}

/**
 * @brief  [EN] Update stable battery percent from raw percent with 2% hysteresis and special 0/1 handling.
 *         General: |raw-stable| <2 → keep stable; >=2 → stable = raw. Special: stable 0 stays 0 until raw>=2 then →1; stable 1: raw==0→0, raw>=3→2, else keep 1.
 *         [FA] درصد پایدار را از درصد خام با هیسترزیس ۲٪ و رفتار خاص ۰/۱ به‌روز می‌کند.
 * @param  uint8_t__rawPercent [EN] Raw percent 0..100 / درصد خام
 * @return uint8_t [EN] Stable percent after hysteresis / درصد پایدار
 */
static uint8_t func__Ui_UpdateBatteryStablePercent(uint8_t uint8_t__rawPercent)
{
    uint8_t uint8_t__stablePercent;
    uint8_t uint8_t__diffPercent;

    if (BOOL__G__UiBatteryStableInitialized == false)
    {
        UINT8_T__G__UiBatteryStablePercent = uint8_t__rawPercent;
        BOOL__G__UiBatteryStableInitialized = true;
        return UINT8_T__G__UiBatteryStablePercent;
    }

    uint8_t__stablePercent = UINT8_T__G__UiBatteryStablePercent;

    if (uint8_t__stablePercent == 0u)
    {
        if (uint8_t__rawPercent >= UI_BATTERY_ZERO_EXIT_THRESHOLD)
        {
            uint8_t__stablePercent = 1u;
        }
        else
        {
            /* [EN] Keep 0 until raw reaches 2.
               [FA] تا raw به ۲ نرسیده روی ۰ بماند. */
        }
    }
    else if (uint8_t__stablePercent == 1u)
    {
        if (uint8_t__rawPercent == 0u)
        {
            uint8_t__stablePercent = 0u;
        }
        else if (uint8_t__rawPercent >= UI_BATTERY_ONE_EXIT_THRESHOLD)
        {
            uint8_t__stablePercent = 2u;
        }
        else
        {
            /* [EN] Keep 1 for raw 1..2.
               [FA] برای raw ۱ تا ۲ روی ۱ بماند. */
        }
    }
    else
    {
        if (uint8_t__rawPercent > uint8_t__stablePercent)
        {
            uint8_t__diffPercent = uint8_t__rawPercent - uint8_t__stablePercent;
        }
        else
        {
            uint8_t__diffPercent = uint8_t__stablePercent - uint8_t__rawPercent;
        }

        if (uint8_t__diffPercent >= UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT)
        {
            uint8_t__stablePercent = uint8_t__rawPercent;
        }
        else
        {
            /* [EN] Jitter <2% keeps BatteryRun stable, e.g., 56↔57, 57↔58 preserves 57.
               [FA] نوسان کمتر از ۲٪، پایدار BatteryRun را نگه می‌دارد. */
        }
    }

    UINT8_T__G__UiBatteryStablePercent = uint8_t__stablePercent;
    return uint8_t__stablePercent;
}

/**
 * @brief  [EN] Reset Charging stable percent (5% hysteresis).
 *         [FA] درصد پایدار شارژ را بازنشانی می‌کند.
 */
static void func__Ui_ResetChargingStablePercent(void)
{
    UINT8_T__G__UiChargingStablePercent = 0u;
    BOOL__G__UiChargingStableInitialized = false;
}

/**
 * @brief  [EN] Update Charging stable percent with 5% hysteresis. |raw-stable| <5 keeps stable, >=5 updates.
 *         [FA] درصد پایدار شارژ را با هیسترزیس ۵٪ به‌روز می‌کند.
 * @param  uint8_t__rawPercent [EN] Raw percent 0..100
 * @return uint8_t stable
 */
static uint8_t func__Ui_UpdateChargingStablePercent(uint8_t uint8_t__rawPercent)
{
    uint8_t uint8_t__stablePercent;
    uint8_t uint8_t__diffPercent;

    if (BOOL__G__UiChargingStableInitialized == false)
    {
        UINT8_T__G__UiChargingStablePercent = uint8_t__rawPercent;
        BOOL__G__UiChargingStableInitialized = true;
        return UINT8_T__G__UiChargingStablePercent;
    }

    uint8_t__stablePercent = UINT8_T__G__UiChargingStablePercent;

    if (uint8_t__rawPercent > uint8_t__stablePercent)
    {
        uint8_t__diffPercent = uint8_t__rawPercent - uint8_t__stablePercent;
    }
    else
    {
        uint8_t__diffPercent = uint8_t__stablePercent - uint8_t__rawPercent;
    }

    if (uint8_t__diffPercent >= UI_CHARGING_PERCENT_HYSTERESIS_PERCENT)
    {
        uint8_t__stablePercent = uint8_t__rawPercent;
    }
    else
    {
        /* [EN] Jitter <5% keeps charging stable, e.g., 57 with 53..61 stays 57.
           [FA] نوسان کمتر از ۵٪ پایدار شارژ را نگه می‌دارد. */
    }

    UINT8_T__G__UiChargingStablePercent = uint8_t__stablePercent;
    return uint8_t__stablePercent;
}

/**
 * @brief  [EN] Reset Charging full (InputOk) hysteresis state.
 *         [FA] وضعیت هیسترزیس فول شارژ را بازنشانی می‌کند.
 */
static void func__Ui_ResetChargingFullHysteresis(void)
{
    BOOL__G__UiChargingFullActive = false;
}

/**
 * @brief  [EN] Update Charging full hysteresis. Enter InputOk only at raw 100, stay until raw<95.
 *         [FA] هیسترزیس ورود/خروج InputOk را به‌روز می‌کند.
 * @param  uint8_t__rawPercent [EN] Raw percent
 * @return bool true = InputOk (full), false = Charging
 */
static bool func__Ui_UpdateChargingFullHysteresis(uint8_t uint8_t__rawPercent)
{
    if (BOOL__G__UiChargingFullActive == true)
    {
        if (uint8_t__rawPercent < UI_CHARGING_FULL_EXIT_PERCENT)
        {
            BOOL__G__UiChargingFullActive = false;
        }
        else
        {
            /* [EN] Keep InputOk until <95.
               [FA] تا کمتر از ۹۵ در InputOk بمان. */
        }
    }
    else
    {
        if (uint8_t__rawPercent >= UI_CHARGING_FULL_ENTER_PERCENT)
        {
            BOOL__G__UiChargingFullActive = true;
        }
        else
        {
            /* [EN] Stay Charging until 100.
               [FA] تا ۱۰۰ در Charging بمان. */
        }
    }

    return BOOL__G__UiChargingFullActive;
}

/* ==================== Charging yellow blink helpers / کمک‌های چشمک زرد شارژ ==================== */

/**
 * @brief  [EN] Reset Charging yellow blink phase.
 *         [FA] فاز چشمک زرد شارژ را بازنشانی می‌کند.
 */
static void func__Ui_ResetChargingYellowBlink(void)
{
    BOOL__G__UiChargingYellowOn = false;
    BOOL__G__UiChargingYellowBlinkInitialized = false;
    TICK_T__G__UiChargingYellowPhaseStartTick = 0u;
    UINT32_T__G__UiChargingYellowOnMs = 0u;
    UINT32_T__G__UiChargingYellowOffMs = 0u;
}

/**
 * @brief  [EN] Update non-blocking Charging yellow blink. Preserves phase on duration change.
 *         [FA] چشمک غیرمسدودکننده زرد شارژ را به‌روز می‌کند و فاز را حفظ می‌کند.
 * @param  uint32_t__yellowOnMs [EN] Yellow ON duration / مدت روشن‌بودن
 * @param  uint32_t__yellowOffMs [EN] Yellow OFF duration / مدت خاموش‌بودن
 */
static void func__Ui_UpdateChargingYellowBlink(uint32_t uint32_t__yellowOnMs, uint32_t uint32_t__yellowOffMs)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__currentPhaseMs;

    ticktype__nowTick = osKernelGetTickCount();

    if (BOOL__G__UiChargingYellowBlinkInitialized == false)
    {
        BOOL__G__UiChargingYellowBlinkInitialized = true;
        BOOL__G__UiChargingYellowOn = true;
        TICK_T__G__UiChargingYellowPhaseStartTick = ticktype__nowTick;
        UINT32_T__G__UiChargingYellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__UiChargingYellowOffMs = uint32_t__yellowOffMs;
    }
    else if ((UINT32_T__G__UiChargingYellowOnMs != uint32_t__yellowOnMs) ||
             (UINT32_T__G__UiChargingYellowOffMs != uint32_t__yellowOffMs))
    {
        /* [EN] Duration changed: keep current phase and start tick, only update stored durations.
             New timing takes effect from next phase boundary.
           [FA] مدت تغییر کرد: فاز فعلی و تیک شروع حفظ شود، فقط مدت ذخیره به‌روز شود. */
        UINT32_T__G__UiChargingYellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__UiChargingYellowOffMs = uint32_t__yellowOffMs;
    }
    else
    {
        uint32_t__currentPhaseMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICK_T__G__UiChargingYellowPhaseStartTick);

        if ((BOOL__G__UiChargingYellowOn == true) &&
            (uint32_t__currentPhaseMs >= uint32_t__yellowOnMs))
        {
            BOOL__G__UiChargingYellowOn = false;
            TICK_T__G__UiChargingYellowPhaseStartTick = ticktype__nowTick;
        }
        else if ((BOOL__G__UiChargingYellowOn == false) &&
                 (uint32_t__currentPhaseMs >= uint32_t__yellowOffMs))
        {
            BOOL__G__UiChargingYellowOn = true;
            TICK_T__G__UiChargingYellowPhaseStartTick = ticktype__nowTick;
        }
        else
        {
            /* [EN] Keep current yellow phase until duration expires.
               [FA] فاز فعلی زرد را تا پایان مدت حفظ کن. */
        }
    }

    func__yellow(BOOL__G__UiChargingYellowOn);
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

    if (BOOL__G__UiBatteryGreenBlinkInitialized == false)
    {
        BOOL__G__UiBatteryGreenBlinkInitialized = true;
        BOOL__G__UiBatteryGreenOn = true;
        TICKTYPE_T__G__UiBatteryGreenPhaseStartTick = ticktype__nowTick;
        UINT32_T__G__UiBatteryGreenOnMs = uint32_t__greenOnMs;
        UINT32_T__G__UiBatteryGreenOffMs = uint32_t__greenOffMs;
    }
    else if ((UINT32_T__G__UiBatteryGreenOnMs != uint32_t__greenOnMs) ||
             (UINT32_T__G__UiBatteryGreenOffMs != uint32_t__greenOffMs))
    {
        /* [EN] Duration changed due to stable percent jitter: keep current phase and start tick, only update stored durations.
             New timing applies from next phase boundary, so a 10ms jitter does not restart the green ON.
           [FA] مدت به خاطر نوسان درصد پایدار تغییر کرد: فاز و تیک شروع حفظ شود، فقط مدت ذخیره به‌روز شود. */
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
    func__Ui_ResetChargingYellowBlink();

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

/* ==================== Scenario BatLost Tick / تیک سناریوی قطع باتری ==================== */

/**
 * @brief  [EN] Battery-lost announcement: red fast blink (50% of a 1 s period,
 *         distinctly unlike the slow overvoltage pulse), green steady since
 *         the input is present in both detection cases, and the periodic
 *         three-short-beeps-plus-pause buzzer pattern. The pattern phase uses
 *         the absolute kernel tick so no state needs remembering here; the
 *         fault bit itself is owned (set and cleared) only by the Fault
 *         module. Must be called every Ui pass while active so the buzzer
 *         pattern advances.
 *         [FA] اعلان قطع باتری: قرمز چشمک‌تند ۵۰٪ در دوره یک‌ثانیه، سبز
 *         ثابت (ورودی حاضر است) و الگوی سه بیپ کوتاه + مکث. فاز از تیک مطلق
 *         گرفته می‌شود تا وضعیتی لازم نباشد؛ خود پرچم فقط در Fault مدیریت
 *         می‌شود. تا وقتی فعال است هر پاس صدا زده شود.
 */
void func__Ui_ScenarioBatLost_Tick(void)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__phaseMs;
    bool     bool__redOn;

    /* [EN] Foreign blink/beep states belong to other scenarios; stop them so
       nothing from BatteryRun/Charging bleeds into this pattern.
       [FA] وضعیت‌های چشمک/بوق سناریوهای دیگر ریست شود تا اثری از آن‌ها در
       این الگو نیفتد. */
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();

    uint32_t__nowTick   = osKernelGetTickCount();
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick);
    uint32_t__phaseMs   = uint32_t__elapsedMs % UI_BAT_LOST_LED_PERIOD_MS;

    /* [EN] 0..50% of the period = ON, 50..100% = OFF -> 500/500 ms fast blink.
       [FA] نیمه اول دوره روشن، نیمه دوم خاموش ⇒ چشمک تند ۵۰۰/۵۰۰. */
    bool__redOn = (uint32_t__phaseMs <
                   ((UI_BAT_LOST_LED_PERIOD_MS * UI_BAT_LOST_LED_DUTY_PERCENT) /
                    UI_PERCENT_SCALE));

    func__green(true);
    func__yellow(false);
    func__red(bool__redOn);

    (void)func__Ui_Buzzer_Tick(
        UI_BAT_LOST_BEEP_PERIOD_MS,
        (uint8_t)UI_BAT_LOST_BEEP_DUTY_PERCENT,
        (uint8_t)UI_BAT_LOST_BEEP_COUNT,
        UI_BAT_LOST_BEEP_GAP_MS);
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
    func__Ui_ResetChargingYellowBlink();

    func__green(true);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);

    /* [EN] InputOk is steady green, no blink — return immediately. The UI task's 10ms loop provides the poll period,
         so a 500ms blocking delay inside the scenario is not needed and would slow UI reaction.
       [FA] InputOk سبز ثابت است؛ بدون چشمک و بدون تاخیر مسدودکننده. حلقه ۱۰ms تسک، دوره polling را می‌دهد. */
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
    uint8_t uint8_t__rawPercent;
    uint8_t uint8_t__stablePercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__yellowOnMs;
    uint32_t uint32_t__yellowOffMs;

    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);

    /* [EN] Charging yellow timing uses hysteresis 5%: stable only moves when |raw-stable|>=5. v_bat is PA3 / v_bat24_mv.
       [FA] زمان‌بندی زرد شارژ با هیسترزیس ۵٪: پایدار فقط وقتی اختلاف حداقل ۵ باشد به‌روز می‌شود. */
    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);
    uint8_t__stablePercent = func__Ui_UpdateChargingStablePercent(uint8_t__rawPercent);

    func__green(true);
    func__red(false);

    if (uint8_t__stablePercent >= UI_PERCENT_FULL)
    {
        func__Ui_ResetChargingYellowBlink();
        func__yellow(false);
        return;
    }

    if (uint8_t__stablePercent == 0u)
    {
        func__Ui_ResetChargingYellowBlink();
        func__yellow(true);
        return;
    }

    /* [EN] Non-linear formula with chargingStablePercent: remaining drives yellow ON time.
       [FA] فرمول غیرخطی با درصد پایدار شارژ: مانده تا فول، زمان روشن‌بودن زرد را می‌دهد. */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__stablePercent;
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

    func__Ui_UpdateChargingYellowBlink(uint32_t__yellowOnMs, uint32_t__yellowOffMs);
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
    uint8_t uint8_t__rawPercent;
    uint8_t uint8_t__stablePercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__greenOffMs;
    uint32_t uint32_t__greenOnMs;
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__criticalElapsedMs;

    func__Ui_ResetChargingYellowBlink();

    /* [EN] Step 1: raw percent from PA3 / ADC1_IN3 / v_bat24_mv.
       [FA] گام ۱: درصد خام از PA3. */
    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    /* [EN] Step 2: stable percent with 2% hysteresis and special 0/1 handling.
       This hysteresis is only for BatteryRun display/timing, not for input/overvoltage/Low Alarm.
       [FA] گام ۲: درصد پایدار با هیسترزیس ۲٪ و رفتار خاص ۰/۱ (فقط BatteryRun). */
    uint8_t__stablePercent = func__Ui_UpdateBatteryStablePercent(uint8_t__rawPercent);

    /* [EN] Critical 0% uses stable percent: one 10s beep, then LEDs off until battery exits critical area.
       Noise 0<->1 does not restart the beep because stable 0 stays 0 until raw>=2.
       [FA] ۰٪ بحرانی با درصد پایدار: یک بوق ۱۰ ثانیه‌ای فقط یک بار، سپس LED خاموش تا خروج از ناحیه بحرانی. */
    if (uint8_t__stablePercent < UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT)
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

    /* [EN] Non-linear: green blink OFF = remaining * period/100, with min. Input is stablePercent.
       [FA] فرمول غیرخطی سبز چشمک: خاموشی برابر مانده درصد پایدار ضربدر دوره است. */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__stablePercent;
    uint32_t__periodPerPercent = APP_CONFIG.ui_blink_period_ms / UI_PERCENT_SCALE;
    uint32_t__greenOffMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__greenOffMs < APP_CONFIG.ui_green_min_off_ms)
    {
        uint32_t__greenOffMs = APP_CONFIG.ui_green_min_off_ms;
    }

    uint32_t__greenOnMs = APP_CONFIG.ui_blink_period_ms - uint32_t__greenOffMs;

    if (uint8_t__stablePercent >= UI_BATTERY_RUN_BEEP_START_PERCENT)
    {
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    else if (uint8_t__stablePercent >= UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT)
    {
        (void)func__Ui_Buzzer_Tick(
            UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS,
            (uint8_t)UI_BATTERY_RUN_BEEP_STANDARD_DUTY_PERCENT,
            (uint8_t)UI_BATTERY_RUN_BEEP_STANDARD_COUNT,
            UI_BATTERY_RUN_BEEP_GAP_MS);
    }
    else if (uint8_t__stablePercent >= UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT)
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
    uint8_t uint8_t__rawPercent;
    bool bool__snapshotValid;
    bool bool__isFull;

    if (measurement_snapshot_t__snap == NULL)
    {
        func__all_off();
        BOOL__G__UiBatteryAlarmIssued = false;
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        TICKTYPE_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetBatteryCriticalBeep();
        func__Ui_ResetBatteryRunGreenBlink();
        func__Ui_ResetChargingYellowBlink();
        func__Ui_ResetBatteryStablePercent();
        func__Ui_ResetChargingStablePercent();
        func__Ui_ResetChargingFullHysteresis();
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
        func__Ui_ResetChargingYellowBlink();
        /* [EN] BatteryRun and Charging stable percents keep previous values on invalid but phases reset; full hysteresis keeps state to avoid flicker on noisy valid->invalid.
           Battery voltage source is PA3 / v_bat24_mv only; do not use v_in here.
           [FA] درصدهای پایدار مقدار قبلی را نگه می‌دارند اما فازها ریست می‌شوند. */
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

#if MODULE_FAULT
    /* [EN] Battery-lost, priority 2 (overvoltage first, this second, normal
       scenarios after). The Fault module latches and clears the bit; while it
       is set we a) show this scenario and b) return, so the BatteryRun
       critical beep (input-absent world) can never overlap with this pattern
       (input-present world). When the battery is back and the settle time
       passed, the bit clears and the previous scenario resumes by itself.
       [FA] قطع باتری با اولویت دوم (بعد از اضافه‌ولتاژ، قبل از سناریوهای
       نرمال). فقط تا وقتی پرچم متمرکز قفل است نشان می‌دهیم و return می‌کنیم
       تا هرگز با بوق بحرانی دشارژ قاطی نشود؛ با پاک‌شدن پرچم، سناریوی قبلی
       خودبه‌خود برمی‌گردد. */
    if ((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE)
    {
        func__Ui_ScenarioBatLost_Tick();
        return;
    }
#endif

    uint32_t__batteryClampedMv = uint32_t__batteryVoltageMv;
    if (uint32_t__batteryClampedMv > APP_CONFIG.ui_bat_v_max_mv)
    {
        uint32_t__batteryClampedMv = APP_CONFIG.ui_bat_v_max_mv;
    }

    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryClampedMv);
    bool__isFull = func__Ui_UpdateChargingFullHysteresis(uint8_t__rawPercent);

    if (BOOL__G__UiInputPresent == true)
    {
        if (bool__isFull == true)
        {
            /* [EN] Full hysteresis: entered at 100, stays InputOk until <95.
               [FA] هیسترزیس فول: ورود در ۱۰۰، ماندن تا کمتر از ۹۵. */
            func__Ui_ScenarioInputOk();
        }
        else
        {
            func__Ui_ScenarioCharging_Tick(uint32_t__batteryClampedMv);
        }
    }
    else
    {
        /* [EN] Input disconnected → BatteryRun uses 2% hysteresis + 0/1.
           [FA] ورودی قطع → BatteryRun با هیسترزیس ۲٪ + ۰/۱. */
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
    func__Ui_ResetChargingYellowBlink();
    func__Ui_ResetBatteryStablePercent();
    func__Ui_ResetChargingStablePercent();
    func__Ui_ResetChargingFullHysteresis();
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
    func__Ui_ResetChargingYellowBlink();
    func__Ui_ResetBatteryStablePercent();
    func__Ui_ResetChargingStablePercent();
    func__Ui_ResetChargingFullHysteresis();

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
