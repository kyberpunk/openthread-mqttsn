################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include utilities/subdir.mk
-include startup/subdir.mk
-include source/subdir.mk
-include paho/MQTTSNPacket/src/subdir.mk
-include openthread/third_party/nxp/MKW41Z4/XCVR/cfgs_kw4x_3x_2x/subdir.mk
-include openthread/third_party/nxp/MKW41Z4/XCVR/XCVR_Test/subdir.mk
-include openthread/third_party/nxp/MKW41Z4/XCVR/subdir.mk
-include openthread/third_party/mbedtls/repo/library/subdir.mk
-include openthread/src/core/utils/subdir.mk
-include openthread/src/core/thread/subdir.mk
-include openthread/src/core/net/subdir.mk
-include openthread/src/core/meshcop/subdir.mk
-include openthread/src/core/mac/subdir.mk
-include openthread/src/core/crypto/subdir.mk
-include openthread/src/core/common/subdir.mk
-include openthread/src/core/coap/subdir.mk
-include openthread/src/core/api/subdir.mk
-include openthread/examples/platforms/utils/subdir.mk
-include openthread/examples/platforms/kw41z/subdir.mk
-include drivers/subdir.mk
-include board/subdir.mk
-include CMSIS/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: openthread-mqttsn.axf

# Tool invocations
openthread-mqttsn.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU C++ Linker'
	arm-none-eabi-c++ -nostdlib -Xlinker -Map="openthread-mqttsn.map" -Xlinker --gc-sections -Xlinker -print-memory-usage -mcpu=cortex-m0plus -mthumb -T openthread-mqttsn_Debug.ld -u _printf_float -u _scanf_float -o "openthread-mqttsn.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(CC_DEPS)$(C++_DEPS)$(EXECUTABLES)$(C_UPPER_DEPS)$(CXX_DEPS)$(OBJS)$(CPP_DEPS)$(C_DEPS) openthread-mqttsn.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size "openthread-mqttsn.axf"; # arm-none-eabi-objcopy -v -O binary "openthread-mqttsn.axf" "openthread-mqttsn.bin" ; # checksum -p MKW41Z512xxx4 -d "openthread-mqttsn.bin";
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
