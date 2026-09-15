/**
 * @file    esp_link.h
 * @brief   [EN] ESP power and UART telemetry (placeholder). Full type naming, func_ prefix.
 *          [FA] تغذیه ESP و تله‌متری UART (اسکلت). نام تایپ کامل.
 */

#ifndef ESP_LINK_H
#define ESP_LINK_H

#include "app_types.h"

/**
 * @brief  [EN] Keep ESP powered off until that stage is enabled.
 *         [FA] ESP را خاموش نگه می‌دارد تا آن مرحله روشن شود.
 */
void func_EspLink_Init(void);

/**
 * @brief  [EN] Send telemetry. No STM command protocol yet.
 *         [FA] تله‌متری می‌فرستد. هنوز پروتکل فرمان به STM نیست.
 * @param  measurement_snapshot_t_snap [EN] Snapshot / نمونه
 * @param  app_state_t_state [EN] System state / حالت
 * @param  fault_mask_t_faults [EN] Fault bits / بیت خطا
 */
void func_EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t_snap, app_state_t app_state_t_state, fault_mask_t fault_mask_t_faults);

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool_on [EN] true=on, false=off / روشن/خاموش
 */
void func_EspLink_Power(bool bool_on);

#endif /* ESP_LINK_H */
