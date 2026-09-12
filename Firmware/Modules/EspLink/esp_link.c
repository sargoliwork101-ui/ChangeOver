#include "esp_link.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "board_pins.h"

void EspLink_Init(void)
{
    EspLink_Power(false);
}

void EspLink_Power(bool on)
{
    BspGpio_Write(PIN_ESP_CHPD_PORT, PIN_ESP_CHPD_PIN, on);
}

void EspLink_Run(const measurement_snapshot_t *snap, app_state_t state, fault_mask_t faults)
{
    (void)snap;
    (void)state;
    (void)faults;
    (void)APP_CONFIG;
    /* Step 9: send telemetry only. No STM commands until protocol is defined. */
}
