/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported variables ------------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCU_BUZZER_Pin GPIO_PIN_4
#define MCU_BUZZER_GPIO_Port GPIOA
#define MCU_R_LED_Pin GPIO_PIN_0
#define MCU_R_LED_GPIO_Port GPIOB
#define MCU_Y_LED_Pin GPIO_PIN_1
#define MCU_Y_LED_GPIO_Port GPIOB
#define MCU_G_LED_Pin GPIO_PIN_10
#define MCU_G_LED_GPIO_Port GPIOB
#define MCU_ESP_CHPD_Pin GPIO_PIN_8
#define MCU_ESP_CHPD_GPIO_Port GPIOA
#define MCU_BAT_SWITCH_Pin GPIO_PIN_5
#define MCU_BAT_SWITCH_GPIO_Port GPIOB
#define MCU_PROTECT_CHARGER_Pin GPIO_PIN_7
#define MCU_PROTECT_CHARGER_GPIO_Port GPIOB
#define MCU_PROTECT_BATT_Pin GPIO_PIN_11
#define MCU_PROTECT_BATT_GPIO_Port GPIOB
#define MCU_JITTER1_Pin GPIO_PIN_2
#define MCU_JITTER1_GPIO_Port GPIOB
#define MCU_JITTER2_Pin GPIO_PIN_6
#define MCU_JITTER2_GPIO_Port GPIOB
#define MCU_INT_24_IN_Pin GPIO_PIN_4
#define MCU_INT_24_IN_GPIO_Port GPIOB
#define MCU_PWM1_Pin GPIO_PIN_0
#define MCU_PWM1_GPIO_Port GPIOA
#define MCU_PWM2_Pin GPIO_PIN_6
#define MCU_PWM2_GPIO_Port GPIOA
#define MCU_TX_Pin GPIO_PIN_9
#define MCU_TX_GPIO_Port GPIOA
#define MCU_RX_Pin GPIO_PIN_10
#define MCU_RX_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
