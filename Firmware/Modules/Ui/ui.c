#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"

static app_state_t s_state = APP_STATE_BOOT;
static fault_mask_t s_faults = FAULT_NONE;
static uint8_t s_tick = 0u;

void Ui_Init(void)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, false);
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, false);
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, false);
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
    s_state = APP_STATE_BOOT;
    s_faults = FAULT_NONE;
    s_tick = 0u;
}

void Ui_Show(app_state_t state, fault_mask_t faults)
{
    s_state = state;
    s_faults = faults;
}

void Ui_Run(void)
{
    bool green;
    bool red;

    s_tick = (uint8_t)(s_tick + 1u);

    red = (s_faults != FAULT_NONE) || (s_state == APP_STATE_FAULT);
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, red);

    /* Step 1 heartbeat: green blinks so we know RTOS is running */
    green = ((s_tick & 0x04u) != 0u);
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, green);

    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, false);
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);

    (void)s_state;
}
