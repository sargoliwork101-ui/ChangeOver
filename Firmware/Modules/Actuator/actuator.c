#include "actuator.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "board_pins.h"

static void write_safe_gpio(void)
{
    /* Relay off: schematic active high */
    BspGpio_Write(PIN_RELAY_PORT, PIN_RELAY_PIN, false);

    /* Keep control PSU on battery at boot: BAT_SWITCH low */
    BspGpio_Write(PIN_BAT_SWITCH_PORT, PIN_BAT_SWITCH_PIN, false);

    /* Do not assert PROTECT_BATT until polarity is MEASURED.
     * Hardware already has analog changeover. Leaving low = no MCU force-off. */
    BspGpio_Write(PIN_PROTECT_BATT_PORT, PIN_PROTECT_BATT_PIN, false);

    BspGpio_Write(PIN_ESP_CHPD_PORT, PIN_ESP_CHPD_PIN, false);
}

void Actuator_Init(void)
{
    write_safe_gpio();
    BspPwm_StopAll();
}

void Actuator_EnterSafeState(void)
{
    BspPwm_StopAll();
    BspGpio_Write(PIN_RELAY_PORT, PIN_RELAY_PIN, false);
    /* load-path and PSU policy for fault is filled in step 4 */
}

void Actuator_RequestRelay(bool on)
{
    if (!APP_CONFIG.power_stage_enabled)
    {
        on = false;
    }
    BspGpio_Write(PIN_RELAY_PORT, PIN_RELAY_PIN, on);
}

void Actuator_RequestBatSwitchDisconnect(bool disconnect_psu_from_battery)
{
    /* HIGH disconnects battery from control PSU (schematic) */
    BspGpio_Write(PIN_BAT_SWITCH_PORT, PIN_BAT_SWITCH_PIN, disconnect_psu_from_battery);
}

void Actuator_RequestProtectBatt(bool force_battery_path_off)
{
    if (!APP_CONFIG.power_stage_enabled)
    {
        return;
    }
    BspGpio_Write(PIN_PROTECT_BATT_PORT, PIN_PROTECT_BATT_PIN, force_battery_path_off);
}

void Actuator_RequestPwm1Permille(uint16_t permille)
{
    uint16_t duty = 0u;

    if (APP_CONFIG.power_stage_enabled)
    {
        duty = permille;
        if (duty > APP_CONFIG.pwm_max_duty_permille)
        {
            duty = APP_CONFIG.pwm_max_duty_permille;
        }
    }
    BspPwm_SetDutyPermille(1u, duty);
}

void Actuator_RequestPwm2Permille(uint16_t permille)
{
    uint16_t duty = 0u;

    if (APP_CONFIG.power_stage_enabled)
    {
        duty = permille;
        if (duty > APP_CONFIG.pwm_max_duty_permille)
        {
            duty = APP_CONFIG.pwm_max_duty_permille;
        }
    }
    BspPwm_SetDutyPermille(2u, duty);
}
