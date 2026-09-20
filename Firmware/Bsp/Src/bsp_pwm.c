/**
 * @file    bsp_pwm.c
 * @brief   [EN] STM32F103C8T6 PWM port for schematic MCU_PWM1/MCU_PWM2.
 *          [FA] پورت PWM برای MCU_PWM1/MCU_PWM2 شماتیک روی STM32F103C8T6.
 *
 * @note    [EN] The .ioc initializes TIM2_CH1 on PA0 and TIM3_CH1 on PA6.
 *              Product code sees only logical charger channels.
 *          [FA] فایل .ioc، TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 را مقداردهی
 *              می‌کند و کد محصول فقط کانال‌های منطقی شارژر را می‌بیند.
 */

#include "bsp_pwm.h"
#include "main.h"

#include <stdbool.h>
#include <stddef.h>

/* ==================== Static state / حالت داخلی ==================== */
/* [EN] Tracks whether each logical channel's PWM output is currently running,
 *      mirroring this module's own HAL_TIM_PWM_Start/Stop calls. The 180deg
 *      phase alignment only fires on a stopped->running transition: a running
 *      timer's CNT is never rewritten (that would glitch its pulse train).
 * [FA] نشان می‌دهد خروجی PWM هر کانال منطقی درحال‌چرخش است یا نه (فقط
 *      بازتاب Start/Stopهای همین ماژول). تراز فاز ۱۸۰ درجه فقط در گذر
 *      توقف→چرخش اعمال می‌شود؛ CNT تایمر درحال‌کار هرگز بازنویسی نمی‌شود
 *      چون قطار پالسش را glitch می‌کند. */
static bool BOOL__G__PwmRunning[BSP_PWM_CHANNEL_COUNT];

/* ==================== BspPwm_GetTimer ==================== */
/**
 * @brief  [EN] Resolve a logical channel to its board timer and channel.
 *         [FA] کانال منطقی را به تایمر و کانال فیزیکی برد تبدیل می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel /
 *                                     کانال منطقی
 * @param  TIM_HandleTypeDef__timer [EN] Output timer handle / هندل تایمر خروجی
 * @param  uint32_t__halChannel [EN] Output HAL channel identifier /
 *                                   شناسهٔ کانال HAL خروجی
 * @return bool [EN] true for a valid channel / برای کانال معتبر true
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

/* ==================== BspPwm_AlignPhaseToOther ==================== */
/**
 * @brief  [EN] Phase-lock the about-to-start timer half a period (180deg =
 *              10 us at 50 kHz) away from the other channel's timer, so the
 *              two flyback stages never switch simultaneously: the ON-pulse
 *              of one falls inside the OFF-time of the other. The shift is
 *              computed from the live ARR (period/2) on purpose, not a
 *              hardcoded 720, so a future period change follows automatically.
 *              +1/2 period and -1/2 period are the same offset, so which of
 *              the two timers leads is irrelevant. Both timers share the same
 *              72 MHz clock and identical ARR, hence the locked offset cannot
 *              drift. Counter read-then-set skew is ~100 ns (~0.5deg) and has
 *              no effect on the interleave. This helper applies ONLY on a
 *              stopped->running transition (see BOOL__G__PwmRunning) and only
 *              when the other channel is actually running; a timer that is
 *              already running keeps its pulse train untouched.
 *         [FA] تایمر در شرف استارت را نیم‌دوره (۱۸۰ درجه = ۱۰ میکروثانیه در
 *              ۵۰ کیلوهرتز) از تایمر کانال دیگر فاصله می‌دهد تا دو استیج
 *              فلای‌بک هرگز همزمان سوییچ نکنند: پالسِ روشنِ یکی داخل زمانِ
 *              خاموشِ دیگری می‌افتد. شیفت عمداً از ARR واقعی (نصف دوره)
 *              محاسبه می‌شود نه عدد ثابت ۷۲۰ تا با تغییر دوره خودکار دنبال
 *              شود. +نیم‌دوره و -نیم‌دوره یک شیفت‌اند، پس مهم نیست کدام تایمر
 *              جلو بیفتد. هر دو تایمر روی کلاک ۷۲ مگاهرتز و ARR یکسان‌اند پس
 *              آفست قفل‌شده رانش ندارد. خطای خواندن-و-نوشتن شمارنده حدود
 *              صد نانوثانیه است و اثری ندارد. این تابع فقط در گذر توقف→چرخش
 *              و فقط وقتی کانال دیگر واقعاً درحال‌چرخش است اعمال می‌شود؛
 *              تایمر درحال‌کار دست‌نخورده می‌ماند.
 * @param  bsp_pwm_channel_t__channel [EN] Channel being started /
 *                                     کانالی که استارت می‌شود
 * @param  TIM_HandleTypeDef__timer [EN] Its board timer / تایمر بردش
 */
