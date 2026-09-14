/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios with tunable parameters on top, separate buzzer function, voltage-based.
 *          [FA] سناریوهای LED/بازر با پارامترهای قابل تنظیم در بالا، تابع جدا بازر، مبتنی بر ولتاژ.
 *
 * @note    [EN] Teaching:
 *          - All tunable numbers are at the top of this file (and also in app_config.c). Change them here to tune behavior without touching logic.
 *          - Battery 0% is NOT 0V, it is UI_BAT_V_MIN_MV (21V). 100% is UI_BAT_V_MAX_MV (28V). This maps voltage to percent.
 *          - Input present if V_in >= UI_INPUT_THRESHOLD_MV (20V).
 *          - Variable naming: first type prefix, then name. Global type UPPERCASE (e.g. U32_G_...), local lowercase (e.g. u32_...).
 *          - Buzzer is separate: Ui_BuzzerBeep() can be called from any scenario.
 *          [FA] آموزش:
 *          - همه اعداد قابل تنظیم بالای همین فایل هستند (و همچنین در app_config.c). برای تغییر رفتار همین‌جا را عوض کن.
 *          - باتری صفر درصد صفر ولت نیست، UI_BAT_V_MIN_MV یعنی ۲۱ ولت است. فول UI_BAT_V_MAX_MV یعنی ۲۸ ولت.
 *          - ورودی وصل اگر V_in >= UI_INPUT_THRESHOLD_MV (۲۰ ولت).
 *          - نام متغیر: اول تایپ بعد نام. اگر گلوبال تایپ با حروف بزرگ (U32_G_...)، اگر داخلی کوچک (u32_...).
 *          - بازر جدا: Ui_BuzzerBeep را در هر سناریو می‌توان صدا زد.
 */

/* ==================== Includes ==================== */
#include "ui.h"
#include "ui_config.h"  /* [EN] Single source for all thresholds/timings / تنها منبع آستانه‌ها و تایم‌ها */
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

/* ==================== Low-level LED/Buzzer helpers ==================== */

/**
 * @brief  [EN] Green LED PB10, active-high via Q6.
 *         [FA] LED سبز PB10، active-high از طریق Q6.
 * @param  b_on [EN] true=on / روشن
 */
static void green(bool b_on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, b_on);
}

/**
 * @brief  [EN] Red LED PB0, reserved for critical later.
 *         [FA] LED قرمز PB0، برای سناریوی بحرانی بعدی رزرو.
 * @param  b_on [EN] true=on
 */
static void red(bool b_on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, b_on);
}

/**
 * @brief  [EN] Yellow LED PB1, active-high via Q5. Used in Charging scenario.
 *         [FA] LED زرد PB1، active-high از طریق Q5. در سناریوی شارژ استفاده می‌شود.
 * @param  b_on [EN] true=on
 */
static void yellow(bool b_on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, b_on);
}

/**
 * @brief  [EN] Buzzer PA4, active-high via Q7. Low-level.
 *         [FA] بازر PA4، active-high از طریق Q7. سطح پایین.
 * @param  b_on [EN] true=sound
 */
static void buzzer(bool b_on)
{
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, b_on);
}

/**
 * @brief  [EN] All UI outputs off.
 *         [FA] همه خروجی‌های UI خاموش.
 */
