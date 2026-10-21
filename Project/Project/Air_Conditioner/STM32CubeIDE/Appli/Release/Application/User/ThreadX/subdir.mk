################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/ThreadX/App/tx_initialize_low_level.S 

OBJS += \
./Application/User/ThreadX/tx_initialize_low_level.o 

S_UPPER_DEPS += \
./Application/User/ThreadX/tx_initialize_low_level.d 


# Each subdirectory must supply rules for building sources it contributes
Application/User/ThreadX/tx_initialize_low_level.o: E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/ThreadX/App/tx_initialize_low_level.S Application/User/ThreadX/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m55 -DTX_SINGLE_MODE_SECURE -DTX_INCLUDE_USER_DEFINE_FILE -DTX_EXECUTION_PROFILE_ENABLE -c -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/ThreadX/App" -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Application-2f-User-2f-ThreadX

clean-Application-2f-User-2f-ThreadX:
	-$(RM) ./Application/User/ThreadX/tx_initialize_low_level.d ./Application/User/ThreadX/tx_initialize_low_level.o

.PHONY: clean-Application-2f-User-2f-ThreadX

