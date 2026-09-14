/**
 * @file    esp_link.c
 * @brief   [EN] ESP power and UART telemetry (placeholder).
 *          [FA] تغذیه ESP و تله‌متری UART (اسکلت).
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
void EspLink_Init(void)
{
    EspLink_Power(false);
}

/**
 * @brief  [EN] Drive CH_PD pin.
 *         [FA] پایه CH_PD را می‌زند.
 */
void EspLink_Power(bool on)
{
    BspGpio_Write(PIN_ESP_CHPD_PORT, PIN_ESP_CHPD_PIN, on);
}

/**
 * @brief  [EN] Send telemetry. No STM command protocol yet.
 *         [FA] تله‌متری می‌فرستد. هنوز پروتکل فرمان به STM نیست.
 */
void EspLink_Run(const measurement_snapshot_t *snap, app_state_t state, fault_mask_t faults)
{
    (void)snap;
    (void)state;
    (void)faults;
    (void)APP_CONFIG;
}
