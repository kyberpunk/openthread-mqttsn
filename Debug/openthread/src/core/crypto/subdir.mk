################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../openthread/src/core/crypto/aes_ccm.cpp \
../openthread/src/core/crypto/aes_ecb.cpp \
../openthread/src/core/crypto/ecdsa.cpp \
../openthread/src/core/crypto/hmac_sha256.cpp \
../openthread/src/core/crypto/mbedtls.cpp \
../openthread/src/core/crypto/pbkdf2_cmac.cpp \
../openthread/src/core/crypto/sha256.cpp 

OBJS += \
./openthread/src/core/crypto/aes_ccm.o \
./openthread/src/core/crypto/aes_ecb.o \
./openthread/src/core/crypto/ecdsa.o \
./openthread/src/core/crypto/hmac_sha256.o \
./openthread/src/core/crypto/mbedtls.o \
./openthread/src/core/crypto/pbkdf2_cmac.o \
./openthread/src/core/crypto/sha256.o 

CPP_DEPS += \
./openthread/src/core/crypto/aes_ccm.d \
./openthread/src/core/crypto/aes_ecb.d \
./openthread/src/core/crypto/ecdsa.d \
./openthread/src/core/crypto/hmac_sha256.d \
./openthread/src/core/crypto/mbedtls.d \
./openthread/src/core/crypto/pbkdf2_cmac.d \
./openthread/src/core/crypto/sha256.d 


# Each subdirectory must supply rules for building sources it contributes
openthread/src/core/crypto/%.o: ../openthread/src/core/crypto/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C++ Compiler'
	arm-none-eabi-c++ -D__NEWLIB__ -DFSL_RTOS_BM -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DMBEDTLS_CONFIG_FILE='"mbedtls-config.h"' -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE='"openthread-core-kw41z-config.h"' -DOPENTHREAD_FTD=1 -DCPU_MKW41Z512VHT4 -DCPU_MKW41Z512VHT4_cm0plus -DOPENTHREAD_CONFIG_ENABLE_DEBUG_UART=1 -DOPENTHREAD_CONFIG_LOG_OUTPUT=OPENTHREAD_CONFIG_LOG_OUTPUT_APP -I../board -I../source -I../ -I../drivers -I../CMSIS -I../utilities -I../startup -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/src/core" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/mbedtls/repo/include" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/examples/platforms/kw41z" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/openthread/third_party/nxp/MKW41Z4/XCVR" -I"C:/MCUXpressoIDE/workspace/openthread-mqttsn/paho/MQTTSNPacket/src" -O1 -fno-common -g3 -Wall -c -fmessage-length=0 -mcpu=cortex-m0plus -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


