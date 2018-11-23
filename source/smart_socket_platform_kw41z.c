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
 *  The file contains smart socket platform specific functionality implementation for KW41Z platform.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "fsl_common.h"
#include "fsl_port.h"
#include "pin_mux.h"

#include "fsl_device_registers.h"
#include "smart_socket_platform.h"
#include "smart_socket_config.h"
#include "board.h"
#include "platform.h"
#include "fsl_debug_console.h"

#define PIN6_IDX                         6u
#define PIN7_IDX                         7u
#define SOPT5_LPUART0RXSRC_LPUART_RX  0x00u
#define LED_PIN                         19U
#define LED_PORT                      PORTA
#define LED_GPIO_PORT                 GPIOA
#define CS_PIN                           1U
#define CS_PORT                       PORTB
#define CS_GPIO_PORT                  GPIOB
#define EN_PIN                           3U
#define EN_PORT                       PORTB
#define EN_GPIO_PORT                  GPIOB
#define SYN_PIN                          2U
#define SYN_PORT                      PORTB
#define SYN_GPIO_PORT                 GPIOB

#define PIN16_IDX                       16U
#define PIN17_IDX                       17U
#define PIN18_IDX                       18U
#define PIN19_IDX                       19U

SPI_Instance_t spi1 = {
		.clock = DSPI1_CLK_SRC,
		.spi_type = DSPI1
};

spi_handle_data spi_config_default = {
		.spi = &spi1
};

pin_handle_data pin_config_default = {
		.cs_pin = CS_PIN,
		.cs_pin_type = CS_GPIO_PORT,
		.en_pin = EN_PIN,
		.en_pin_type = EN_GPIO_PORT,
		.syn_pin = SYN_PIN,
		.syn_pin_type = SYN_GPIO_PORT
};

void socket_platform_init() {
	CLOCK_EnableClock(kCLOCK_PortC);
	PORT_SetPinMux(PORTC, PIN6_IDX, kPORT_MuxAlt4);
	PORT_SetPinMux(PORTC, PIN7_IDX, kPORT_MuxAlt4);

	CLOCK_EnableClock(kCLOCK_PortA);
	CLOCK_EnableClock(kCLOCK_PortB);

	// Configure GPIO pins
	// LED indicator pin
	PORT_SetPinMux(LED_PORT, LED_PIN, kPORT_MuxAsGpio);
	// Metrology module control pins
	PORT_SetPinMux(CS_PORT, CS_PIN, kPORT_MuxAsGpio);
	PORT_SetPinMux(EN_PORT, EN_PIN, kPORT_MuxAsGpio);
	PORT_SetPinMux(SYN_PORT, SYN_PIN, kPORT_MuxAsGpio);

	// Configure SPI for metrology module
	PORT_SetPinMux(PORTA, PIN16_IDX, kPORT_MuxAlt2); // SPI1_SOUT
	PORT_SetPinMux(PORTA, PIN17_IDX, kPORT_MuxAlt2); // SPI1_SIN
	PORT_SetPinMux(PORTA, PIN18_IDX, kPORT_MuxAlt2); // SPI1_SCK
	//PORT_SetPinMux(PORTC, PIN19_IDX, kPORT_MuxAlt2); // SPI1_PCS0

	SIM->SOPT5 = ((SIM->SOPT5 & (~(SIM_SOPT5_LPUART0RXSRC_MASK))) | SIM_SOPT5_LPUART0RXSRC(SOPT5_LPUART0RXSRC_LPUART_RX));

	// Init debug console to printing on LPUART0
	BOARD_InitDebugConsole();
}

char *socket_level_to_text(socket_log_level_t level) {
	switch (level) {
	case SOCKET_LOG_NONE:
		return "VERBOSE";
	case SOCKET_LOG_INFO:
		return "INFO";
	case SOCKET_LOG_WARN:
		return "WARNING";
	case SOCKET_LOG_CRIT:
		return "CRITICAL";
	default:
		return "UNKNOWN";
	}
}

void socket_platform_log(socket_log_level_t level, const char *format, ...) {
	if (level >= SMART_SOCKET_LOG_LEVEL) {
		char *level_text = socket_level_to_text(level);
		va_list args;
		va_start(args, format);
		printf("%s:", level_text);
		vprintf(format, args);
		printf("\r\n");
		va_end(args);
	}
}

void socket_platform_reset(void) {
    NVIC_SystemReset();

    while (1)
    {

    }
}
