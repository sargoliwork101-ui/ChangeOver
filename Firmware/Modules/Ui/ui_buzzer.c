/**
 * @file    ui_buzzer.c
 * @brief   [EN] One non-blocking periodic buzzer service. It calculates pulse timing from period, duty, count, and gap.
 *          [FA] یک سرویس غیرمسدودکننده بوق دوره‌ای؛ زمان پالس را از دوره، دیوتی، تعداد و گپ محاسبه می‌کند.
 *
 * @note    [EN] No automatic scenario owns the buzzer. A caller explicitly supplies the four inputs to func__Ui_Buzzer_Tick().
 *          [FA] هیچ سناریویی به‌صورت خودکار مالک بوق نیست؛ فراخواننده چهار ورودی را صریحاً به func__Ui_Buzzer_Tick() می‌دهد.
 */

#include "ui_buzzer.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>
#include <stdint.h>

/* ==================== Buzzer / Beep ==================== */

/* ==================== Buzzer state validity ==================== */

static bool BOOL__G__BuzzerPatternValid = false;

/* ==================== Buzzer period ==================== */

static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;

/* ==================== Buzzer duty and count ==================== */

static uint8_t UINT8_T__G__BuzzerDutyPercent = 0u;
static uint8_t UINT8_T__G__BuzzerCount = 0u;

/* ==================== Buzzer gap ==================== */

static uint32_t UINT32_T__G__BuzzerGapMs = 0u;

/* ==================== Buzzer cycle start ==================== */

static TickType_t TICKTYPE_T__G__BuzzerCycleStartTick = 0;

/* ==================== Buzzer service ==================== */

/**
 * @brief  [EN] Service one periodic buzzer pattern without blocking the task.
 *         The duty window is period*duty/100. Pulses and gaps fill that window;
 *         the remaining time is low until the next period starts.
 *         [FA] یک الگوی دوره‌ای بوق را بدون قفل کردن تسک اجرا می‌کند.
 *         پنجره دیوتی period*duty/100 است؛ بوق‌ها و گپ‌ها این پنجره را پر می‌کنند
 *         و زمان باقی‌مانده تا دوره بعدی خاموش است.
 * @param  uint32_t__periodMs [EN] Pattern period in ms, non-zero / دوره الگو بر حسب ms، غیرصفر
 * @param  uint8_t__dutyPercent [EN] Duty window 0..100 percent / پنجره دیوتی از صفر تا صد درصد
 * @param  uint8_t__beepCount [EN] Number of pulses in duty window; zero disables / تعداد پالس در پنجره دیوتی؛ صفر یعنی خاموش
 * @param  uint32_t__gapMs [EN] Low gap between adjacent pulses in ms / گپ خاموش بین پالس‌های مجاور بر حسب ms
 */
