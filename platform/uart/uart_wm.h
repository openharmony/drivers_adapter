/*
 * Copyright (c) 2022 Winner Microelectronics Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef UART_WINNERMICRO_H
#define UART_WINNERMICRO_H

#include "uart_if.h"
#include "uart_core.h"
#include "osal_sem.h"
#include "wm_uart.h"
#ifdef __cplusplus
extern "C" {
#endif

#define UART_DEV_SERVICE_NAME_PREFIX "HDF_PLATFORM_UART%d"
#define MAX_DEV_NAME_SIZE 32

struct UartResource {
    uint32_t num;      /* UART port num */
    uint32_t baudRate; /* Default baudrate */
    uint32_t wLen;     /* Default word length */
    uint32_t parity;   /* Default parity */
    uint32_t stopBit;  /* Default stop bits */
    bool txDMA;
    bool rxDMA;
};

enum UartDeviceState {
    UART_DEVICE_UNINITIALIZED = 0x0u,
    UART_DEVICE_INITIALIZED = 0x1u,
};

struct UART_CTX_OBJ {
    bool txDMA;
    bool rxDMA;
    bool isBlock;
    struct OsalSem rxSem;
    struct OsalSem txSem;
};

struct UartDevice {
    struct IDeviceIoService ioService;
    struct UartResource resource;
    tls_uart_options_t config;
    uint32_t uartId;
    bool initFlag;
    uint32_t transMode;
};

enum {
    UART_READ = 0,
    UART_WRITE
};

int32_t UartDispatch(struct HdfDeviceIoClient *client, int cmdId, struct HdfSBuf *data, struct HdfSBuf *reply);

#ifdef __cplusplus
}
#endif

#endif