static void func__BspPwm_AlignPhaseToOther(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                           TIM_HandleTypeDef *TIM_HandleTypeDef__timer)
{
    bsp_pwm_channel_t bsp_pwm_channel_t__other;
    TIM_HandleTypeDef *TIM_HandleTypeDef__otherTimer = NULL;
    uint32_t uint32_t__otherHalChannel = 0u;
    uint32_t uint32_t__periodCounts;
    uint32_t uint32_t__shiftedCounts;

    if (TIM_HandleTypeDef__timer == NULL)
    {
        return;
    }

    bsp_pwm_channel_t__other =
        (bsp_pwm_channel_t__channel == BSP_PWM_CHARGER_1) ? BSP_PWM_CHARGER_2
                                                          : BSP_PWM_CHARGER_1;

    if (BOOL__G__PwmRunning[bsp_pwm_channel_t__other] == false)
    {
        /* [EN] Nothing to lock against: first starter defines the phase.
           [FA] چیزی برای قفل‌شدن وجود ندارد: اولین استارت‌کننده فاز را تعریف می‌کند. */
        return;
    }

    if (func__BspPwm_GetTimer(bsp_pwm_channel_t__other,
                              &TIM_HandleTypeDef__otherTimer,
                              &uint32_t__otherHalChannel) == false)
    {
        return;
    }

    uint32_t__periodCounts = __HAL_TIM_GET_AUTORELOAD(TIM_HandleTypeDef__otherTimer) + 1u;
    uint32_t__shiftedCounts =
        (__HAL_TIM_GET_COUNTER(TIM_HandleTypeDef__otherTimer) +
         (uint32_t__periodCounts / 2u)) % uint32_t__periodCounts;
    __HAL_TIM_SET_COUNTER(TIM_HandleTypeDef__timer, uint32_t__shiftedCounts);
}

/* ==================== BspPwm_SetOneDuty ==================== */
/**
 * @brief  [EN] Apply one clamped duty to one initialized timer channel.
 *         [FA] وظیفهٔ محدودشدهٔ یک کانال تایمر مقداردهی‌شده را اعمال می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel / کانال منطقی
 * @param  TIM_HandleTypeDef__timer [EN] Board timer handle / هندل تایمر برد
 * @param  uint32_t__halChannel [EN] HAL channel / کانال HAL
 * @param  uint16_t__permille [EN] Duty in 0..1000 permille /
 *                                 وظیفه در بازهٔ ۰ تا ۱۰۰۰ پرمیل
 */
static void func__BspPwm_SetOneDuty(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                     TIM_HandleTypeDef *TIM_HandleTypeDef__timer,
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

    if (uint16_t__permille == 0u)
    {
        (void)HAL_TIM_PWM_Stop(TIM_HandleTypeDef__timer, uint32_t__halChannel);
        BOOL__G__PwmRunning[bsp_pwm_channel_t__channel] = false;
    }
    else
    {
        if (BOOL__G__PwmRunning[bsp_pwm_channel_t__channel] == false)
        {
            /* [EN] Stopped->running: interleave against the other timer
               BEFORE starting, so the first pulse already sits in the other
               stage's OFF-time.
               [FA] توقف→چرخش: قبل از استارت نسبت به تایمر دیگر درهم‌گذاری
               فاز انجام می‌شود تا از همان پالس اول، در زمانِ خاموشِ استیج
               دیگر قرار بگیرد. */
            func__BspPwm_AlignPhaseToOther(bsp_pwm_channel_t__channel,
                                           TIM_HandleTypeDef__timer);
        }
        (void)HAL_TIM_PWM_Start(TIM_HandleTypeDef__timer, uint32_t__halChannel);
        BOOL__G__PwmRunning[bsp_pwm_channel_t__channel] = true;
    }
}

/* ==================== BspPwm_Init ==================== */
/**
 * @brief  [EN] Force both schematic PWM outputs to a deterministic off state.
 *         [FA] هر دو خروجی PWM شماتیک را در وضعیت خاموش قطعی قرار می‌دهد.
 */
void func__BspPwm_Init(void)
{
    func__BspPwm_StopAll();
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
        func__BspPwm_SetOneDuty(bsp_pwm_channel_t__channel,
                                TIM_HandleTypeDef__timer,
                                uint32_t__halChannel,
                                uint16_t__permille);
    }
}

/* ==================== BspPwm_StopAll ==================== */
/**
 * @brief  [EN] Stop both PWM outputs and clear their compare values.
 *         [FA] هر دو خروجی PWM را متوقف و compare آن‌ها را صفر می‌کند.
 */
void func__BspPwm_StopAll(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);
    (void)HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    BOOL__G__PwmRunning[BSP_PWM_CHARGER_1] = false;
    BOOL__G__PwmRunning[BSP_PWM_CHARGER_2] = false;
}