void func__Ui_Buzzer_Tick(uint32_t uint32_t__periodMs, uint8_t uint8_t__dutyPercent, uint8_t uint8_t__beepCount, uint32_t uint32_t__gapMs)
{
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__dutyWindowMs;
    uint32_t uint32_t__gapCount;
    uint32_t uint32_t__totalGapMs;
    uint32_t uint32_t__availableOnMs;
    uint32_t uint32_t__beepOnMs;
    uint32_t uint32_t__lastBeepOnMs;
    uint32_t uint32_t__onRemainderMs;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__cursorMs;
    uint32_t uint32_t__currentBeepOnMs;
    uint8_t uint8_t__beepIndex;
    uint64_t uint64_t__dutyProduct;
    uint64_t uint64_t__gapProduct;
    bool bool__configurationChanged;
    bool bool__buzzerOn;

    if ((uint32_t__periodMs == 0u) ||
        (uint8_t__dutyPercent == 0u) ||
        (uint8_t__beepCount == 0u) ||
        (uint8_t__dutyPercent > UI_BUZZER_DUTY_MAX_PERCENT))
    {
        func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
        BOOL__G__BuzzerPatternValid = false;
        return;
    }

    /* [EN] Calculate the duty window without overflowing a 32-bit period.
       [FA] پنجره دیوتی را بدون سرریز دوره ۳۲ بیتی محاسبه کن. */
    uint64_t__dutyProduct = (uint64_t)uint32_t__periodMs * (uint64_t)uint8_t__dutyPercent;
    uint32_t__dutyWindowMs = (uint32_t)(uint64_t__dutyProduct / UI_BUZZER_PERCENT_SCALE);

    uint32_t__gapCount = (uint32_t)uint8_t__beepCount - 1u;
    uint64_t__gapProduct = (uint64_t)uint32_t__gapMs * (uint64_t)uint32_t__gapCount;

    /* [EN] Reject a pattern when gaps leave no positive time for every pulse.
       [FA] اگر گپ‌ها زمان مثبت برای همه پالس‌ها باقی نگذارند، الگو را رد کن. */
    if ((uint32_t__dutyWindowMs == 0u) ||
        (uint64_t__gapProduct >= (uint64_t)uint32_t__dutyWindowMs) ||
        ((uint32_t__dutyWindowMs - (uint32_t)uint64_t__gapProduct) < (uint32_t)uint8_t__beepCount))
    {
        func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
        BOOL__G__BuzzerPatternValid = false;
        return;
    }

    uint32_t__totalGapMs = (uint32_t)uint64_t__gapProduct;
    uint32_t__availableOnMs = uint32_t__dutyWindowMs - uint32_t__totalGapMs;
    uint32_t__beepOnMs = uint32_t__availableOnMs / (uint32_t)uint8_t__beepCount;
    uint32_t__onRemainderMs = uint32_t__availableOnMs % (uint32_t)uint8_t__beepCount;
    uint32_t__lastBeepOnMs = uint32_t__beepOnMs + uint32_t__onRemainderMs;

    bool__configurationChanged = (BOOL__G__BuzzerPatternValid == false);
    if (UINT32_T__G__BuzzerPeriodMs != uint32_t__periodMs)
    {
        bool__configurationChanged = true;
    }
    if (UINT8_T__G__BuzzerDutyPercent != uint8_t__dutyPercent)
    {
        bool__configurationChanged = true;
    }
    if (UINT8_T__G__BuzzerCount != uint8_t__beepCount)
    {
        bool__configurationChanged = true;
    }
    if (UINT32_T__G__BuzzerGapMs != uint32_t__gapMs)
    {
        bool__configurationChanged = true;
    }

    ticktype__nowTick = xTaskGetTickCount();
    if (bool__configurationChanged == true)
    {
        UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
        UINT8_T__G__BuzzerDutyPercent = uint8_t__dutyPercent;
        UINT8_T__G__BuzzerCount = uint8_t__beepCount;
        UINT32_T__G__BuzzerGapMs = uint32_t__gapMs;
        TICKTYPE_T__G__BuzzerCycleStartTick = ticktype__nowTick;
        BOOL__G__BuzzerPatternValid = true;
    }

    uint32_t__elapsedMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__BuzzerCycleStartTick) * portTICK_PERIOD_MS);
    if (uint32_t__elapsedMs >= uint32_t__periodMs)
    {
        TICKTYPE_T__G__BuzzerCycleStartTick = ticktype__nowTick;
        uint32_t__elapsedMs = 0u;
    }

    bool__buzzerOn = false;
    if (uint32_t__elapsedMs < uint32_t__dutyWindowMs)
    {
        uint32_t__cursorMs = 0u;

        for (uint8_t__beepIndex = 0u; uint8_t__beepIndex < uint8_t__beepCount; uint8_t__beepIndex++)
        {
            uint32_t__currentBeepOnMs = uint32_t__beepOnMs;
            if (uint8_t__beepIndex == (uint8_t__beepCount - 1u))
            {
                uint32_t__currentBeepOnMs = uint32_t__lastBeepOnMs;
            }

            if (uint32_t__elapsedMs < (uint32_t__cursorMs + uint32_t__currentBeepOnMs))
            {
                bool__buzzerOn = true;
                break;
            }

            uint32_t__cursorMs += uint32_t__currentBeepOnMs;
            if (uint8_t__beepIndex < (uint8_t__beepCount - 1u))
            {
                if (uint32_t__elapsedMs < (uint32_t__cursorMs + uint32_t__gapMs))
                {
                    break;
                }
                uint32_t__cursorMs += uint32_t__gapMs;
            }
        }
    }

    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool__buzzerOn);
}
