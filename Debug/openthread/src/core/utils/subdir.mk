################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../openthread/src/core/utils/channel_manager.cpp \
../openthread/src/core/utils/channel_monitor.cpp \
../openthread/src/core/utils/child_supervision.cpp \
../openthread/src/core/utils/heap.cpp \
../openthread/src/core/utils/jam_detector.cpp \
../openthread/src/core/utils/parse_cmdline.cpp \
../openthread/src/core/utils/slaac_address.cpp 

C_SRCS += \
../openthread/src/core/utils/missing_strlcat.c \
../openthread/src/core/utils/missing_strlcpy.c \
../openthread/src/core/utils/missing_strnlen.c 

OBJS += \
./openthread/src/core/utils/channel_manager.o \
./openthread/src/core/utils/channel_monitor.o \
./openthread/src/core/utils/child_supervision.o \
./openthread/src/core/utils/heap.o \
./openthread/src/core/utils/jam_detector.o \
./openthread/src/core/utils/missing_strlcat.o \
./openthread/src/core/utils/missing_strlcpy.o \
./openthread/src/core/utils/missing_strnlen.o \
./openthread/src/core/utils/parse_cmdline.o \
./openthread/src/core/utils/slaac_address.o 

CPP_DEPS += \
./openthread/src/core/utils/channel_manager.d \
./openthread/src/core/utils/channel_monitor.d \
./openthread/src/core/utils/child_supervision.d \
./openthread/src/core/utils/heap.d \
./openthread/src/core/utils/jam_detector.d \
./openthread/src/core/utils/parse_cmdline.d \
./openthread/src/core/utils/slaac_address.d 

C_DEPS += \
./openthread/src/core/utils/missing_strlcat.d \
./openthread/src/core/utils/missing_strlcpy.d \
./openthread/src/core/utils/missing_strnlen.d 


# Each subdirectory must supply rules for building sources it contributes
openthread/src/core/utils/%.o: ../openthread/src/core/utils/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C++ Compiler'
	arm-none-eabi-c++ -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/paho/MQTTSNPacket/src" -Og -fno-common -g3 -Wall -c -fmessage-length=0 -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

openthread/src/core/utils/%.o: ../openthread/src/core/utils/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCPU_MKW41Z512CAT4_cm0plus -DCPU_MKW41Z512CAT4 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/paho/MQTTSNPacket/src" -Og -fno-common -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


