/**
 * @file    bsp_pwm.c
 * @brief   [EN] PWM wrapper for charger channels (placeholder). Full type naming, func__ prefix.
 *          [FA] پوشش PWM کانال شارژر (اسکلت). نام تایپ کامل.
 */

#include "bsp_pwm.h"
#include <stddef.h>

static TIM_HandleTypeDef *TIM_HANDLETYPEDEF__G__Tim1 = NULL;
static TIM_HandleTypeDef *TIM_HANDLETYPEDEF__G__Tim2 = NULL;

/**
 * @brief  [EN] Store timer handles. No PWM output until later stage.
 *         [FA] هندل تایمر را نگه می‌دارد. خروجی PWM هنوز نیست.
 * @param  TIM_HandleTypeDef__htimCh1 [EN] Timer ch1 / تایمر کانال ۱
 * @param  TIM_HandleTypeDef__htimCh2 [EN] Timer ch2 / تایمر کانال ۲
 */
void func__BspPwm_Init(TIM_HandleTypeDef *TIM_HandleTypeDef__htimCh1, TIM_HandleTypeDef *TIM_HandleTypeDef__htimCh2)
{
    TIM_HANDLETYPEDEF__G__Tim1 = TIM_HandleTypeDef__htimCh1;
    TIM_HANDLETYPEDEF__G__Tim2 = TIM_HandleTypeDef__htimCh2;
    func__BspPwm_StopAll();
}

/**
 * @brief  [EN] Set duty in permille (0..1000). No-op until implemented.
 *         [FA] وظیفه را به پرمیل می‌گذارد.
 * @param  uint8_t__channel [EN] 1 or 2 / کانال
 * @param  uint16_t__permille [EN] 0..1000 / پرمیل
 */
void func__BspPwm_SetDutyPermille(uint8_t uint8_t__channel, uint16_t uint16_t__permille)
{
    (void)uint8_t__channel;
    (void)uint16_t__permille;
    (void)TIM_HANDLETYPEDEF__G__Tim1;
    (void)TIM_HANDLETYPEDEF__G__Tim2;
}

/**
 * @brief  [EN] Force both PWM channels off.
 *         [FA] هر دو کانال PWM را خاموش می‌کند.
 */
void func__BspPwm_StopAll(void)
{
}

