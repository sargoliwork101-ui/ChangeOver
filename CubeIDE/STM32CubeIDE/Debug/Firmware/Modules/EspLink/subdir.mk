################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/EspLink/esp_link.c 

OBJS += \
./Firmware/Modules/EspLink/esp_link.o 

C_DEPS += \
./Firmware/Modules/EspLink/esp_link.d 


# Each subdirectory must supply rules for building sources it contributes
Firmware/Modules/EspLink/esp_link.o: C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/EspLink/esp_link.c Firmware/Modules/EspLink/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../../Core/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Firmware-2f-Modules-2f-EspLink

clean-Firmware-2f-Modules-2f-EspLink:
	-$(RM) ./Firmware/Modules/EspLink/esp_link.cyclo ./Firmware/Modules/EspLink/esp_link.d ./Firmware/Modules/EspLink/esp_link.o ./Firmware/Modules/EspLink/esp_link.su

.PHONY: clean-Firmware-2f-Modules-2f-EspLink

