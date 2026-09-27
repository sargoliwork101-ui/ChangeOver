/**
 * @file    ui_buzzer.c
 * @brief   [EN] One non-blocking periodic buzzer service. It calculates pulse timing from period, duty, count, and gap.
 *          [FA] یک سرویس غیرمسدودکننده بوق دوره‌ای؛ زمان پالس را از دوره، دیوتی، تعداد و گپ محاسبه می‌کند.
 *
 * @note    [EN] Only explicit scenarios call this service; all buzzer GPIO control remains here. The caller supplies the four inputs to func__Ui_Buzzer_Tick().
 *          [FA] فقط سناریوهای صریح این سرویس را صدا می‌زنند؛ کنترل GPIO بوق همچنان در این فایل است و فراخواننده چهار ورودی را می‌دهد.
 */

#include "ui_buzzer.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include "rtos_time.h"

#include <stdbool.h>
#include <stdint.h>

/* ==================== Buzzer / Beep / بازر و بوق ==================== */

/* ==================== Buzzer persistent state / وضعیت ماندگار بازر ==================== */

/**
 * @brief  [EN] File-local validity flag for the stored buzzer configuration.
 *         It remains static because func__Ui_Buzzer_Tick() is called repeatedly
 *         and must know whether the current pattern has already been initialized.
 *         FALSE forces the next valid request to start a new buzzer cycle.
 *         [FA] پرچم اعتبار پیکربندی ذخیره‌شده بوق در سطح همین فایل.
 *         این متغیر static است چون func__Ui_Buzzer_Tick() به‌صورت تکراری صدا زده می‌شود
 *         و باید بداند آیا الگوی فعلی قبلاً مقداردهی شده است یا نه.
 *         مقدار FALSE باعث می‌شود درخواست معتبر بعدی از ابتدای یک چرخه جدید شروع شود.
 */
static bool BOOL__G__BuzzerPatternValid = false;

/* ==================== Buzzer period / دوره بازر ==================== */

/**
 * @brief  [EN] Previously accepted complete buzzer period in milliseconds.
 *         It remains static so the service can compare the new period with the
 *         previous one and restart the timing cycle when the period changes.
 *         [FA] دوره کامل قبلی بوق بر حسب میلی‌ثانیه.
 *         این متغیر static است تا سرویس بتواند دوره جدید را با دوره قبلی مقایسه کند
 *         و اگر دوره تغییر کرد، زمان‌بندی چرخه را از ابتدا شروع کند.
 */
static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;

/* ==================== Buzzer duty and count / دیوتی و تعداد بوق ==================== */

/**
 * @brief  [EN] Previously accepted duty-window percentage.
 *         It remains static so a change in duty can be detected between
 *         consecutive service calls and the pattern can be restarted safely.
 *         [FA] درصد پنجره دیوتی پذیرفته‌شده قبلی.
 *         این متغیر static است تا تغییر دیوتی بین دو فراخوانی متوالی تشخیص داده شود
 *         و الگو در صورت تغییر، به‌صورت امن از ابتدا شروع شود.
 */
static uint8_t UINT8_T__G__BuzzerDutyPercent = 0u;

/**
 * @brief  [EN] Previously accepted number of beeps inside the duty window.
 *         It remains static so changing the pulse count can restart the pattern
 *         instead of continuing with the timing of the old configuration.
 *         [FA] تعداد بوق‌های پذیرفته‌شده قبلی در پنجره دیوتی.
 *         این متغیر static است تا تغییر تعداد پالس باعث شروع دوباره الگو شود
 *         و زمان‌بندی پیکربندی قبلی ادامه پیدا نکند.
 */
static uint8_t UINT8_T__G__BuzzerCount = 0u;

/* ==================== Buzzer gap / گپ بوق ==================== */

