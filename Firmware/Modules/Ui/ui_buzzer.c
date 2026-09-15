/**
 * @file    ui_buzzer.c
 * @brief   [EN] UI buzzer patterns - separate from LED, constants for buzzer in its own header.
 *          Non-linear formulas broken into steps, RTOS simple readable, markers above each func and variable.
 *          [FA] الگوهای بازر ماژول UI - ثابت‌های بازر در هدر خودش، هر تابع و متغیر با جدا کننده و کامنت.
 *
 * @note    [EN] Buzzer constants in ui_buzzer.h per user request. Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed, HAL_Delay forbidden. Formulas non-linear broken into steps.
 *          [FA] ثابت‌های بازر در همین هدر. نام‌گذاری با __، پیشوند func__، فرمول غیرخطی.
 */

#include "ui_buzzer.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ==================== Buzzer / Beep ==================== */

/* ==================== Buzzer Low Level ==================== */

/**
 * @brief  [EN] Drive buzzer on/off. Low-level wrapper around BSP GPIO.
 *         [FA] بازر را روشن/خاموش می‌کند - سطح پایین.
 * @param  bool__buzzerOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__buzzer(bool bool__buzzerOn)
{
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool__buzzerOn);
}

/* ==================== Calc Beep On ==================== */

/**
 * @brief  [EN] Calculate single pulse ON time from total ON, repeat count, gap. Non-linear broken into steps.
 *         Formula: repeatMinusOne = repeat-1, totalGap = gap*repeatMinusOne, if totalGap>=totalOn => denominator=repeat*2-1, pulseOn=totalOn/denominator else pulseOn=(totalOn-totalGap)/repeat.
 *         [FA] محاسبه زمان روشن هر بوق از کل روشن، تکرار، گپ - غیرخطی گام به گام.
 * @param  uint32_t__totalOnMs [EN] Total ON ms including gaps, 10..10000ms / کل زمان روشن شامل گپ
 * @param  uint8_t__repeatCount [EN] Repeat inside ON 1..10 / تکرار داخل روشن
 * @param  uint32_t__gapMs [EN] Gap ms 0..5000 / گپ میلی‌ثانیه
 * @return uint32_t [EN] Single pulse ON ms / زمان روشن هر بوق
 */
static uint32_t func__calc_beep_on(uint32_t uint32_t__totalOnMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__totalGapMs;
    uint32_t uint32_t__pulseOnMs;
    uint32_t uint32_t__repeatMinusOne;
    uint32_t uint32_t__denominator;

    if (uint8_t__repeatCount <= 1u)
    {
        return uint32_t__totalOnMs;
    }

    uint32_t__repeatMinusOne = (uint32_t)uint8_t__repeatCount - 1u;
    uint32_t__totalGapMs = uint32_t__gapMs * uint32_t__repeatMinusOne;

    if (uint32_t__totalGapMs >= uint32_t__totalOnMs)
    {
        uint32_t__denominator = (uint32_t)uint8_t__repeatCount * 2u - 1u;
        uint32_t__pulseOnMs = uint32_t__totalOnMs / uint32_t__denominator;
        if (uint32_t__pulseOnMs < 10u)
        {
            uint32_t__pulseOnMs = 10u;
        }
        return uint32_t__pulseOnMs;
    }

    uint32_t__pulseOnMs = (uint32_t__totalOnMs - uint32_t__totalGapMs) / (uint32_t)uint8_t__repeatCount;
    if (uint32_t__pulseOnMs < 10u)
    {
        uint32_t__pulseOnMs = 10u;
    }
    return uint32_t__pulseOnMs;
}

/* ==================== Buzzer State Type ==================== */

/**
 * @brief  [EN] Buzzer state machine type: IDLE, PULSE_ON, GAP_OFF, PERIOD_OFF.
 *         [FA] نوع حالت بازر: بیکار، روشن، گپ، خاموش دوره‌ای.
 */
typedef enum { BUZZER_IDLE, BUZZER_PULSE_ON, BUZZER_GAP_OFF, BUZZER_PERIOD_OFF } buzzer_state_t;

/* ==================== Buzzer State ==================== */

/**
 * @brief  [EN] Current buzzer state, IDLE at start.
 *         [FA] حالت فعلی بازر، اول بیکار.
 */
static buzzer_state_t BUZZER_STATE__G__State = BUZZER_IDLE;

/* ==================== Buzzer Total On Ms ==================== */

/**
 * @brief  [EN] Total ON time ms including gaps, clamped 10..10000ms.
 *         [FA] کل زمان روشن شامل گپ، محدود ۱۰..۱۰۰۰۰ms.
 */
static uint32_t UINT32_T__G__BuzzerTotalOnMs = 0u;

/* ==================== Buzzer Gap Ms ==================== */

/**
 * @brief  [EN] Gap between pulses ms, clamped 0..5000ms, ignored if repeat=1.
 *         [FA] گپ بین بوق‌ها میلی‌ثانیه، اگر تکرار=۱ نادیده.
 */
static uint32_t UINT32_T__G__BuzzerGapMs = 0u;

/* ==================== Buzzer Pulse On Ms ==================== */

