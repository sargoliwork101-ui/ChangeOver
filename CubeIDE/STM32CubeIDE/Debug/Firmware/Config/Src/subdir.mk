################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Config/Src/app_config.c 

OBJS += \
./Firmware/Config/Src/app_config.o 

C_DEPS += \
./Firmware/Config/Src/app_config.d 


# Each subdirectory must supply rules for building sources it contributes
Firmware/Config/Src/app_config.o: C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Config/Src/app_config.c Firmware/Config/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../../Core/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Firmware-2f-Config-2f-Src

clean-Firmware-2f-Config-2f-Src:
	-$(RM) ./Firmware/Config/Src/app_config.cyclo ./Firmware/Config/Src/app_config.d ./Firmware/Config/Src/app_config.o ./Firmware/Config/Src/app_config.su

.PHONY: clean-Firmware-2f-Config-2f-Src