/**
 * @brief  [EN] Previously accepted low gap between adjacent beeps in milliseconds.
 *         It remains static so a gap change can be detected and the cycle timing
 *         can be restarted with the new pattern.
 *         [FA] گپ خاموش پذیرفته‌شده قبلی بین بوق‌های مجاور بر حسب میلی‌ثانیه.
 *         این متغیر static است تا تغییر گپ تشخیص داده شود و زمان‌بندی چرخه
 *         با الگوی جدید از ابتدا شروع شود.
 */
static uint32_t UINT32_T__G__BuzzerGapMs = 0u;

/* ==================== Buzzer cycle start / شروع چرخه بوق ==================== */

/**
 * @brief  [EN] CMSIS-RTOS2 tick at which the current buzzer cycle started.
 *         It remains static because the service is non-blocking and is called
 *         in separate invocations. The stored tick is used to calculate elapsed
 *         time and decide whether the buzzer is ON, in a gap, or in the period tail.
 *         [FA] تیک RTOS در زمان شروع چرخه فعلی بوق.
 *         این متغیر static است چون سرویس غیرمسدودکننده است و در فراخوانی‌های جداگانه
 *         اجرا می‌شود. از این زمان برای محاسبه زمان سپری‌شده و تشخیص وضعیت بوق،
 *         گپ یا خاموشی انتهای دوره استفاده می‌شود.
 */
static uint32_t TICKTYPE_T__G__BuzzerCycleStartTick = 0;

/* ==================== Buzzer service / سرویس بازر ==================== */

/**
 * @brief  [EN] Service one periodic buzzer pattern without blocking the task.
 *         The duty window is period*duty/100. Pulses and gaps fill that window;
 *         the remaining time is low until the next period starts.
 *         [FA] یک الگوی دوره‌ای بوق را بدون قفل کردن تسک اجرا می‌کند.
 *         پنجره دیوتی period*duty/100 است؛ بوق‌ها و گپ‌ها این پنجره را پر می‌کنند
 *         و زمان باقی‌مانده تا دوره بعدی خاموش است.
 * @param  uint32_t__periodMs [EN] Pattern period in ms / دوره الگو بر حسب ms
 * @param  uint8_t__dutyPercent [EN] Duty window 0..100 percent / پنجره دیوتی از صفر تا صد درصد
 * @param  uint8_t__beepCount [EN] Number of pulses; zero disables / تعداد پالس؛ صفر یعنی خاموش
 * @param  uint32_t__gapMs [EN] Low gap between adjacent pulses in ms / گپ خاموش بین پالس‌های مجاور بر حسب ms
 * @return int32_t [EN] Recommended next-call delay, zero for valid off, or -1 for invalid input.
 *         [FA] زمان پیشنهادی مراجعه بعدی، صفر برای خاموشی معتبر، یا منفی یک برای ورودی نامعتبر.
 */
