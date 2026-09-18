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
#ifndef CHG_PWM_FREQ_HZ
#define CHG_PWM_FREQ_HZ 50000u
#define CHG_TIMER_CLOCK_HZ 72000000u
#endif

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

/* ==================== BspPwm_SetOneDuty ==================== */
/**
 * @brief  [EN] Apply one clamped duty to one initialized timer channel.
 *         [FA] وظیفهٔ محدودشدهٔ یک کانال تایمر مقداردهی‌شده را اعمال می‌کند.
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

    if (uint16_t__permille == 0u)
    {
        (void)HAL_TIM_PWM_Stop(TIM_HandleTypeDef__timer, uint32_t__halChannel);
    }
    else
    {
        (void)HAL_TIM_PWM_Start(TIM_HandleTypeDef__timer, uint32_t__halChannel);
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
        func__BspPwm_SetOneDuty(TIM_HandleTypeDef__timer,
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
}

/* ==================== BspPwm_TripOffFromIsr ==================== */
/**
 * @brief  [EN] ISR-safe trip: zero CCR, disable CCxE, no HAL, no RTOS.
 *         [FA] تریپ ISR-safe: صفر CCR، قطع خروجی.
 */
void func__BspPwm_TripOffFromIsr(void)
{
    /* [EN] Very short, no RTOS API, no blocking, latch already in Charger.
       Direct register writes are ISR-safe; HAL_TIM_PWM_Stop is NOT assumed safe.
       [FA] بسیار کوتاه، بدون RTOS. */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);

    /* [EN] Clear CC1E to disable output quickly.
       [FA] قطع خروجی. */
    htim2.Instance->CCER &= (uint32_t)(~TIM_CCER_CC1E);
    htim3.Instance->CCER &= (uint32_t)(~TIM_CCER_CC1E);

    /* [EN] For advanced timers BDTR MOE would be cleared, but TIM2/3 are general purpose.
       [FA] برای TIM2/3 نیازی به MOE نیست. */
}

/* ==================== BspPwm_GetFrequency ==================== */
uint32_t func__BspPwm_GetFrequencyHz(void)
{
    uint32_t uint32_t__psc;
    uint32_t uint32_t__arr;

    /* [EN] Read actual timer registers; after fix PSC=0 ARR=1439 → 50kHz.
       [FA] خواندن رجیستر واقعی. */
    uint32_t__psc = (uint32_t)htim2.Instance->PSC;
    uint32_t__arr = (uint32_t)htim2.Instance->ARR;

    if ((uint32_t__psc == 0u) && (uint32_t__arr == 0u))
    {
        /* [EN] Not yet initialized (host), return spec value.
           [FA] اگر هنوز Init نشده، مقدار spec. */
        return CHG_PWM_FREQ_HZ;
    }

    /* [EN] TIM clock 72MHz, but APB1 timers x2 when APB1 prescaler 2.
       SystemClock sets APB1 div2, TIM2/3 on APB1 → timer clock 72MHz.
       Formula: 72M/(PSC+1)/(ARR+1).
       [FA] فرمول فرکانس. */
    return CHG_TIMER_CLOCK_HZ / (uint32_t__psc + 1u) / (uint32_t__arr + 1u);
}
