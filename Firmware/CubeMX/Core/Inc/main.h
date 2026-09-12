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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_I1_Pin GPIO_PIN_1
#define ADC_I1_GPIO_Port GPIOA
#define ADC_VIN_Pin GPIO_PIN_2
#define ADC_VIN_GPIO_Port GPIOA
#define ADC_BAT24_Pin GPIO_PIN_3
#define ADC_BAT24_GPIO_Port GPIOA
#define MCU_BUZZER_Pin GPIO_PIN_4
#define MCU_BUZZER_GPIO_Port GPIOA
#define ADC_BAT12_Pin GPIO_PIN_5
#define ADC_BAT12_GPIO_Port GPIOA
#define ADC_I2_Pin GPIO_PIN_7
#define ADC_I2_GPIO_Port GPIOA
#define MCU_R_LED_Pin GPIO_PIN_0
#define MCU_R_LED_GPIO_Port GPIOB
#define MCU_Y_LED_Pin GPIO_PIN_1
#define MCU_Y_LED_GPIO_Port GPIOB
#define MCU_G_LED_Pin GPIO_PIN_10
#define MCU_G_LED_GPIO_Port GPIOB
#define MCU_PROTECT_BATT_Pin GPIO_PIN_11
#define MCU_PROTECT_BATT_GPIO_Port GPIOB
#define MCU_ESP_CHPD_Pin GPIO_PIN_8
#define MCU_ESP_CHPD_GPIO_Port GPIOA
#define MCU_CONTROL_PS_Pin GPIO_PIN_5
#define MCU_CONTROL_PS_GPIO_Port GPIOB
#define MCU_RELAY_Pin GPIO_PIN_7
#define MCU_RELAY_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
