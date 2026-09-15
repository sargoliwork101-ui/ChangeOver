/**
 * @file    esp_link.c
 * @brief   [EN] ESP power and UART telemetry (placeholder). Full type naming, func__ prefix.
 *          [FA] تغذیه ESP و تله‌متری UART (اسکلت). نام تایپ کامل و پیشوند func__.
 */

#include "esp_link.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "board_pins.h"

/**
 * @brief  [EN] Keep ESP powered off until that stage is enabled.
 *         [FA] ESP را خاموش نگه می‌دارد تا آن مرحله روشن شود.
 */
/* ==================== EspLink_Init ==================== */

void func__EspLink_Init(void)
{
    func__EspLink_Power(false);
}

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool__on [EN] true=ESP on (HIGH), false=off / روشن/خاموش
 */
/* ==================== EspLink_Power ==================== */

void func__EspLink_Power(bool bool__on)
{
    func__BspGpio_Write(PIN_ESP_CHPD_PORT, PIN_ESP_CHPD_PIN, bool__on);
}

/**
 * @brief  [EN] Send telemetry. No STM command protocol yet.
 *         [FA] تله‌متری می‌فرستد. هنوز پروتکل فرمان به STM نیست.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  app_state_t__state [EN] System state / حالت سیستم
 * @param  fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 */
/* ==================== EspLink_Run ==================== */

void func__EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t__snap, app_state_t app_state_t__state, fault_mask_t fault_mask_t__faults)
{
    (void)measurement_snapshot_t__snap;
    (void)app_state_t__state;
    (void)fault_mask_t__faults;
    (void)APP_CONFIG;
}