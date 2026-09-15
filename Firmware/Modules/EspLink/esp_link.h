/**
 * @file    esp_link.h
 * @brief   [EN] ESP power and UART telemetry (placeholder). Full type naming, func__ prefix.
 *          [FA] تغذیه ESP و تله‌متری UART (اسکلت). نام تایپ کامل.
 */

#ifndef ESP_LINK_H
#define ESP_LINK_H

#include "app_types.h"

/**
 * @brief  [EN] Keep ESP powered off until that stage is enabled.
 *         [FA] ESP را خاموش نگه می‌دارد تا آن مرحله روشن شود.
 */
void func__EspLink_Init(void);

/**
 * @brief  [EN] Send telemetry. No STM command protocol yet.
 *         [FA] تله‌متری می‌فرستد. هنوز پروتکل فرمان به STM نیست.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  app_state_t__state [EN] System state / حالت
 * @param  fault_mask_t__faults [EN] Fault bits / بیت خطا
 */
void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap, app_state_t app_state_t__state, fault_mask_t fault_mask_t__faults);

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool__on [EN] true=on, false=off / روشن/خاموش
 */
void func__EspLink_Power(bool bool__on);

#endif /* ESP_LINK_H */