static void all_off(void)
{
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

/* ==================== Global (file-scope) beep tracking ==================== */
/* [EN] File-scope globals use UPPERCASE type prefix per user naming rule.
   [FA] متغیرهای گلوبال سطح فایل با تایپ حروف بزرگ طبق قانون نام‌گذاری کاربر. */

static uint32_t U32_G_BeepCycleCounter = 0u;  /* [EN] Counts BatteryRun cycles for beep interval / شمارنده سیکل برای بوق */

/* ==================== Public API ==================== */

/**
 * @brief  [EN] Safe state before scheduler: all off.
 *         [FA] حالت امن قبل از زمان‌بند: همه خاموش.
 */
void Ui_Init(void)
{
    all_off();
}

/**
 * @brief  [EN] One-shot R/Y/G then beep. Times from top defines (also in APP_CONFIG).
 *         [FA] تست یک‌باره قرمز/زرد/سبز و بوق. زمان از بالای همین فایل.
 */
void Ui_BoardTest(void)
{
    all_off();

    red(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    red(false);

    yellow(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    yellow(false);

    green(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    green(false);

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(UI_BOOT_BEEP_MS));
    buzzer(false);
}

/**
 * @brief  [EN] Separate buzzer function. Can be called from any scenario.
 *         [FA] تابع جدا بازر. از هر سناریو می‌توان صدا زد.
 * @param  u32_durationMs [EN] Beep length ms / طول بوق
 */
void Ui_BuzzerBeep(uint32_t u32_durationMs)
{
    uint32_t u32_dur = u32_durationMs;

    /* [EN] Clamp zero to base to avoid silent call / صفر را به پایه محدود کن */
    if (u32_dur == 0u)
    {
        u32_dur = UI_BEEP_BASE_MS;
    }

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(u32_dur));
    buzzer(false);
}

/**
 * @brief  [EN] Convert battery voltage mV to percent 0..100 using Vmin=21V (0%) and Vmax=28V (100%).
 *         [FA] تبدیل ولتاژ باتری به درصد ۰..۱۰۰ با Vmin=۲۱V صفر و Vmax=۲۸V فول.
 * @param  u32_batteryMv [EN] Battery voltage mV / ولتاژ باتری میلی‌ولت
 * @return uint8_t percent 0..100
 */
uint8_t Ui_BatteryVoltageToPercent(uint32_t u32_batteryMv)
{
    uint32_t u32_v = u32_batteryMv;
    uint32_t u32_range;
    uint32_t u32_offset;
    uint8_t u8_percent;

    /* [EN] Below min => 0% / زیر حداقل صفر درصد */
    if (u32_v <= UI_BAT_V_MIN_MV)
    {
        return 0u;
    }

    /* [EN] Above max => 100% / بالای حداکثر فول */
    if (u32_v >= UI_BAT_V_MAX_MV)
    {
        return UI_PERCENT_FULL;
    }

    u32_range = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV; /* e.g. 7000mV */
    u32_offset = u32_v - UI_BAT_V_MIN_MV;

    /* [EN] Avoid division by zero, though range is constant 7000 / جلوگیری از تقسیم بر صفر */
    if (u32_range == 0u)
    {
        return 0u;
    }

    u8_percent = (uint8_t)((u32_offset * UI_PERCENT_SCALE) / u32_range);

    if (u8_percent > UI_PERCENT_FULL)
    {
        u8_percent = UI_PERCENT_FULL;
    }

    return u8_percent;
}

/**
 * @brief  [EN] Scenario Input Normal — steady green, others off. One cycle.
 *         [FA] سناریوی ورودی عادی — سبز ثابت، بقیه خاموش. یک سیکل.
 */
void Ui_ScenarioInputOk(void)
{
    /* [EN] Parameters on top: UI_INPUT_OK_POLL_MS / پارامتر بالا */
    green(true);
    red(false);
    yellow(false);
    buzzer(false);

    vTaskDelay(pdMS_TO_TICKS(UI_INPUT_OK_POLL_MS));
}

/**
 * @brief  [EN] Scenario Battery Run — green blink where on-time = battery percent.
 *         Yellow OFF per new requirement. Smart beep handled here using separate buzzer function.
 *         Beep logic: if pct<50, beep every pct seconds (40%->40s). If pct<20, beep duration x2.
 *         [FA] سناریوی دشارژ باتری — چشمک سبز با روشن‌بودن برابر درصد. زرد خاموش.
 *         بوق هوشمند: اگر درصد<۵۰ هر درصد ثانیه یک بوق (۴۰٪ هر ۴۰ ثانیه). اگر <۲۰٪ طول بوق ۲ برابر.
 * @param  u32_batteryMv [EN] Battery voltage mV (21V=0%,28V=100%) / ولتاژ باتری
 */
void Ui_ScenarioBatteryRun(uint32_t u32_batteryMv)
{
    uint8_t u8_batteryPercent;
    uint32_t u32_offMs;
    uint32_t u32_onMs;
    uint32_t u32_beepDurationMs;
    uint32_t u32_beepIntervalCycles;
    bool b_shouldBeep = false;

    /* [EN] Convert voltage to percent using tunable Vmin/Vmax on top / تبدیل ولتاژ به درصد */
    u8_batteryPercent = Ui_BatteryVoltageToPercent(u32_batteryMv);

    /* [EN] Green blink: on = pct * period/100, off = rest, min off 10ms for full / چشمک سبز */
    u32_offMs = (uint32_t)(UI_PERCENT_FULL - u8_batteryPercent) *
                (UI_BLINK_PERIOD_MS / UI_PERCENT_SCALE);

    if (u32_offMs < UI_GREEN_MIN_OFF_MS)
    {
        u32_offMs = UI_GREEN_MIN_OFF_MS; /* [EN] Full battery still blinks briefly / فول هم لحظه‌ای خاموش */
    }

    u32_onMs = UI_BLINK_PERIOD_MS - u32_offMs;

    /* [EN] Yellow OFF in this stage per requirement / زرد خاموش در این مرحله */
    red(false);
    yellow(false);

    green(true);
    vTaskDelay(pdMS_TO_TICKS(u32_onMs));
    green(false);
    vTaskDelay(pdMS_TO_TICKS(u32_offMs));

    /* ==================== Smart Beep Logic (separate buzzer function) ==================== */
    /* [EN] Tunable on top: UI_BEEP_START_PCT=50, UI_BEEP_DOUBLE_THRESH_PCT=20, UI_BEEP_BASE_MS=250
       [FA] پارامترهای بوق بالا قابل تغییر هستند */

    if (u8_batteryPercent < UI_BEEP_START_PCT)
    {
        /* [EN] Interval = percent seconds. 40% => 40 cycles (each cycle ~1s) / فاصله = درصد ثانیه */
        u32_beepIntervalCycles = (uint32_t)u8_batteryPercent;

        /* [EN] Avoid 0 interval when 0% / جلوگیری از صفر */
        if (u32_beepIntervalCycles == 0u)
        {
            u32_beepIntervalCycles = 1u;
        }

        if (U32_G_BeepCycleCounter >= u32_beepIntervalCycles)
        {
            b_shouldBeep = true;
            U32_G_BeepCycleCounter = 0u;
        }
        else
        {
            U32_G_BeepCycleCounter++;
        }

        if (b_shouldBeep == true)
        {
            u32_beepDurationMs = UI_BEEP_BASE_MS;

            if (u8_batteryPercent < UI_BEEP_DOUBLE_THRESH_PCT)
            {
                u32_beepDurationMs *= 2u; /* [EN] Double duration under 20% / زیر ۲۰٪ طول ۲ برابر */
            }

            /* [EN] Call separate buzzer function / صدا زدن تابع جدا بازر */
            Ui_BuzzerBeep(u32_beepDurationMs);
        }
    }
    else
    {
        /* [EN] Above 50% no beep, reset counter / بالای ۵۰٪ بوق نداریم */
        U32_G_BeepCycleCounter = 0u;
    }
}

/**
 * @brief  [EN] Scenario Charging — yellow indicates remaining to full.
 *         0% = yellow steady ON, 100% = OFF, intermediate blink ON=(100-pct)*period.
 *         Green steady ON (input present). One cycle.
 *         [FA] سناریوی شارژ — زرد مانده تا فول را نشان می‌دهد.
 *         ۰٪ زرد ثابت روشن، ۱۰۰٪ خاموش، بینشان ON=(۱۰۰-درصد)*دوره.
 *         سبز ثابت روشن.
 * @param  u32_batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void Ui_ScenarioCharging(uint32_t u32_batteryMv)
{
    uint8_t u8_batteryPercent;
    uint32_t u32_onMs;
    uint32_t u32_offMs;

    u8_batteryPercent = Ui_BatteryVoltageToPercent(u32_batteryMv);

    /* [EN] Charging: yellow ON time proportional to remaining (100-pct) / شارژ: ON برابر مانده تا فول */
    if (u8_batteryPercent >= UI_PERCENT_FULL)
    {
        /* [EN] 100% => yellow OFF / فول زرد خاموش */
        yellow(false);
        green(true);
        red(false);
        buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    if (u8_batteryPercent == 0u)
    {
        /* [EN] 0% => yellow steady ON / صفر درصد زرد ثابت روشن */
        yellow(true);
        green(true);
        red(false);
        buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    /* [EN] Intermediate: ON = (100-pct)*period/100, OFF = rest / بینابین */
    u32_onMs = (uint32_t)(UI_PERCENT_FULL - u8_batteryPercent) *
               (UI_CHARGING_BLINK_PERIOD_MS / UI_PERCENT_SCALE);

    if (u32_onMs < UI_CHARGING_YELLOW_MIN_OFF_MS)
    {
        u32_onMs = UI_CHARGING_YELLOW_MIN_OFF_MS;
    }

    if (u32_onMs > UI_CHARGING_BLINK_PERIOD_MS)
    {
        u32_onMs = UI_CHARGING_BLINK_PERIOD_MS;
    }

    u32_offMs = UI_CHARGING_BLINK_PERIOD_MS - u32_onMs;

    /* [EN] Green steady ON indicates input present, red off / سبز ثابت ورودی وصل */
    green(true);
    red(false);
    buzzer(false);

    yellow(true);
    vTaskDelay(pdMS_TO_TICKS(u32_onMs));
    yellow(false);
    vTaskDelay(pdMS_TO_TICKS(u32_offMs));
}
