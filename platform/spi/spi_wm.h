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
#ifndef SPI_WINNERMICRO_H
#define SPI_WINNERMICRO_H

#include "osal_mutex.h"
#include "osal_sem.h"
#include "wm_hostspi.h"
#include "spi_if.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SpiResource {
    uint32_t num;
    uint32_t speed;
    enum SpiTransferMode transmode;
    uint32_t mode; // TLS_SPI_MODE_x
    uint32_t dataSize;
    uint32_t spiCsSoft;
    uint32_t spiClkPin;
    uint32_t spiMosiPin;
    uint32_t spiMisoPin;
    uint32_t spiCsPin;
};

struct SpiDevice {
    uint32_t spiId;
    struct SpiResource resource;
    struct OsalMutex mutex;
};

#ifdef __cplusplus
}
#endif

#endif