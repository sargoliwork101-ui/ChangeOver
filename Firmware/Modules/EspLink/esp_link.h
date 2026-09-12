/**
 * @file    esp_link.h
 * @brief   [EN] ESP power and UART telemetry (placeholder).
 *          [FA] تغذیه ESP و تله‌متری UART (اسکلت).
 *
 * @stage   Placeholder
 */

#ifndef ESP_LINK_H
#define ESP_LINK_H

#include "app_types.h"

/**
 * @brief  [EN] Keep ESP powered off until that stage is enabled.
 *         [FA] ESP را خاموش نگه می‌دارد تا آن مرحله روشن شود.
 */
void EspLink_Init(void);

/**
 * @brief  [EN] Send telemetry. No STM command protocol yet.
 *         [FA] تله‌متری می‌فرستد. هنوز پروتکل فرمان به STM نیست.
 */
void EspLink_Run(const measurement_snapshot_t *snap, app_state_t state, fault_mask_t faults);

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 */
void EspLink_Power(bool on);

#endif /* ESP_LINK_H */
