/*
 * Copyright (c) 2022 Talkweb Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef __WATCHDOG_STM32F4XX_H__
#define __WATCHDOG_STM32F4XX_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define WATCHDOG_MIN_TIMEOUT    1
#define WATCHDOG_MAX_TIMEOUT    4096

#define WATCHDOG_UPDATE_TIME    (((6UL * 256UL * 1000UL) / LSI_VALUE) + ((LSI_STARTUP_TIME / 1000UL) + 1UL))

typedef struct {
    int watchdogId;
    int timeout;    // Maximum interval between watchdog feeding, unit: ms
} WatchdogDeviceInfo;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif