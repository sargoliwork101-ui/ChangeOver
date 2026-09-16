/**
 * @file    bsp_pwm.c
 * @brief   [EN] Board PWM port placeholder.
 *          [FA] اسکلت پورت PWM برد.
 *
 * @note    [EN] The active CubeMX configuration does not enable charger PWM.
 *              A future board implementation may include its HAL timer headers
 *              here, without changing the public interface or Charger module.
 *          [FA] پیکربندی فعال CubeMX، PWM شارژر را روشن نکرده است. در پورت
 *              برد آینده می‌توان headerهای HAL تایمر را فقط اینجا آورد، بدون
 *              تغییر رابط عمومی یا ماژول Charger.
 */

#include "bsp_pwm.h"

/**
 * @brief  [EN] Initialize the board PWM backend and force outputs off.
 *         [FA] Backend PWM برد را مقداردهی و خروجی‌ها را خاموش می‌کند.
 */
/* ==================== BspPwm_Init ==================== */
void func__BspPwm_Init(void)
{
    func__BspPwm_StopAll();
}

/**
 * @brief  [EN] Set logical charger duty. This stage is intentionally a no-op.
 *         [FA] وظیفهٔ منطقی شارژر را تنظیم می‌کند؛ این مرحله عمداً بدون عمل است.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel / کانال منطقی
 * @param  uint16_t__permille [EN] Duty in permille / وظیفه بر حسب پرمیل
 */
/* ==================== BspPwm_SetDutyPermille ==================== */
void func__BspPwm_SetDutyPermille(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   uint16_t uint16_t__permille)
{
    (void)bsp_pwm_channel_t__channel;
    (void)uint16_t__permille;
}

/**
 * @brief  [EN] Force every board PWM output off.
 *         [FA] همهٔ خروجی‌های PWM برد را خاموش می‌کند.
 */
/* ==================== BspPwm_StopAll ==================== */
void func__BspPwm_StopAll(void)
{
}
