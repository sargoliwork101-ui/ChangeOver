################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Changeover/changeover.c 

OBJS += \
./Firmware/Modules/Changeover/changeover.o 

C_DEPS += \
./Firmware/Modules/Changeover/changeover.d 


# Each subdirectory must supply rules for building sources it contributes
Firmware/Modules/Changeover/changeover.o: C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Changeover/changeover.c Firmware/Modules/Changeover/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../../Core/Inc -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/App/Inc" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Bsp/Inc" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Config/Inc" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Rtos/Inc" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Changeover" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Charger" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/EspLink" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Fault" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Jitter" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Measurement" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Protection" -I"C:/Users/hamed/OneDrive/Documents/GitHub/ChangeOver/Firmware/Modules/Ui" -I../../Drivers/STM32F1xx_HAL_Driver/Inc -I../../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Firmware-2f-Modules-2f-Changeover

clean-Firmware-2f-Modules-2f-Changeover:
	-$(RM) ./Firmware/Modules/Changeover/changeover.cyclo ./Firmware/Modules/Changeover/changeover.d ./Firmware/Modules/Changeover/changeover.o ./Firmware/Modules/Changeover/changeover.su

.PHONY: clean-Firmware-2f-Modules-2f-Changeover

