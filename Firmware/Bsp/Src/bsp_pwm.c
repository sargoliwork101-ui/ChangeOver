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
