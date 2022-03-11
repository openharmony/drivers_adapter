/*
 * Copyright (c) 2021 Bestechnic (Shanghai) Co., Ltd. All rights reserved.
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
#ifndef __PWM_BES_H__
#define __PWM_BES_H__

#include "hal_pwm.h"
#include "hal_gpio.h"
#ifdef CHIP_BEST2003
#include "hal_iomux.h"
#endif
#include "pwm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PwmResource {
    uint32_t pwmPin;
    uint32_t pwmId;
};

struct PwmDevice {
    struct IDeviceIoService ioService;
    struct HAL_PWM_CFG_T pwmCfg;
    struct PwmConfig *cfg;
    struct PwmResource resource;
};

#ifdef __cplusplus
}
#endif
#endif