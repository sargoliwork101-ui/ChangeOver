/**
 * @file    bsp_pwm.c
 * @brief   [EN] PWM wrapper for charger channels (placeholder). Full type naming, func_ prefix.
 *          [FA] پوشش PWM کانال شارژر (اسکلت). نام تایپ کامل.
 */

#include "bsp_pwm.h"
#include <stddef.h>

static TIM_HandleTypeDef *TIM_HANDLETYPEDEF_G_Tim1 = NULL;
static TIM_HandleTypeDef *TIM_HANDLETYPEDEF_G_Tim2 = NULL;

/**
 * @brief  [EN] Store timer handles. No PWM output until later stage.
 *         [FA] هندل تایمر را نگه می‌دارد. خروجی PWM هنوز نیست.
 * @param  TIM_HandleTypeDef_htimCh1 [EN] Timer ch1 / تایمر کانال ۱
 * @param  TIM_HandleTypeDef_htimCh2 [EN] Timer ch2 / تایمر کانال ۲
 */
void func_BspPwm_Init(TIM_HandleTypeDef *TIM_HandleTypeDef_htimCh1, TIM_HandleTypeDef *TIM_HandleTypeDef_htimCh2)
{
    TIM_HANDLETYPEDEF_G_Tim1 = TIM_HandleTypeDef_htimCh1;
    TIM_HANDLETYPEDEF_G_Tim2 = TIM_HandleTypeDef_htimCh2;
    func_BspPwm_StopAll();
}

/**
 * @brief  [EN] Set duty in permille (0..1000). No-op until implemented.
 *         [FA] وظیفه را به پرمیل می‌گذارد.
 * @param  uint8_t_channel [EN] 1 or 2 / کانال
 * @param  uint16_t_permille [EN] 0..1000 / پرمیل
 */
void func_BspPwm_SetDutyPermille(uint8_t uint8_t_channel, uint16_t uint16_t_permille)
{
    (void)uint8_t_channel;
    (void)uint16_t_permille;
    (void)TIM_HANDLETYPEDEF_G_Tim1;
    (void)TIM_HANDLETYPEDEF_G_Tim2;
}

/**
 * @brief  [EN] Force both PWM channels off.
 *         [FA] هر دو کانال PWM را خاموش می‌کند.
 */
void func_BspPwm_StopAll(void)
{
}

