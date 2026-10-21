################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Utilities/lcd/Src/subdir.mk
-include Model/subdir.mk
-include Middlewares/ThreadX/Ports/subdir.mk
-include Middlewares/ThreadX/Execution_Profile_Kit/subdir.mk
-include Middlewares/ThreadX/Common/subdir.mk
-include Middlewares/STM32_VISION_MODELS_PP/subdir.mk
-include Middlewares/STM32_MW_ISP/isp/subdir.mk
-include Middlewares/STM32_MW_CAMERA/sensors/imx335/subdir.mk
-include Middlewares/STM32_MW_CAMERA/sensors/subdir.mk
-include Middlewares/STM32_MW_CAMERA/STM32_MW_CAMERA/subdir.mk
-include Middlewares/AI/Npu/ll_aton/subdir.mk
-include Middlewares/AI/Npu/Devices/STM32N6XX/subdir.mk
-include Middlewares/LVGL/subdir.mk
-include Drivers/STM32N6xx_HAL_Driver/subdir.mk
-include Drivers/CMSIS/subdir.mk
-include Drivers/BSP/UART/subdir.mk
-include Drivers/BSP/TOUCH/subdir.mk
-include Drivers/BSP/SYS/subdir.mk
-include Drivers/BSP/REMOTE/subdir.mk
-include Drivers/BSP/NORFlash/subdir.mk
-include Drivers/BSP/LED/subdir.mk
-include Drivers/BSP/LCD/subdir.mk
-include Drivers/BSP/KEY/subdir.mk
-include Drivers/BSP/HyperRAM/subdir.mk
-include Drivers/BSP/DHT11/subdir.mk
-include Drivers/BSP/BUS/subdir.mk
-include Drivers/BSP/AP3216C/subdir.mk
-include Application/User/ThreadX/subdir.mk
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


BUILD_ARTIFACT_NAME := 991_AI_People_Detection_Appli
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
991_AI_People_Detection_Appli.elf \

MAP_FILES += \
991_AI_People_Detection_Appli.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
991_AI_People_Detection_Appli.list \

OBJCOPY_BIN += \
991_AI_People_Detection_Appli.bin \


# All Target
all:
	+@$(MAKE) --no-print-directory main-build && $(MAKE) --no-print-directory post-build

# Main-build Target
main-build: 991_AI_People_Detection_Appli.elf secondary-outputs

# Tool invocations
991_AI_People_Detection_Appli.elf 991_AI_People_Detection_Appli.map: $(OBJS) $(USER_OBJS) E:\Automation\STM32project\STM32N6\Air_Conditioner\Project\Project\Air_Conditioner\STM32CubeIDE\Appli\STM32N647X0HXQ_ROMxspi2_RAMxspi1.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "991_AI_People_Detection_Appli.elf" $(OBJS) $(USER_OBJS) $(LIBS) -mcpu=cortex-m55 -T"E:\Automation\STM32project\STM32N6\Air_Conditioner\Project\Project\Air_Conditioner\STM32CubeIDE\Appli\STM32N647X0HXQ_ROMxspi2_RAMxspi1.ld" --specs=nosys.specs -Wl,-Map="991_AI_People_Detection_Appli.map" -Wl,--gc-sections -static -Wl,--cmse-implib -Wl,--out-implib=./secure_nsclib.o -L"E:\Automation\STM32project\STM32N6\Air_Conditioner\Middlewares\STM32_MW_ISP\evision\Lib" -L"E:\Automation\STM32project\STM32N6\Air_Conditioner\Middlewares\AI\Lib\GCC\ARMCortexM55" -u _printf_float -Wl,--no-warn-rwx-segments --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

991_AI_People_Detection_Appli.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "991_AI_People_Detection_Appli.list"
	@echo 'Finished building: $@'
	@echo ' '

991_AI_People_Detection_Appli.bin: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objcopy  -O binary $(EXECUTABLES) "991_AI_People_Detection_Appli.bin"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) 991_AI_People_Detection_Appli.bin 991_AI_People_Detection_Appli.elf 991_AI_People_Detection_Appli.list 991_AI_People_Detection_Appli.map default.size.stdout
	-@echo ' '

post-build:
	bash "../postbuild.sh" "E:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin"
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(OBJCOPY_BIN)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified post-build

-include ../makefile.targets
