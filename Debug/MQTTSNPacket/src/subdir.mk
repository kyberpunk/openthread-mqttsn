################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../MQTTSNPacket/src/MQTTSNConnectClient.c \
../MQTTSNPacket/src/MQTTSNConnectServer.c \
../MQTTSNPacket/src/MQTTSNDeserializePublish.c \
../MQTTSNPacket/src/MQTTSNPacket.c \
../MQTTSNPacket/src/MQTTSNSearchClient.c \
../MQTTSNPacket/src/MQTTSNSearchServer.c \
../MQTTSNPacket/src/MQTTSNSerializePublish.c \
../MQTTSNPacket/src/MQTTSNSubscribeClient.c \
../MQTTSNPacket/src/MQTTSNSubscribeServer.c \
../MQTTSNPacket/src/MQTTSNUnsubscribeClient.c \
../MQTTSNPacket/src/MQTTSNUnsubscribeServer.c 

OBJS += \
./MQTTSNPacket/src/MQTTSNConnectClient.o \
./MQTTSNPacket/src/MQTTSNConnectServer.o \
./MQTTSNPacket/src/MQTTSNDeserializePublish.o \
./MQTTSNPacket/src/MQTTSNPacket.o \
./MQTTSNPacket/src/MQTTSNSearchClient.o \
./MQTTSNPacket/src/MQTTSNSearchServer.o \
./MQTTSNPacket/src/MQTTSNSerializePublish.o \
./MQTTSNPacket/src/MQTTSNSubscribeClient.o \
./MQTTSNPacket/src/MQTTSNSubscribeServer.o \
./MQTTSNPacket/src/MQTTSNUnsubscribeClient.o \
./MQTTSNPacket/src/MQTTSNUnsubscribeServer.o 

C_DEPS += \
./MQTTSNPacket/src/MQTTSNConnectClient.d \
./MQTTSNPacket/src/MQTTSNConnectServer.d \
./MQTTSNPacket/src/MQTTSNDeserializePublish.d \
./MQTTSNPacket/src/MQTTSNPacket.d \
./MQTTSNPacket/src/MQTTSNSearchClient.d \
./MQTTSNPacket/src/MQTTSNSearchServer.d \
./MQTTSNPacket/src/MQTTSNSerializePublish.d \
./MQTTSNPacket/src/MQTTSNSubscribeClient.d \
./MQTTSNPacket/src/MQTTSNSubscribeServer.d \
./MQTTSNPacket/src/MQTTSNUnsubscribeClient.d \
./MQTTSNPacket/src/MQTTSNUnsubscribeServer.d 


# Each subdirectory must supply rules for building sources it contributes
MQTTSNPacket/src/%.o: ../MQTTSNPacket/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCPU_MKW41Z512CAT4_cm0plus -DCPU_MKW41Z512CAT4 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_CONFIG_FILE='"openthread-config-generic.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/MQTTSNPacket/src" -O0 -fno-common -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


