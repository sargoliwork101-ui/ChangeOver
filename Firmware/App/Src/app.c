#include "app.h"
#include "modules_enable.h"
#include "actuator.h"
#include "rtos_app.h"
#include "bsp_exti.h"
#include "stm32f1xx_hal.h"

#if MODULE_UI
#include "ui.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_MEASUREMENT
#include "measurement.h"
#include "bsp_adc.h"
extern ADC_HandleTypeDef hadc1;
#endif
#if MODULE_PROTECTION
#include "protection.h"
#endif
#if MODULE_CHANGEOVER
#include "changeover.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#include "bsp_pwm.h"
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
#endif
#if MODULE_JITTER
#include "jitter.h"
#endif
#if MODULE_ESP
#include "esp_link.h"
#include "bsp_uart.h"
extern UART_HandleTypeDef huart1;
#endif

void App_Init(void)
{
    BspExti_Init();

#if MODULE_CHARGER
    BspPwm_Init(&htim2, &htim3);
#endif
#if MODULE_ESP
    BspUart_Init(&huart1);
#endif

    /* Always first: power pins in a defined state */
    Actuator_Init();

#if MODULE_FAULT
    Fault_Init();
#endif
#if MODULE_UI
    Ui_Init();
#endif
#if MODULE_MEASUREMENT
    BspAdc_Init(&hadc1);
    Measurement_Init();
#endif
#if MODULE_PROTECTION
    Protection_Init();
#endif
#if MODULE_CHANGEOVER
    Changeover_Init();
#endif
#if MODULE_CHARGER
    Charger_Init();
#endif
#if MODULE_JITTER
    Jitter_Init();
#endif
#if MODULE_ESP
    EspLink_Init();
#endif
}

void App_Start(void)
{
    App_Init();
    Rtos_Start();
}
