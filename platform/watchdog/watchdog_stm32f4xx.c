/*
 * Copyright (c) 2022 Talkweb Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <stdlib.h>
#include <stdio.h>
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#include "hcs_macro.h"
#include "hdf_config_macro.h"
#else
#include "device_resource_if.h"
#endif
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "watchdog_core.h"
#include "watchdog_if.h"

#define WATCHDOG_MIN_TIMEOUT    1
#define WATCHDOG_MAX_TIMEOUT    4096
#define WATCHDOG_UPDATE_TIME    (((6UL * 256UL * 1000UL) / LSI_VALUE) + ((LSI_STARTUP_TIME / 1000UL) + 1UL))

typedef struct {
    int watchdogId;
    int timeout;    // Maximum interval between watchdog feeding, unit: ms
} WatchdogDeviceInfo;

static IWDG_TypeDef *hdf_iwdg = NULL;
static int g_watchdogStart = 0;
static int g_watchdogTimeout = 0;

static int32_t WatchdogDevStart(struct WatchdogCntlr *watchdogCntlr);
static int32_t WatchdogDevStop(struct WatchdogCntlr *watchdogCntlr);
static int32_t WatchdogDevSetTimeout(struct WatchdogCntlr *watchdogCntlr, uint32_t seconds);
static int32_t WatchdogDevGetTimeout(struct WatchdogCntlr *watchdogCntlr, uint32_t *seconds);
static int32_t WatchdogDevGetStatus(struct WatchdogCntlr *watchdogCntlr, uint32_t *status);
static int32_t WatchdogDevFeed(struct WatchdogCntlr *watchdogCntlr);

struct WatchdogMethod g_WatchdogCntlrMethod = {
    .getStatus = WatchdogDevGetStatus,
    .setTimeout = WatchdogDevSetTimeout,
    .getTimeout = WatchdogDevGetTimeout,
    .start  = WatchdogDevStart,
    .stop   = WatchdogDevStop,
    .feed   = WatchdogDevFeed,
    .getPriv = NULL,
    .releasePriv = NULL,
};

static int InitWatchdogDeviceInfo(WatchdogDeviceInfo *watchdogdeviceinfo)
{
    if (watchdogdeviceinfo == NULL) {
        HDF_LOGE("%s: invaild parameter\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    return HDF_SUCCESS;
}

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#define WATCHDOG_FIND_CONFIG(node, name, device) \
    do { \
        if (strcmp(HCS_PROP(node, match_attr), name) == 0) { \
            device->watchdogId = HCS_PROP(node, id); \
            device->timeout = HCS_PROP(node, timeout); \
            result = HDF_SUCCESS; \
        } \
    } while (0)
#define PLATFORM_CONFIG HCS_NODE(HCS_ROOT, platform)
#define PLATFORM_WATCHDOG_CONFIG HCS_NODE(HCS_NODE(HCS_ROOT, platform), watchdog_config)
static uint32_t GetWatchdogDeviceInfoResource(WatchdogDeviceInfo *device, const char *deviceMatchAttr)
{
    int32_t result = HDF_FAILURE;
    if (device == NULL || deviceMatchAttr == NULL) {
        HDF_LOGE("device or deviceMatchAttr is NULL\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
#if HCS_NODE_HAS_PROP(PLATFORM_CONFIG, watchdog_config)
    HCS_FOREACH_CHILD_VARGS(PLATFORM_WATCHDOG_CONFIG, WATCHDOG_FIND_CONFIG, deviceMatchAttr, device);
#endif
    if (result != HDF_SUCCESS) {
        HDF_LOGE("resourceNode %s is NULL\r\n", deviceMatchAttr);
        return result;
    }

    return HDF_SUCCESS;
}
#else
static uint32_t GetWatchdogDeviceInfoResource(WatchdogDeviceInfo *device, const struct DeviceResourceNode *resourceNode)
{
    struct DeviceResourceIface *dri = NULL;
    if (device == NULL || resourceNode == NULL) {
        HDF_LOGE("resource or device is NULL\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    
    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (dri->GetUint32(resourceNode, "id", &device->watchdogId, 0) != HDF_SUCCESS) {
        HDF_LOGE("read watchdogId fail\r\n");
        return HDF_FAILURE;
    }
    if (dri->GetUint32(resourceNode, "timeout", &device->timeout, 0) != HDF_SUCCESS) {
        HDF_LOGE("read watchdogId fail\r\n");
        return HDF_FAILURE;
    }

    HDF_LOGI("watchdogId = %d\n", device->watchdogId);
    HDF_LOGI("timeout = %dms\n", device->timeout);

    return HDF_SUCCESS;
}
#endif

static int32_t AttachWatchdogDeviceInfo(struct WatchdogCntlr *watchdogCntlr, struct HdfDeviceObject *device)
{
    int32_t ret;
    WatchdogDeviceInfo *watchdogdeviceinfo = NULL;

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    if (device == NULL || device->deviceMatchAttr == NULL) {
#else
    if (device == NULL || device->property == NULL) {
#endif
        HDF_LOGE("%s: param is NULL\r\n", __func__);
        return HDF_FAILURE;
    }

    watchdogdeviceinfo = (WatchdogDeviceInfo *)OsalMemAlloc(sizeof(WatchdogDeviceInfo));
    if (watchdogdeviceinfo == NULL) {
        HDF_LOGE("%s: OsalMemAlloc WatchdogDeviceInfo error\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    ret = GetWatchdogDeviceInfoResource(watchdogdeviceinfo, device->deviceMatchAttr);
#else
    ret = GetWatchdogDeviceInfoResource(watchdogdeviceinfo, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(watchdogdeviceinfo);
        return HDF_FAILURE;
    }

    (void)OsalMutexInit(&watchdogCntlr->lock);

    watchdogCntlr->priv = watchdogdeviceinfo;
    watchdogCntlr->wdtId = watchdogdeviceinfo->watchdogId;

    return InitWatchdogDeviceInfo(watchdogdeviceinfo);
}
/* HdfDriverEntry method definitions */
static int32_t WatchdogDriverBind(struct HdfDeviceObject *device);
static int32_t WatchdogDriverInit(struct HdfDeviceObject *device);
static void WatchdogDriverRelease(struct HdfDeviceObject *device);

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_watchdogDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "ST_WATCHDOG_MODULE_HDF",
    .Bind = WatchdogDriverBind,
    .Init = WatchdogDriverInit,
    .Release = WatchdogDriverRelease,
};