/**
 * @brief  [EN] Single pulse ON ms calculated by func__calc_beep_on.
 *         [FA] زمان روشن هر بوق محاسبه شده.
 */
static uint32_t UINT32_T__G__BuzzerPulseOnMs = 0u;

/* ==================== Buzzer Period Ms ==================== */

/**
 * @brief  [EN] Period between pattern starts ms, 0=once, 0..60000ms.
 *         [FA] دوره تناوب بین شروع الگوها، ۰=یک بار.
 */
static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;

/* ==================== Buzzer Repeat Count ==================== */

/**
 * @brief  [EN] Repeat count inside ON 1..10, clamped.
 *         [FA] تعداد تکرار داخل روشن ۱..۱۰.
 */
static uint8_t UINT8_T__G__BuzzerRepeatCount = 0u;

/* ==================== Buzzer Pulse Index ==================== */

/**
 * @brief  [EN] Current pulse index 0..repeat-1.
 *         [FA] اندیس بوق فعلی.
 */
static uint8_t UINT8_T__G__BuzzerPulseIndex = 0u;

/* ==================== Buzzer Last Tick ==================== */

/**
 * @brief  [EN] Last tick for elapsed calculation, from xTaskGetTickCount().
 *         [FA] آخرین تیکه برای محاسبه زمان سپری شده.
 */
static TickType_t TICKTYPE_T__G__BuzzerLastTick = 0;

/* ==================== Buzzer Running ==================== */

/**
 * @brief  [EN] True if buzzer pattern running.
 *         [FA] اگر الگوی بازر در حال اجراست true.
 */
static bool BOOL__G__BuzzerRunning = false;

/* ==================== Buzzer Start Internal ==================== */

/**
 * @brief  [EN] Start buzzer pattern internal, clamps inputs, calculates pulse ON via calc_beep_on.
 *         Non-linear: gapTimesRepeat = gap*(repeat-1), if gapTimesRepeat>=totalOn => defaultGapTotal = totalOn*20%/100, gap = defaultGapTotal/(repeat-1).
 *         [FA] شروع الگوی بازر داخلی، ورودی‌ها را محدود می‌کند، فرمول غیرخطی.
 * @param  uint32_t__periodMs [EN] Period ms, 0..60000ms / دوره تناوب
 * @param  uint32_t__totalOnMs [EN] Total ON ms 10..10000ms / کل روشن
 * @param  uint8_t__repeatCount [EN] Repeat 1..10 / تکرار
 * @param  uint32_t__gapMs [EN] Gap ms 0..5000ms / گپ
 */
static void func__buzzer_start_internal(uint32_t uint32_t__periodMs, uint32_t uint32_t__totalOnMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__totalOnClamped;
    uint32_t uint32_t__gapClamped;
    uint8_t uint8_t__repeatClamped;
    uint32_t uint32_t__gapTimesRepeat;

    if (uint32_t__totalOnMs < 10u) uint32_t__totalOnClamped = 10u;
    else if (uint32_t__totalOnMs > 10000u) uint32_t__totalOnClamped = 10000u;
    else uint32_t__totalOnClamped = uint32_t__totalOnMs;

    if (uint32_t__gapMs > 5000u) uint32_t__gapClamped = 5000u;
    else uint32_t__gapClamped = uint32_t__gapMs;

    if (uint8_t__repeatCount == 0u) uint8_t__repeatClamped = 1u;
    else if (uint8_t__repeatCount > 10u) uint8_t__repeatClamped = 10u;
    else uint8_t__repeatClamped = uint8_t__repeatCount;

    if (uint8_t__repeatClamped > 1u)
    {
        uint32_t__gapTimesRepeat = uint32_t__gapClamped * (uint32_t)(uint8_t__repeatClamped - 1u);
        if (uint32_t__gapTimesRepeat >= uint32_t__totalOnClamped)
        {
            uint32_t uint32_t__defaultGapTotal;
            uint32_t__defaultGapTotal = (uint32_t__totalOnClamped * UI_BUZZER_DEFAULT_GAP_PERCENT) / 100u;
            uint32_t__gapClamped = uint32_t__defaultGapTotal / (uint32_t)(uint8_t__repeatClamped - 1u);
        }
    }

    UINT32_T__G__BuzzerTotalOnMs = uint32_t__totalOnClamped;
    UINT32_T__G__BuzzerGapMs = uint32_t__gapClamped;
    UINT8_T__G__BuzzerRepeatCount = uint8_t__repeatClamped;
    UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
    UINT8_T__G__BuzzerPulseIndex = 0u;
    UINT32_T__G__BuzzerPulseOnMs = func__calc_beep_on(uint32_t__totalOnClamped, uint8_t__repeatClamped, uint32_t__gapClamped);
    TICKTYPE_T__G__BuzzerLastTick = xTaskGetTickCount();
    BUZZER_STATE__G__State = BUZZER_PULSE_ON;
    BOOL__G__BuzzerRunning = true;
    func__buzzer(true);
}

/* ==================== Buzzer Pattern Ms Start ==================== */

