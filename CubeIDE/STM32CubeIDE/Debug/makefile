################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Middlewares/FreeRTOS/subdir.mk
-include Firmware/Rtos/Src/subdir.mk
-include Firmware/Modules/Ui/subdir.mk
-include Firmware/Modules/Protection/subdir.mk
-include Firmware/Modules/Measurement/subdir.mk
-include Firmware/Modules/Jitter/subdir.mk
-include Firmware/Modules/Fault/subdir.mk
-include Firmware/Modules/EspLink/subdir.mk
-include Firmware/Modules/Charger/subdir.mk
-include Firmware/Modules/Changeover/subdir.mk
-include Firmware/Config/Src/subdir.mk
-include Firmware/Bsp/Src/subdir.mk
-include Firmware/App/Src/subdir.mk
-include Drivers/STM32F1xx_HAL_Driver/subdir.mk
-include Drivers/CMSIS/subdir.mk
-include Application/User/Startup/subdir.mk
-include Application/User/Core/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := CubeIDE
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
CubeIDE.elf \

MAP_FILES += \
CubeIDE.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
CubeIDE.list \


# All Target
all: main-build

# Main-build Target
main-build: CubeIDE.elf secondary-outputs

# Tool invocations
CubeIDE.elf CubeIDE.map: $(OBJS) $(USER_OBJS) C:\Users\hamed\OneDrive\Documents\GitHub\ChangeOver\CubeIDE\STM32CubeIDE\STM32F103C8TX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "CubeIDE.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m3 -T"C:\Users\hamed\OneDrive\Documents\GitHub\ChangeOver\CubeIDE\STM32CubeIDE\STM32F103C8TX_FLASH.ld" --specs=nosys.specs -Wl,-Map="CubeIDE.map" -Wl,--gc-sections -static --specs=nano.specs -mfloat-abi=soft -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

CubeIDE.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "CubeIDE.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) CubeIDE.elf CubeIDE.list CubeIDE.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
