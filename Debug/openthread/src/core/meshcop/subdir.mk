################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../openthread/src/core/meshcop/announce_begin_client.cpp \
../openthread/src/core/meshcop/border_agent.cpp \
../openthread/src/core/meshcop/commissioner.cpp \
../openthread/src/core/meshcop/dataset.cpp \
../openthread/src/core/meshcop/dataset_local.cpp \
../openthread/src/core/meshcop/dataset_manager.cpp \
../openthread/src/core/meshcop/dataset_manager_ftd.cpp \
../openthread/src/core/meshcop/dtls.cpp \
../openthread/src/core/meshcop/energy_scan_client.cpp \
../openthread/src/core/meshcop/joiner.cpp \
../openthread/src/core/meshcop/joiner_router.cpp \
../openthread/src/core/meshcop/leader.cpp \
../openthread/src/core/meshcop/meshcop.cpp \
../openthread/src/core/meshcop/meshcop_tlvs.cpp \
../openthread/src/core/meshcop/panid_query_client.cpp \
../openthread/src/core/meshcop/timestamp.cpp 

OBJS += \
./openthread/src/core/meshcop/announce_begin_client.o \
./openthread/src/core/meshcop/border_agent.o \
./openthread/src/core/meshcop/commissioner.o \
./openthread/src/core/meshcop/dataset.o \
./openthread/src/core/meshcop/dataset_local.o \
./openthread/src/core/meshcop/dataset_manager.o \
./openthread/src/core/meshcop/dataset_manager_ftd.o \
./openthread/src/core/meshcop/dtls.o \
./openthread/src/core/meshcop/energy_scan_client.o \
./openthread/src/core/meshcop/joiner.o \
./openthread/src/core/meshcop/joiner_router.o \
./openthread/src/core/meshcop/leader.o \
./openthread/src/core/meshcop/meshcop.o \
./openthread/src/core/meshcop/meshcop_tlvs.o \
./openthread/src/core/meshcop/panid_query_client.o \
./openthread/src/core/meshcop/timestamp.o 

CPP_DEPS += \
./openthread/src/core/meshcop/announce_begin_client.d \
./openthread/src/core/meshcop/border_agent.d \
./openthread/src/core/meshcop/commissioner.d \
./openthread/src/core/meshcop/dataset.d \
./openthread/src/core/meshcop/dataset_local.d \
./openthread/src/core/meshcop/dataset_manager.d \
./openthread/src/core/meshcop/dataset_manager_ftd.d \
./openthread/src/core/meshcop/dtls.d \
./openthread/src/core/meshcop/energy_scan_client.d \
./openthread/src/core/meshcop/joiner.d \
./openthread/src/core/meshcop/joiner_router.d \
./openthread/src/core/meshcop/leader.d \
./openthread/src/core/meshcop/meshcop.d \
./openthread/src/core/meshcop/meshcop_tlvs.d \
./openthread/src/core/meshcop/panid_query_client.d \
./openthread/src/core/meshcop/timestamp.d 


# Each subdirectory must supply rules for building sources it contributes
openthread/src/core/meshcop/%.o: ../openthread/src/core/meshcop/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C++ Compiler'
	arm-none-eabi-c++ -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_CONFIG_FILE='"openthread-config-generic.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/MQTTSNPacket/src" -O0 -fno-common -g3 -Wall -c -fmessage-length=0 -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


