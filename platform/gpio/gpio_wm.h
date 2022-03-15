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
#ifndef GPIO_WINNERMICRO_H
#define GPIO_WINNERMICRO_H

#include "gpio_core.h"
#include "gpio_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pin configuration
 */
enum GPIO_CONFIG {
    ANALOG_MODE,               /* Used as a function pin, input and output analog */
    IRQ_MODE,                  /* Used to trigger interrupt */
    INPUT_PULL_UP,             /* Input with an internal pull-up resistor - use with devices
                                  that actively drive the signal low - e.g. button connected to ground */
    INPUT_PULL_DOWN,           /* Input with an internal pull-down resistor - use with devices
                                  that actively drive the signal high - e.g. button connected to a power rail */
    INPUT_HIGH_IMPEDANCE,      /* Input - must always be driven, either actively or by an external pullup resistor */
    OUTPUT_PUSH_PULL,          /* Output actively driven high and actively driven low -
                                  must not be connected to other active outputs - e.g. LED output */
    OUTPUT_OPEN_DRAIN_NO_PULL, /* Output actively driven low but is high-impedance when set high -
                                  can be connected to other open-drain/open-collector outputs.
                                  Needs an external pull-up resistor */
    OUTPUT_OPEN_DRAIN_PULL_UP, /* Output actively driven low and is pulled high
                                  with an internal resistor when set high -
                                  can be connected to other open-drain/open-collector outputs. */
};

struct GpioResource {
    uint32_t groupNum;
    uint32_t realPin;
    uint32_t config;
    uint32_t pinNum;
};

struct GpioDevice {
    uint8_t port; /* gpio port */
    struct GpioResource resource;
    enum GPIO_CONFIG config; /* gpio config */
};

typedef void (* tls_gpio_pin_orq_handler)(enum tls_io_name pin);

#define DECIMALNUM 10
#define OCTALNUM 8
#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H__ */