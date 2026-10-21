################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/system_stm32n6xx_s.c 

OBJS += \
./Drivers/CMSIS/system_stm32n6xx_s.o 

C_DEPS += \
./Drivers/CMSIS/system_stm32n6xx_s.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/CMSIS/system_stm32n6xx_s.o: E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/system_stm32n6xx_s.c Drivers/CMSIS/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32N647xx -DBSP_UART_RX_DISABLE -DTX_SINGLE_MODE_SECURE -DTX_INCLUDE_USER_DEFINE_FILE -DTX_HAS_PARALLEL_NETWORKS=0 -DTX_EXECUTION_PROFILE_ENABLE -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_OSAL=LL_ATON_OSAL_THREADX -DLL_ATON_SW_FALLBACK -c -I../../../Appli/Core/Inc -I../../../Secure_nsclib -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/STM32N6xx_HAL_Driver/Inc -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../../../../../STM32Cube_FW_N6_V1.0.0/Drivers/CMSIS/Include -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/NORFlash" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/HyperRAM" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/LED" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Drivers/BSP/LCD" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Drivers/BSP/BUS" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/UART" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/DHT11" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/AP3216C" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/REMOTE" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/KEY" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/SYS" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers/BSP/TOUCH" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/LVGL/src" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/STM32Cube_FW_N6_V1.0.0/Drivers/CMSIS/DSP/Include" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/THREADX/ports/cortex_m55/gnu/inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/THREADX/common/inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/THREADX/utility/execution_profile_kit" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/ThreadX/App" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_ISP/isp/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_ISP/evision/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/STM32_MW_ISP" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_CAMERA" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_CAMERA/sensors" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_MW_CAMERA/sensors/imx335" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Appli/STM32_MW_CAMERA" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/AI/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/AI/Npu/ll_aton" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/AI/Npu/Devices/STM32N6XX" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Middlewares/STM32_VISION_MODELS_PP/Inc" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/STM32Cube_FW_N6_V1.0.0/Utilities/lcd" -I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Project/Project/Air_Conditioner/Model" -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-CMSIS

clean-Drivers-2f-CMSIS:
	-$(RM) ./Drivers/CMSIS/system_stm32n6xx_s.cyclo ./Drivers/CMSIS/system_stm32n6xx_s.d ./Drivers/CMSIS/system_stm32n6xx_s.o ./Drivers/CMSIS/system_stm32n6xx_s.su

.PHONY: clean-Drivers-2f-CMSIS

