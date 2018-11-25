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
 *  The file contains smart socket configuration.
 */

#ifndef SMART_SOCKET_CONFIG_H_
#define SMART_SOCKET_CONFIG_H_

#include "smart_socket_platform.h"

#ifndef PSKD
/**
 * @def PSKD
 *
 * Joiner pre-shared key.
 *
 */
#define PSKD "123456"
#endif

#ifndef ADDRESS_LT
/**
 * @def ADDRESS_LT
 *
 * Device address registration timeout in milliseconds for gateway.
 *
 */
#define ADDRESS_LT 86400
#endif

#ifndef LONG_PRESS
/**
 * @def LONG_PRESS
 *
 * Push-button long press threshold time on milliseconds.
 *
 */
#define LONG_PRESS 3000
#endif

#ifndef SMART_SOCKET_LOG_LEVEL
/**
 * @def COAP_ATTEMPTS
 *
 * CoAP request attempts before failure.
 *
 */
#define SMART_SOCKET_LOG_LEVEL SOCKET_LOG_INFO
#endif

#ifndef BYPASS_JOINER
/**
 * @def BYPASS_JOINER
 *
 * Set value to 1 to bypass joiner process and start network with predefined settings or set 0 otherwise.
 *
 */
#define BYPASS_JOINER 1
#endif

#ifndef NETWORK_NAME
/**
 * @def NETWORK_NAME
 *
 * Thread network name string. Used only for joiner bypass.
 *
 */
#define NETWORK_NAME "OpenThreadDemo"
#endif

#ifndef PANID
/**
 * @def PANID
 *
 * Thread PAN ID string (hexadecimal in format 0xffff). Used only for joiner bypass.
 *
 */
#define PANID "0x1234"
#endif

#ifndef EXTPANID
/**
 * @def EXTPANID
 *
 * Thread extended PAN ID string (hexadecimal). Used only for joiner bypass.
 *
 */
#define EXTPANID "1111111122222222"
#endif

#ifndef DEFAULT_CHANNEL
/**
 * @def DEFAULT_CHANNEL
 *
 * Default radio channel number. Used only for joiner bypass.
 *
 */
#define DEFAULT_CHANNEL 15
#endif

#ifndef MASTER_KEY
/**
 * @def MASTER_KEY
 *
 * Network master key string (hexadecimal). Used only for joiner bypass.
 *
 */
#define MASTER_KEY "00112233445566778899aabbccddeeff"
#endif

#endif /* SMART_SOCKET_CONFIG_H_ */