/**
 * @brief  [EN] Buzzer pattern with gap ms - start pattern. Calls internal start.
 *         [FA] الگوی بازر با گپ میلی‌ثانیه - شروع.
 * @param  uint32_t__periodMs [EN] Period ms 0..60000ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] Total ON ms 10..10000ms / کل روشن
 * @param  uint8_t__repeatCount [EN] Repeat 1..10 / تکرار
 * @param  uint32_t__gapMs [EN] Gap ms 0..5000ms / گپ
 */
void func__Ui_BuzzerPatternMs_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__gapMs);
}

/* ==================== Buzzer Pattern Ms Tick ==================== */

/**
 * @brief  [EN] Buzzer pattern tick - call every UI_TICK_MS, calculates elapsed ms = (now-last)*portTICK_PERIOD_MS.
 *         Non-linear state machine: PULSE_ON -> GAP_OFF -> PULSE_ON -> PERIOD_OFF.
 *         [FA] تیکه الگوی بازر - هر ۱۰ms، محاسبه زمان سپری شده، ماشین حالت.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BuzzerPatternMs_Tick(void)
{
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__periodOffMs;

    if (BOOL__G__BuzzerRunning == false) return false;

    ticktype__nowTick = xTaskGetTickCount();
    uint32_t__elapsedMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__BuzzerLastTick) * portTICK_PERIOD_MS);

    switch (BUZZER_STATE__G__State)
    {
        case BUZZER_PULSE_ON:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerPulseOnMs)
            {
                func__buzzer(false);
                if (UINT8_T__G__BuzzerPulseIndex < UINT8_T__G__BuzzerRepeatCount - 1u)
                {
                    BUZZER_STATE__G__State = BUZZER_GAP_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                }
                else
                {
                    if (UINT32_T__G__BuzzerPeriodMs == 0u || UINT32_T__G__BuzzerPeriodMs <= UINT32_T__G__BuzzerTotalOnMs)
                    {
                        BOOL__G__BuzzerRunning = false;
                        return false;
                    }
                    BUZZER_STATE__G__State = BUZZER_PERIOD_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                }
            }
            break;

        case BUZZER_GAP_OFF:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerGapMs)
            {
                UINT8_T__G__BuzzerPulseIndex++;
                BUZZER_STATE__G__State = BUZZER_PULSE_ON;
                TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                func__buzzer(true);
            }
            break;

        case BUZZER_PERIOD_OFF:
            uint32_t__periodOffMs = UINT32_T__G__BuzzerPeriodMs - UINT32_T__G__BuzzerTotalOnMs;
            if (uint32_t__elapsedMs >= uint32_t__periodOffMs)
            {
                BOOL__G__BuzzerRunning = false;
                return false;
            }
            break;

        default:
            BOOL__G__BuzzerRunning = false;
            return false;
    }
    return true;
}

/* ==================== Buzzer Pattern Percent Start ==================== */

/**
 * @brief  [EN] Buzzer pattern with gap percent - start. Non-linear: onTimeTimesPercent = onTime*percent, gap = onTimeTimesPercent/100 broken into steps.
 *         [FA] الگوی بازر با گپ درصدی - شروع، فرمول غیرخطی گام به گام.
 * @param  uint32_t__periodMs [EN] Period ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t__repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t__gapPercent [EN] Gap percent 0..90, ignored if repeat=1 / گپ درصدی
 */
void func__Ui_BuzzerPatternPercent_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint8_t uint8_t__gapPercent)
{
    uint8_t uint8_t__gapPctClamped;
    uint32_t uint32_t__gapMs;

    if (uint8_t__gapPercent > 90u) uint8_t__gapPctClamped = 90u;
    else uint8_t__gapPctClamped = uint8_t__gapPercent;

    if (uint8_t__repeatCount <= 1u)
    {
        func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, 0u);
        return;
    }

    /* [EN] Non-linear: gap = onTime * percent / 100, broken into steps
       [FA] فرمول غیرخطی گپ درصدی */
    uint32_t uint32_t__onTimeTimesPercent = uint32_t__onTimeMs * (uint32_t)uint8_t__gapPctClamped;
    uint32_t__gapMs = uint32_t__onTimeTimesPercent / 100u;

    func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__gapMs);
}

/* ==================== Buzzer Pattern Percent Tick ==================== */

/**
 * @brief  [EN] Buzzer pattern percent tick - calls Ms Tick.
 *         [FA] تیکه الگوی بازر درصدی - صدا زدن تیکه میلی‌ثانیه.
 * @return bool [EN] true=running / در حال اجرا
 */
bool func__Ui_BuzzerPatternPercent_Tick(void)
{
    return func__Ui_BuzzerPatternMs_Tick();
}

/* ==================== Buzzer Pattern Stop ==================== */

/**
 * @brief  [EN] Stop buzzer pattern immediately, drive low, set IDLE.
 *         [FA] توقف فوری الگوی بازر، خاموش، بیکار.
 */
void func__Ui_BuzzerPattern_Stop(void)
{
    func__buzzer(false);
    BUZZER_STATE__G__State = BUZZER_IDLE;
    BOOL__G__BuzzerRunning = false;
}
