################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/Automation/STM32project/STM32N6/Air_Conditioner/STM32Cube_FW_N6_V1.0.0/Utilities/lcd/stm32_lcd.c 

OBJS += \
./Utilities/lcd/Src/stm32_lcd.o 

C_DEPS += \
./Utilities/lcd/Src/stm32_lcd.d 


# Each subdirectory must supply rules for building sources it contributes
Utilities/lcd/Src/stm32_lcd.o: E:/Automation/STM32project/STM32N6/Air_Conditioner/STM32Cube_FW_N6_V1.0.0/Utilities/lcd/stm32_lcd.c Utilities/lcd/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32N647xx -DBSP_UART_RX_DISABLE -DTX_SINGLE_MODE_SECURE -DTX_INCLUDE_USER_DEFINE_FILE -DTX_HAS_PARALLEL_NETWORKS=0 -DTX_EXECUTION_PROFILE_ENABLE -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_OSAL=LL_ATON_OSAL_THREADX -DLL_ATON_SW_FALLBACK -c -I../../../Appli/Core/Inc -I../../../Secure_nsclib -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/STM32N6xx_HAL_Driver/Inc -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/CMSIS/Include -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/NORFlash" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/HyperRAM" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/LED" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Drivers/BSP/LCD" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Drivers/BSP/BUS" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/UART" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/DHT11" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/AP3216C" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/REMOTE" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/KEY" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/SYS" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/TOUCH" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/LVGL/src" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/STM32Cube_FW_N6_V1.0.0/Drivers/CMSIS/DSP/Include" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/THREADX/ports/cortex_m55/gnu/inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/THREADX/common/inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/THREADX/utility/execution_profile_kit" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/ThreadX/App" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_ISP/isp/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_ISP/evision/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/STM32_MW_ISP" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_CAMERA" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_CAMERA/sensors" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_CAMERA/sensors/imx335" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/STM32_MW_CAMERA" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/AI/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/AI/Npu/ll_aton" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/AI/Npu/Devices/STM32N6XX" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_VISION_MODELS_PP/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/STM32Cube_FW_N6_V1.0.0/Utilities/lcd" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Model" -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Utilities-2f-lcd-2f-Src

clean-Utilities-2f-lcd-2f-Src:
	-$(RM) ./Utilities/lcd/Src/stm32_lcd.cyclo ./Utilities/lcd/Src/stm32_lcd.d ./Utilities/lcd/Src/stm32_lcd.o ./Utilities/lcd/Src/stm32_lcd.su

.PHONY: clean-Utilities-2f-lcd-2f-Src