// Initialize HdfDriverEntry
HDF_INIT(g_watchdogDriverEntry);

static int32_t WatchdogDriverBind(struct HdfDeviceObject *device)
{
    struct WatchdogCntlr *watchdogCntlr = NULL;

    if (device == NULL) {
        HDF_LOGE("hdfDevice object is null!\r\n");
        return HDF_FAILURE;
    }

    watchdogCntlr = (struct WatchdogCntlr *)OsalMemAlloc(sizeof(struct WatchdogCntlr));
    if (watchdogCntlr == NULL) {
        HDF_LOGE("%s: OsalMemAlloc watchdogCntlr error\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    HDF_LOGI("Enter %s\r\n", __func__);
    device->service = &watchdogCntlr->service;
    watchdogCntlr->device = device;
    watchdogCntlr->priv = NULL;
    return HDF_SUCCESS;
}

static int32_t WatchdogDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct WatchdogCntlr *watchdogCntlr = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    HDF_LOGI("Enter %s:\r\n", __func__);

    watchdogCntlr = WatchdogCntlrFromDevice(device);
    if (watchdogCntlr == NULL) {
        HDF_LOGE("%s: watchdogCntlr is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    ret = AttachWatchdogDeviceInfo(watchdogCntlr, device);
    if (ret != HDF_SUCCESS) {
        OsalMemFree(watchdogCntlr);
        HDF_LOGE("%s:attach error\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    watchdogCntlr->ops = &g_WatchdogCntlrMethod;

    HDF_LOGI("WatchdogDriverInit success!\r\n");
    return ret;
}

static void WatchdogDriverRelease(struct HdfDeviceObject *device)
{
    struct WatchdogCntlr *watchdogCntlr = NULL;
    WatchdogDeviceInfo *watchdogdeviceinfo = NULL;

    if (device == NULL) {
        HDF_LOGE("device is null\r\n");
        return;
    }

    watchdogCntlr = WatchdogCntlrFromDevice(device);
    if (watchdogCntlr == NULL || watchdogCntlr->priv == NULL) {
        HDF_LOGE("%s: watchdogCntlr is NULL\r\n", __func__);
        return;
    }

    watchdogdeviceinfo = (WatchdogDeviceInfo *)watchdogCntlr->priv;
    if (watchdogdeviceinfo != NULL) {
        OsalMemFree(watchdogdeviceinfo);
    }
    return;
}

static int32_t WatchdogDevStart(struct WatchdogCntlr *watchdogCntlr)
{
    WatchdogDeviceInfo *watchdogdeviceinfo = NULL;
    int32_t watchdogId = 0;
    int32_t timeout = 0;
    unsigned long long tickstart = 0;
    
    if (watchdogCntlr == NULL || watchdogCntlr->priv == NULL) {
        HDF_LOGE("%s: watchdogCntlr is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    watchdogdeviceinfo = (WatchdogDeviceInfo *)watchdogCntlr->priv;
    if (watchdogdeviceinfo == NULL) {
        HDF_LOGE("%s: OBJECT is NULL\r\n", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    watchdogId = watchdogdeviceinfo->watchdogId;
    timeout = watchdogdeviceinfo->timeout;

    if (timeout < WATCHDOG_MIN_TIMEOUT) {
        HDF_LOGW("%s: watchdog timeout must >= 1, set the timeout to 1ms\r\n", __func__);
        timeout = WATCHDOG_MIN_TIMEOUT;
    }
    if (timeout > WATCHDOG_MAX_TIMEOUT) {
        HDF_LOGW("%s: watchdog timeout must <= 1, set the timeout to 4096ms\r\n", __func__);
        timeout = WATCHDOG_MAX_TIMEOUT;
    }

    HDF_LOGI("%s: watchdog Started! timeout: %dms\r\n", __func__, timeout);

    hdf_iwdg = IWDG;  // Point to watchdog register
    hdf_iwdg->KR = IWDG_KEY_ENABLE;
    hdf_iwdg->KR = IWDG_KEY_WRITE_ACCESS_ENABLE;
    hdf_iwdg->PR = IWDG_PRESCALER_32;   // 32 frequency division
    hdf_iwdg->RLR = timeout - 1;        // 32K crystal oscillator corresponds to 1-4096ms under 32 prescaled frequency

    tickstart = LOS_TickCountGet();
    // Wait for the register value to be updated and confirm that the watchdog is started successfully
    while ((hdf_iwdg->SR & (IWDG_SR_RVU | IWDG_SR_PVU)) != 0x00u) {
        if ((LOS_TickCountGet() - tickstart) > WATCHDOG_UPDATE_TIME) {
            if ((hdf_iwdg->SR & (IWDG_SR_RVU | IWDG_SR_PVU)) != 0x00u) {
                return HDF_FAILURE;
            }
        }
    }
    hdf_iwdg->KR = IWDG_KEY_RELOAD;  // Reload initial value

    g_watchdogStart = 1;
    return HDF_SUCCESS;
}

static int32_t WatchdogDevStop(struct WatchdogCntlr *watchdogCntlr)
{
    HDF_LOGW("%s: WatchdogDevStop fail,because of soc not support!!\r\n", __func__);
    return HDF_FAILURE;
}

static int32_t WatchdogDevSetTimeout(struct WatchdogCntlr *watchdogCntlr, uint32_t seconds)
{
    WatchdogDeviceInfo *watchdogdeviceinfo = NULL;
    if (watchdogCntlr == NULL || watchdogCntlr->priv == NULL) {
        HDF_LOGE("%s: watchdogCntlr is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    watchdogdeviceinfo = (WatchdogDeviceInfo *)watchdogCntlr->priv;
    if (watchdogdeviceinfo == NULL) {
        HDF_LOGE("%s: OBJECT is NULL\r\n", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    watchdogdeviceinfo->timeout = seconds;

    return HDF_SUCCESS;
}

static int32_t WatchdogDevGetTimeout(struct WatchdogCntlr *watchdogCntlr, uint32_t *seconds)
{
    WatchdogDeviceInfo *watchdogdeviceinfo = NULL;
    if (watchdogCntlr == NULL || seconds == NULL) {
        HDF_LOGE("%s: PARAM is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    watchdogdeviceinfo = (WatchdogDeviceInfo *)watchdogCntlr->priv;
    if (watchdogdeviceinfo == NULL) {
        HDF_LOGE("%s: OBJECT is NULL\r\n", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    *seconds = watchdogdeviceinfo->timeout;
    return HDF_SUCCESS;
}

static int32_t WatchdogDevGetStatus(struct WatchdogCntlr *watchdogCntlr, uint32_t *status)
{
    if (watchdogCntlr == NULL || status == NULL) {
        HDF_LOGE("%s: PARAM is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    if (g_watchdogStart == 1) {
        *status = WATCHDOG_START;
    } else {
        *status = WATCHDOG_STOP;
    }
    return HDF_SUCCESS;
}

static int32_t WatchdogDevFeed(struct WatchdogCntlr *watchdogCntlr)
{
    (void)watchdogCntlr;
    if (hdf_iwdg != NULL)
        hdf_iwdg->KR = IWDG_KEY_RELOAD;  // Reload initial value
    return HDF_SUCCESS;
}

static int32_t WatchdogDevGetPriv(struct WatchdogCntlr *watchdogCntlr)
{
    (void)watchdogCntlr;
    return HDF_SUCCESS;
}

static int32_t WatchdogDevReleasePriv(struct WatchdogCntlr *watchdogCntlr)
{
    (void)watchdogCntlr;
    return HDF_SUCCESS;
}