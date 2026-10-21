################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Application/User/Startup/startup_stm32n647x0hxq.s 

OBJS += \
./Application/User/Startup/startup_stm32n647x0hxq.o 

S_DEPS += \
./Application/User/Startup/startup_stm32n647x0hxq.d 


# Each subdirectory must supply rules for building sources it contributes
Application/User/Startup/%.o: ../Application/User/Startup/%.s Application/User/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m55 -DTX_SINGLE_MODE_SECURE -DTX_INCLUDE_USER_DEFINE_FILE -DTX_EXECUTION_PROFILE_ENABLE -c -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/ThreadX/App" -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Application-2f-User-2f-Startup

clean-Application-2f-User-2f-Startup:
	-$(RM) ./Application/User/Startup/startup_stm32n647x0hxq.d ./Application/User/Startup/startup_stm32n647x0hxq.o

.PHONY: clean-Application-2f-User-2f-Startup

