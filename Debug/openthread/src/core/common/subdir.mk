################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../openthread/src/core/common/crc16.cpp \
../openthread/src/core/common/instance.cpp \
../openthread/src/core/common/locator.cpp \
../openthread/src/core/common/logging.cpp \
../openthread/src/core/common/message.cpp \
../openthread/src/core/common/notifier.cpp \
../openthread/src/core/common/settings.cpp \
../openthread/src/core/common/string.cpp \
../openthread/src/core/common/tasklet.cpp \
../openthread/src/core/common/timer.cpp \
../openthread/src/core/common/tlvs.cpp \
../openthread/src/core/common/trickle_timer.cpp 

OBJS += \
./openthread/src/core/common/crc16.o \
./openthread/src/core/common/instance.o \
./openthread/src/core/common/locator.o \
./openthread/src/core/common/logging.o \
./openthread/src/core/common/message.o \
./openthread/src/core/common/notifier.o \
./openthread/src/core/common/settings.o \
./openthread/src/core/common/string.o \
./openthread/src/core/common/tasklet.o \
./openthread/src/core/common/timer.o \
./openthread/src/core/common/tlvs.o \
./openthread/src/core/common/trickle_timer.o 

CPP_DEPS += \
./openthread/src/core/common/crc16.d \
./openthread/src/core/common/instance.d \
./openthread/src/core/common/locator.d \
./openthread/src/core/common/logging.d \
./openthread/src/core/common/message.d \
./openthread/src/core/common/notifier.d \
./openthread/src/core/common/settings.d \
./openthread/src/core/common/string.d \
./openthread/src/core/common/tasklet.d \
./openthread/src/core/common/timer.d \
./openthread/src/core/common/tlvs.d \
./openthread/src/core/common/trickle_timer.d 


# Each subdirectory must supply rules for building sources it contributes
openthread/src/core/common/%.o: ../openthread/src/core/common/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C++ Compiler'
	arm-none-eabi-c++ -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/paho/MQTTSNPacket/src" -O1 -fno-common -g3 -Wall -c -fmessage-length=0 -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