int32_t func__Ui_Buzzer_Tick(uint32_t uint32_t__periodMs, uint8_t uint8_t__dutyPercent, uint8_t uint8_t__beepCount, uint32_t uint32_t__gapMs)
{
    uint32_t ticktype__nowTick;
    uint32_t uint32_t__dutyWindowMs;
    uint32_t uint32_t__gapCount;
    uint32_t uint32_t__effectiveGapMs;
    uint32_t uint32_t__totalGapMs;
    uint32_t uint32_t__availableOnMs;
    uint32_t uint32_t__beepOnMs;
    uint32_t uint32_t__lastBeepOnMs;
    uint32_t uint32_t__onRemainderMs;
    uint32_t uint32_t__periodTailMs;
    uint32_t uint32_t__smallestTimingMs;
    uint32_t uint32_t__nextCheckMs;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__cursorMs;
    uint32_t uint32_t__currentBeepOnMs;
    uint8_t uint8_t__beepIndex;
    uint32_t uint32_t__gapProduct;
    bool bool__configurationChanged;
    bool bool__buzzerOn;

    /* [EN] Zero is an intentional safe-off command, not an invalid configuration.
       [FA] صفر یک فرمان خاموشی امن و عمدی است، نه تنظیمات نامعتبر. */
    if ((uint32_t__periodMs == 0u) ||
        (uint8_t__dutyPercent == 0u) ||
        (uint8_t__beepCount == 0u))
    {
        func__BspGpio_Write(BSP_GPIO_BUZZER, false);
        BOOL__G__BuzzerPatternValid = false;
        return UI_BUZZER_OFF_RESULT;
    }

    /* [EN] Reject unsafe non-zero configurations and keep the buzzer LOW.
       [FA] تنظیمات غیرصفر ناامن را رد کن و بوق را LOW نگه دار. */
    if ((uint32_t__periodMs < UI_BUZZER_MIN_PERIOD_MS) ||
        (uint8_t__dutyPercent > UI_BUZZER_DUTY_MAX_PERCENT) ||
        ((uint8_t__beepCount > 1u) && (uint32_t__gapMs < UI_BUZZER_MIN_GAP_MS)))
    {
        func__BspGpio_Write(BSP_GPIO_BUZZER, false);
        BOOL__G__BuzzerPatternValid = false;
        return UI_BUZZER_INVALID_RESULT;
    }

    /* [EN] A gap has meaning only between adjacent pulses.
       [FA] گپ فقط بین پالس‌های مجاور معنا دارد. */
    uint32_t__effectiveGapMs = 0u;
    if (uint8_t__beepCount > 1u)
    {
        uint32_t__effectiveGapMs = uint32_t__gapMs;
    }

    /* [EN] Duty window without overflowing a 32-bit period and WITHOUT u64
       (flash diet 2026-09-27): (p/100)*d + ((p%100)*d)/100 is the identical
       quotient for the FULL u32 range, and (p/100)*d peaks at 4,294,967,200
       < 2^32 because duty <= 100 (proven over 200k random + edge values).
       The u64 division pulled __aeabi_uldivmod (~1 KiB).
       [FA] پنجره دیوتی بدون سرریز و بدون ۶۴بیت: خارج‌قسمت یکسان برای کل
       بازه ۳۲بیت (دیوتی ≤۱۰۰ پس ضرب جا می‌شود). */
    uint32_t__dutyWindowMs =
        ((uint32_t__periodMs / UI_BUZZER_PERCENT_SCALE) * (uint32_t)uint8_t__dutyPercent) +
        (((uint32_t__periodMs % UI_BUZZER_PERCENT_SCALE) * (uint32_t)uint8_t__dutyPercent) /
         UI_BUZZER_PERCENT_SCALE);

    uint32_t__gapCount = (uint32_t)uint8_t__beepCount - 1u;
    /* [EN] Gap total in u32 (flash diet 2026-09-27): an overflowing product
       can only exceed the u32 duty window, i.e. exactly the reject the u64
       path took - clamp it to UINT32_MAX and the comparisons below keep
       identical outcomes; otherwise the product is identical.
       [FA] جمع گپ‌ها با ۳۲بیت: سرریز فقط یعنی رد همان‌طور که قبلاً بود. */
    uint32_t__gapProduct = ((uint32_t__gapCount != 0u) &&
                            (uint32_t__effectiveGapMs > (UINT32_MAX / uint32_t__gapCount)))
                               ? UINT32_MAX
                               : (uint32_t__effectiveGapMs * uint32_t__gapCount);

    /* [EN] Reject a pattern when gaps leave no positive time for every pulse.
       [FA] اگر گپ‌ها زمان مثبت برای همه پالس‌ها باقی نگذارند، الگو را رد کن. */
    if ((uint32_t__dutyWindowMs == 0u) ||
        (uint32_t__gapProduct >= uint32_t__dutyWindowMs) ||
        ((uint32_t__dutyWindowMs - uint32_t__gapProduct) < (uint32_t)uint8_t__beepCount))
    {
        func__BspGpio_Write(BSP_GPIO_BUZZER, false);
        BOOL__G__BuzzerPatternValid = false;
        return UI_BUZZER_INVALID_RESULT;
    }

    uint32_t__totalGapMs = uint32_t__gapProduct;
    uint32_t__availableOnMs = uint32_t__dutyWindowMs - uint32_t__totalGapMs;
    uint32_t__beepOnMs = uint32_t__availableOnMs / (uint32_t)uint8_t__beepCount;
    uint32_t__onRemainderMs = uint32_t__availableOnMs % (uint32_t)uint8_t__beepCount;
    uint32_t__lastBeepOnMs = uint32_t__beepOnMs + uint32_t__onRemainderMs;
    uint32_t__periodTailMs = uint32_t__periodMs - uint32_t__dutyWindowMs;

    /* [EN] The smallest positive segment controls the next RTOS check.
       [FA] کوچک‌ترین بخش مثبت، زمان مراجعه بعدی RTOS را تعیین می‌کند. */
    uint32_t__smallestTimingMs = uint32_t__beepOnMs;
    if (uint32_t__lastBeepOnMs < uint32_t__smallestTimingMs)
    {
        uint32_t__smallestTimingMs = uint32_t__lastBeepOnMs;
    }
    if ((uint32_t__effectiveGapMs > 0u) &&
        (uint32_t__effectiveGapMs < uint32_t__smallestTimingMs))
    {
        uint32_t__smallestTimingMs = uint32_t__effectiveGapMs;
    }
    if ((uint32_t__periodTailMs > 0u) &&
        (uint32_t__periodTailMs < uint32_t__smallestTimingMs))
    {
        uint32_t__smallestTimingMs = uint32_t__periodTailMs;
    }

    /* [EN] Next-check percent WITHOUT u64 (flash diet 2026-09-27): same
       exact split as the duty window (CHECK_PERCENT = 10 <= 100).
       [FA] درصد مراجعه بعدی بدون ۶۴بیت: همان تجزیهٔ دقیق. */
    uint32_t__nextCheckMs =
        ((uint32_t__smallestTimingMs / UI_BUZZER_PERCENT_SCALE) * UI_BUZZER_CHECK_PERCENT) +
        (((uint32_t__smallestTimingMs % UI_BUZZER_PERCENT_SCALE) * UI_BUZZER_CHECK_PERCENT) /
         UI_BUZZER_PERCENT_SCALE);
    if (uint32_t__nextCheckMs < UI_BUZZER_MIN_CHECK_MS)
    {
        uint32_t__nextCheckMs = UI_BUZZER_MIN_CHECK_MS;
    }

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
    if (UINT32_T__G__BuzzerGapMs != uint32_t__effectiveGapMs)
    {
        bool__configurationChanged = true;
    }

    ticktype__nowTick = osKernelGetTickCount();
    if (bool__configurationChanged == true)
    {
        UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
        UINT8_T__G__BuzzerDutyPercent = uint8_t__dutyPercent;
        UINT8_T__G__BuzzerCount = uint8_t__beepCount;
        UINT32_T__G__BuzzerGapMs = uint32_t__effectiveGapMs;
        TICKTYPE_T__G__BuzzerCycleStartTick = ticktype__nowTick;
        BOOL__G__BuzzerPatternValid = true;
    }

    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(ticktype__nowTick - TICKTYPE_T__G__BuzzerCycleStartTick);
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
                if (uint32_t__elapsedMs < (uint32_t__cursorMs + uint32_t__effectiveGapMs))
                {
                    break;
                }
                uint32_t__cursorMs += uint32_t__effectiveGapMs;
            }
        }
    }

    func__BspGpio_Write(BSP_GPIO_BUZZER, bool__buzzerOn);
    return (int32_t)uint32_t__nextCheckMs;
}
