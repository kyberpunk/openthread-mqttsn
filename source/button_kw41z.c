/*
 *  Copyright (c) 2018, Vit Holasek.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @author Vit Holasek
 * @brief
 *  The file contains push-button functions implementation for KW41Z platform.
 */

#include <stdint.h>
#include <stdbool.h>
#include <openthread/platform/alarm-milli.h>

#include "smart_socket_platform.h"
#include "smart_socket_config.h"
#include "button_abstraction.h"
#include "board.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_common.h"
#include "pin_mux.h"
#include "clock_config.h"

// PTC4 pin is used for push-button interrupt
#define SOCKET_SW_GPIO GPIOC
#define SOCKET_SW_PORT PORTC
#define SOCKET_SW_GPIO_PIN 4U
#define SOCKET_SW_IRQ PORTB_PORTC_IRQn
#define SOCKET_SW_IRQ_HANDLER PORTB_PORTC_IRQHandler

static button_cb_t short_press_callback = NULL;
static button_cb_t long_press_callback = NULL;

static bool buttonPressed = false;
static uint32_t buttonTime = 0;

static void button_down()
{
	buttonTime = socket_get_millis();
}

static void button_up()
{
	uint32_t tmpTime = socket_get_millis() - buttonTime;
	socket_platform_log(SOCKET_LOG_INFO, "Button pressed for time %lu", tmpTime);
	if (tmpTime >= LONG_PRESS)
	{
		if (long_press_callback) {
			long_press_callback();
		}
	}
	else
	{
		if (short_press_callback) {
			short_press_callback();
		}
	}
}

/**
 * Button interrupt handler for falling or rising edge.
 */
void SOCKET_SW_IRQ_HANDLER(void)
{
	if (GPIO_GetPinsInterruptFlags(SOCKET_SW_GPIO) & (1U << SOCKET_SW_GPIO_PIN))
	{
		// Clear external interrupt flag.
		GPIO_ClearPinsInterruptFlags(SOCKET_SW_GPIO, 1U << SOCKET_SW_GPIO_PIN);

		if (buttonPressed)
		{
			// Rising edge - button up
			button_up();
			PORT_SetPinInterruptConfig(SOCKET_SW_PORT, SOCKET_SW_GPIO_PIN, kPORT_InterruptFallingEdge);
			buttonPressed = false;
		}
		else
		{
			// Falling edge - button down
			button_down();
			PORT_SetPinInterruptConfig(SOCKET_SW_PORT, SOCKET_SW_GPIO_PIN, kPORT_InterruptRisingEdge);
			buttonPressed = true;
		}
	}
}

void button_init(void) {
	CLOCK_EnableClock(kCLOCK_PortC);
	// Configure GPIO pins
	const port_pin_config_t sw_port_config =
		{
			kPORT_PullUp,
			kPORT_SlowSlewRate,
			kPORT_PassiveFilterDisable,
			kPORT_LowDriveStrength,
			kPORT_MuxAsGpio
		};
	PORT_SetPinConfig(SOCKET_SW_PORT, SOCKET_SW_GPIO_PIN, &sw_port_config);

	buttonPressed = false;
	// Enable interrupt
	PORT_SetPinInterruptConfig(SOCKET_SW_PORT, SOCKET_SW_GPIO_PIN, kPORT_InterruptFallingEdge);
	NVIC_EnableIRQ(SOCKET_SW_IRQ);
	gpio_pin_config_t sw_config =
			{
				kGPIO_DigitalInput,
				0
			};
	GPIO_PinInit(SOCKET_SW_GPIO, SOCKET_SW_GPIO_PIN, &sw_config);
}

void button_short_press_handler(button_cb_t callback) {
	short_press_callback = callback;
}

void button_long_press_handler(button_cb_t callback) {
	long_press_callback = callback;
}

void button_deinit(void) {
	// Disable interrupts
	PORT_SetPinInterruptConfig(SOCKET_SW_PORT, SOCKET_SW_GPIO_PIN, kPORT_InterruptOrDMADisabled);
	NVIC_DisableIRQ(SOCKET_SW_IRQ);
}
