/**
 * @file    bsp_pwm.c
 * @brief   [EN] STM32F103C8T6 PWM port for schematic MCU_PWM1/MCU_PWM2.
 *          [FA] پورت PWM برای MCU_PWM1/MCU_PWM2 شماتیک روی STM32F103C8T6.
 *
 * @note    [EN] The .ioc initializes TIM2_CH1 on PA0 and TIM3_CH1 on PA6.
 *              Both timers RUN CONTINUOUSLY from func__BspPwm_Init with a
 *              frozen half-period phase offset (10 us at 50 kHz): the two
 *              gate pulses rise exactly one half period apart, can never
 *              switch simultaneously, and the interleave can never slip
 *              because the counters are never stopped or rewritten again.
 *              A channel is switched off by compare=0 alone.
 *          [FA] فایل .ioc، TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 را مقداردهی
 *              می‌کند و کد محصول فقط کانال‌های منطقی شارژر را می‌بیند. از
 *              زمان Init هر دو تایمر پیوسته می‌چرخند با آفست فاز ثابتِ
 *              نیم‌دوره (۱۰µs در ۵۰kHz): پالس گیت دوم دقیقاً نیم‌دوره بعد از
 *              پالس اول بالا می‌آید، دو استیج هرگز همزمان سوییچ نمی‌کنند و
 *              چون شمارنده‌ها دیگر متوقف یا بازنویسی نمی‌شوند، این درهم‌گذاری
 *              هرگز نمی‌لغزد. خاموش‌کردن یک کانال فقط با compare=0 انجام
 *              می‌شود.
 */

#include "bsp_pwm.h"
#include "main.h"

#include <stdbool.h>
#include <stddef.h>

/* ==================== BspPwm_GetTimer ==================== */
/**
 * @brief  [EN] Resolve a logical channel to its board timer and channel.
 *         [FA] کانال منطقی را به تایمر و کانال فیزیکی برد تبدیل می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel /
 *                                     کانال منطقی
 * @param  TIM_HandleTypeDef__timer [EN] Output timer handle / هندل تایمر خروجی
 * @param  uint32_t__halChannel [EN] Output HAL channel identifier /
 *                                   شناسهٔ کانال HAL خروجی
 * @return bool [EN] true for a valid channel / برای کانال معتبر true درست
 */
static bool func__BspPwm_GetTimer(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   TIM_HandleTypeDef **TIM_HandleTypeDef__timer,
                                   uint32_t *uint32_t__halChannel)
{
    if ((TIM_HandleTypeDef__timer == NULL) ||
        (uint32_t__halChannel == NULL))
    {
        return false;
    }

    switch (bsp_pwm_channel_t__channel)
    {
        case BSP_PWM_CHARGER_1:
            *TIM_HandleTypeDef__timer = &htim2;
            *uint32_t__halChannel = TIM_CHANNEL_1;
            break;
        case BSP_PWM_CHARGER_2:
            *TIM_HandleTypeDef__timer = &htim3;
            *uint32_t__halChannel = TIM_CHANNEL_1;
            break;
        default:
            return false;
    }

    return true;
}

/* ==================== BspPwm_SetOneDuty ==================== */
/**
 * @brief  [EN] Apply one clamped duty to one timer channel as a compare
 *              value only. The timer counter itself is never touched here:
 *              both counters run continuously from Init with a frozen
 *              half-period offset, so the interleave cannot slip. With
 *              permille=0 the compare is 0, which keeps that gate low
 *              (PWM1: CNT < 0 is never true).
 *         [FA] وظیفهٔ محدودشدهٔ یک کانال را فقط به‌صورت compare اعمال
 *              می‌کند. شمارندهٔ تایمر اینجا دست نمی‌خورد: هر دو شمارنده از
 *              زمان Init پیوسته با آفست نیم‌دوره می‌چرخند پس درهم‌گذاری
 *              نمی‌لغزد. با permille=0 مقدار compare صفر می‌شود و گیت پایین
 *              می‌ماند.
 * @param  TIM_HandleTypeDef__timer [EN] Board timer handle / هندل تایمر برد
 * @param  uint32_t__halChannel [EN] HAL channel / کانال HAL
 * @param  uint16_t__permille [EN] Duty in 0..1000 permille /
 *                                 وظیفه در بازهٔ ۰ تا ۱۰۰۰ پرمیل
 */
