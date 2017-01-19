################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/audio_buffer.c \
../Src/cs43l22.c \
../Src/main.c \
../Src/stm32f4xx_hal_msp.c \
../Src/stm32f4xx_it.c \
../Src/usb_device.c \
../Src/usbd_audio_if.c \
../Src/usbd_conf.c \
../Src/usbd_desc.c 

OBJS += \
./Src/audio_buffer.o \
./Src/cs43l22.o \
./Src/main.o \
./Src/stm32f4xx_hal_msp.o \
./Src/stm32f4xx_it.o \
./Src/usb_device.o \
./Src/usbd_audio_if.o \
./Src/usbd_conf.o \
./Src/usbd_desc.o 

C_DEPS += \
./Src/audio_buffer.d \
./Src/cs43l22.d \
./Src/main.d \
./Src/stm32f4xx_hal_msp.d \
./Src/stm32f4xx_it.d \
./Src/usb_device.d \
./Src/usbd_audio_if.d \
./Src/usbd_conf.d \
./Src/usbd_desc.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o: ../Src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo %cd%
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -D__weak="__attribute__((weak))" -D__packed="__attribute__((__packed__))" -DUSE_HAL_DRIVER -DSTM32F407xx -I"D:/dev/STM32/usb_dac/Inc" -I"D:/dev/STM32/usb_dac/Drivers/STM32F4xx_HAL_Driver/Inc" -I"D:/dev/STM32/usb_dac/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy" -I"D:/dev/STM32/usb_dac/Middlewares/ST/STM32_USB_Device_Library/Core/Inc" -I"D:/dev/STM32/usb_dac/Middlewares/ST/STM32_USB_Device_Library/Class/AUDIO/Inc" -I"D:/dev/STM32/usb_dac/Drivers/CMSIS/Device/ST/STM32F4xx/Include" -I"D:/dev/STM32/usb_dac/Drivers/CMSIS/Include" -I"D:/dev/STM32/usb_dac/Inc"  -Og -g3 -Wall -fmessage-length=0 -ffunction-sections -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


