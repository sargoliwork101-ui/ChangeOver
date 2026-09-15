/**
 * @file    esp_link.c
 * @brief   [EN] ESP power and UART telemetry (placeholder). Full type naming, func_ prefix.
 *          [FA] تغذیه ESP و تله‌متری UART (اسکلت). نام تایپ کامل و پیشوند func_.
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
void func_EspLink_Init(void)
{
    func_EspLink_Power(false);
}

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 * @param  bool_on [EN] true=ESP on (HIGH), false=off / روشن/خاموش
 */
void func_EspLink_Power(bool bool_on)
{
    func_BspGpio_Write(PIN_ESP_CHPD_PORT, PIN_ESP_CHPD_PIN, bool_on);
}

/**
 * @brief  [EN] Send telemetry. No STM command protocol yet.
 *         [FA] تله‌متری می‌فرستد. هنوز پروتکل فرمان به STM نیست.
 * @param  measurement_snapshot_t_snap [EN] Snapshot / نمونه
 * @param  app_state_t_state [EN] System state / حالت سیستم
 * @param  fault_mask_t_faults [EN] Fault bits / بیت‌های خطا
 */
void func_EspLink_Run(const measurement_snapshot_t *measurement_snapshot_t_snap, app_state_t app_state_t_state, fault_mask_t fault_mask_t_faults)
{
    (void)measurement_snapshot_t_snap;
    (void)app_state_t_state;
    (void)fault_mask_t_faults;
    (void)APP_CONFIG;
}
