################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/App/Src/app.c 

OBJS += \
./Firmware/App/Src/app.o 

C_DEPS += \
./Firmware/App/Src/app.d 


# Each subdirectory must supply rules for building sources it contributes
Firmware/App/Src/app.o: C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/App/Src/app.c Firmware/App/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../../Core/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Firmware-2f-App-2f-Src

clean-Firmware-2f-App-2f-Src:
	-$(RM) ./Firmware/App/Src/app.cyclo ./Firmware/App/Src/app.d ./Firmware/App/Src/app.o ./Firmware/App/Src/app.su

.PHONY: clean-Firmware-2f-App-2f-Src