static void func__BspPwm_SetOneDuty(TIM_HandleTypeDef *TIM_HandleTypeDef__timer,
                                     uint32_t uint32_t__halChannel,
                                     uint16_t uint16_t__permille)
{
    uint32_t uint32_t__autoReload;
    uint32_t uint32_t__periodCounts;
    uint32_t uint32_t__compareCounts;

    if (TIM_HandleTypeDef__timer == NULL)
    {
        return;
    }

    if (uint16_t__permille > 1000u)
    {
        uint16_t__permille = 1000u;
    }

    uint32_t__autoReload = __HAL_TIM_GET_AUTORELOAD(TIM_HandleTypeDef__timer);
    uint32_t__periodCounts = uint32_t__autoReload + 1u;
    uint32_t__compareCounts =
        (uint32_t__periodCounts * (uint32_t)uint16_t__permille) / 1000u;

    if (uint32_t__compareCounts > uint32_t__autoReload)
    {
        uint32_t__compareCounts = uint32_t__autoReload;
    }

    __HAL_TIM_SET_COMPARE(TIM_HandleTypeDef__timer,
                          uint32_t__halChannel,
                          uint32_t__compareCounts);
}

/* ==================== BspPwm_Init ==================== */
/**
 * @brief  [EN] Force both gates low, then start both timers ONCE with a
 *              frozen half-period (10 us at 50 kHz) phase offset: TIM3 is
 *              started at CNT = period/2 while TIM2 starts at CNT = 0, so
 *              the channel-2 gate pulse always rises exactly half a period
 *              after the channel-1 gate pulse (user order 2026-09-21). The
 *              offset is derived from the live ARR, not a hardcoded 720, so
 *              it follows any future period change. Both timers share one
 *              72 MHz clock and identical ARR, hence the frozen offset
 *              cannot drift. From this point on the counters are never
 *              stopped again; off = compare 0.
 *         [FA] ابتدا هر دو گیت پایین می‌آیند، سپس هر دو تایمر فقط یک‌بار با
 *              آفست فاز ثابتِ نیم‌دوره (۱۰µs در ۵۰kHz) استارت می‌شوند: TIM3
 *              از نیم‌دوره و TIM2 از صفر، تا پالس گیت کانال دو دقیقاً
 *              نیم‌دوره بعد از پالس گیت کانال یک بالا بیاید (دستور کاربر
 *              ۲۰۲۶-۰۹-۲۱). آفست از ARR واقعی محاسبه می‌شود نه عدد ثابت ۷۲۰
 *              تا با تغییر دوره همراه شود. هر دو تایمر روی یک کلاک ۷۲MHz و
 *              ARR یکسان‌اند پس آفست رانش ندارد. از این‌به‌بعد شمارنده‌ها
 *              دیگر متوقف نمی‌شوند؛ خاموش یعنی compare صفر.
 */
void func__BspPwm_Init(void)
{
    uint32_t uint32_t__periodCounts;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);

    uint32_t__periodCounts = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1u;
    __HAL_TIM_SET_COUNTER(&htim2, 0u);
    __HAL_TIM_SET_COUNTER(&htim3, uint32_t__periodCounts / 2u);

    (void)HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/* ==================== BspPwm_SetDutyPermille ==================== */
/**
 * @brief  [EN] Set one logical charger duty in 0..1000 permille.
 *         [FA] وظیفهٔ یک شارژر منطقی را در بازهٔ ۰ تا ۱۰۰۰ پرمیل تنظیم می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel / کانال منطقی
 * @param  uint16_t__permille [EN] Duty in permille / وظیفه بر حسب پرمیل
 */
void func__BspPwm_SetDutyPermille(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   uint16_t uint16_t__permille)
{
    TIM_HandleTypeDef *TIM_HandleTypeDef__timer = NULL;
    uint32_t uint32_t__halChannel = 0u;

    if (func__BspPwm_GetTimer(bsp_pwm_channel_t__channel,
                              &TIM_HandleTypeDef__timer,
                              &uint32_t__halChannel) == true)
    {
        func__BspPwm_SetOneDuty(TIM_HandleTypeDef__timer,
                                uint32_t__halChannel,
                                uint16_t__permille);
    }
}

/* ==================== BspPwm_StopAll ==================== */
/**
 * @brief  [EN] Force both gates low (compare 0). The counters keep running
 *              on purpose, so the frozen 10 us interleave survives every
 *              SafeIdle/fault episode and is still correct when a channel
 *              returns.
 *         [FA] هر دو گیت را پایین می‌آورد (compare صفر). شمارنده‌ها عمداً به
 *              چرخش می‌مانند تا درهم‌گذاری ثابت ۱۰µs در تمام قسمت‌های
 *              SafeIdle و خطا حفظ شود و هنگام بازگشت کانال درست باقی بماند.
 */
void func__BspPwm_StopAll(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);
}
