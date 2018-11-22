################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../openthread/examples/platforms/kw41z/alarm.c \
../openthread/examples/platforms/kw41z/diag.c \
../openthread/examples/platforms/kw41z/flash.c \
../openthread/examples/platforms/kw41z/logging.c \
../openthread/examples/platforms/kw41z/misc.c \
../openthread/examples/platforms/kw41z/radio.c \
../openthread/examples/platforms/kw41z/random.c \
../openthread/examples/platforms/kw41z/system.c \
../openthread/examples/platforms/kw41z/uart.c 

OBJS += \
./openthread/examples/platforms/kw41z/alarm.o \
./openthread/examples/platforms/kw41z/diag.o \
./openthread/examples/platforms/kw41z/flash.o \
./openthread/examples/platforms/kw41z/logging.o \
./openthread/examples/platforms/kw41z/misc.o \
./openthread/examples/platforms/kw41z/radio.o \
./openthread/examples/platforms/kw41z/random.o \
./openthread/examples/platforms/kw41z/system.o \
./openthread/examples/platforms/kw41z/uart.o 

C_DEPS += \
./openthread/examples/platforms/kw41z/alarm.d \
./openthread/examples/platforms/kw41z/diag.d \
./openthread/examples/platforms/kw41z/flash.d \
./openthread/examples/platforms/kw41z/logging.d \
./openthread/examples/platforms/kw41z/misc.d \
./openthread/examples/platforms/kw41z/radio.d \
./openthread/examples/platforms/kw41z/random.d \
./openthread/examples/platforms/kw41z/system.d \
./openthread/examples/platforms/kw41z/uart.d 


# Each subdirectory must supply rules for building sources it contributes
openthread/examples/platforms/kw41z/%.o: ../openthread/examples/platforms/kw41z/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCPU_MKW41Z512CAT4_cm0plus -DCPU_MKW41Z512CAT4 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_CONFIG_FILE='"openthread-config-generic.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/paho/MQTTSNPacket/src" -O0 -fno-common -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


