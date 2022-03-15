/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef PWM_WINNERMICRO_H
#define PWM_WINNERMICRO_H

#include "pwm_core.h"
#include "wm_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PwmResource {
    uint32_t channel;
    uint32_t freq;
};

struct PwmDevice {
    struct IDeviceIoService ioService;
    pwm_init_param pwmCfg;
    struct PwmConfig *cfg;
    struct PwmResource resource;
};

#ifdef __cplusplus
}
#endif
#endif
