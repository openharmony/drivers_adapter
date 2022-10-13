/*
 * Copyright (c) 2021-2022 Bestechnic (Shanghai) Co., Ltd. All rights reserved.
 *
 * This file is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "i2c_bes.h"
#include <stdlib.h>
#include <securec.h>
#include "i2c_core.h"
#include "i2c_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#include "hcs_macro.h"
#include "hdf_config_macro.h"
#else
#include "device_resource_if.h"
#endif

#define DEC_NUM 10
#define GROUP_PIN_NUM 8
#define I2C_INVALID_ADDR 0xFFFF

/* HdfDriverEntry method definitions */
static int32_t i2cDriverBind(struct HdfDeviceObject *device);
static int32_t i2cDriverInit(struct HdfDeviceObject *device);
static void i2cDriverRelease(struct HdfDeviceObject *device);

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_i2cDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "BES_I2C_MODULE_HDF",
    .Bind = i2cDriverBind,
    .Init = i2cDriverInit,
    .Release = i2cDriverRelease,
};

// Initialize HdfDriverEntry
HDF_INIT(g_i2cDriverEntry);

/* I2cHostMethod method definitions */
static int32_t i2cHostTransfer(struct I2cCntlr *cntlr, struct I2cMsg *msgs, int16_t count);

struct I2cMethod g_i2cHostMethod = {
    .transfer = i2cHostTransfer,
};

static int32_t I2cDeviceIomuxInit(uint32_t i2cId, struct I2cResource *resource)
{
    if (i2cId > HAL_I2C_ID_NUM || resource == NULL) {
        HDF_LOGE("%s %d: invalid parameter\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct HAL_IOMUX_PIN_FUNCTION_MAP pinMuxI2c[] = {
        {0, 0, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENABLE},
        {0, 0, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENABLE},
    };

#ifdef LOSCFG_SOC_SERIES_BES2600
    if (i2cId == 0) {
        pinMuxI2c[0].function = HAL_IOMUX_FUNC_I2C_M0_SCL;
        pinMuxI2c[1].function = HAL_IOMUX_FUNC_I2C_M0_SDA;
    } else {
        pinMuxI2c[0].function = HAL_IOMUX_FUNC_I2C_M1_SCL;
        pinMuxI2c[1].function = HAL_IOMUX_FUNC_I2C_M1_SDA;
    }
#elif defined (LOSCFG_SOC_SERIES_BES2700)
    if (i2cId == 0) {
        pinMuxI2c[0].function = HAL_IOMUX_FUNC_SYS_I2C_M0_SCL;
        pinMuxI2c[1].function = HAL_IOMUX_FUNC_SYS_I2C_M0_SDA;
    } else {
        pinMuxI2c[0].function = HAL_IOMUX_FUNC_SYS_I2C_M1_SCL;
        pinMuxI2c[1].function = HAL_IOMUX_FUNC_SYS_I2C_M1_SDA;
    }
#endif
    pinMuxI2c[0].pin = resource->sclPin;
    pinMuxI2c[1].pin = resource->sdaPin;
    hal_iomux_init(pinMuxI2c, ARRAY_SIZE(pinMuxI2c));
    return HDF_SUCCESS;
}

int32_t InitI2cDevice(struct I2cDevice *device)
{
    int32_t ret;
    uint32_t i2cPort;
    struct I2cResource *resource = NULL;
    struct HAL_I2C_CONFIG_T *i2cConfig = NULL;
    if (device == NULL) {
        HDF_LOGE("%s %d:device is NULL\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("%s %d: invalid parameter\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    i2cConfig = &device->i2cCfg;
    if (i2cConfig == NULL) {
        HDF_LOGE("%s %d: invalid parameter\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    device->port = resource->port;
    i2cPort = device->port;
    if (i2cPort > HAL_I2C_ID_NUM) {
        HDF_LOGE("i2c port %u not support\r\n", i2cPort);
        return HDF_ERR_NOT_SUPPORT;
    }

    if (OsalMutexInit(&device->mutex) != HDF_SUCCESS) {
        HDF_LOGE("%s %d OsalMutexInit fail\r\n", __func__, __LINE__);
        return HDF_FAILURE;
    }

    if (OsalMutexLock(&device->mutex) != HDF_SUCCESS) {
        HDF_LOGE("%s %d osMutexWait fail\r\n", __func__, __LINE__);
        return HDF_ERR_TIMEOUT;
    }

    ret = I2cDeviceIomuxInit(i2cPort, resource);
    if (ret != HDF_SUCCESS) {
        HDF_LOGD("I2C iomux init failed.\r\n");
        OsalMutexUnlock(&device->mutex);
        return ret;
    }

    ret = hal_i2c_open(i2cPort, i2cConfig);
    if (ret == HDF_SUCCESS) {
        HDF_LOGD("open %u i2c succ.\r\n", i2cPort);
    }
    OsalMutexUnlock(&device->mutex);
    return ret;
}

static int32_t GetI2cDeviceIomuxConfig(struct I2cResource *resource, const struct DeviceResourceNode *resourceNode)
{
    struct DeviceResourceIface *dri = NULL;
    if (resource == NULL || resourceNode == NULL) {
        HDF_LOGE("%s %d:resource or resourceNode is NULL\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (dri->GetUint32(resourceNode, "sclPin", &resource->sclPin, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config sclPin fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "sdaPin", &resource->sdaPin, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config sdaPin fail\r\n");
        return HDF_FAILURE;
    }
    resource->sclPin = ((resource->sclPin / DEC_NUM) * GROUP_PIN_NUM) + (resource->sclPin % DEC_NUM);

    resource->sdaPin = ((resource->sdaPin / DEC_NUM) * GROUP_PIN_NUM) + (resource->sdaPin % DEC_NUM);
    return HDF_SUCCESS;
}

static int32_t GetI2cDeviceResource(struct I2cDevice *device, const struct DeviceResourceNode *resourceNode)
{
    struct I2cResource *resource = NULL;
    struct DeviceResourceIface *dri = NULL;
    if (device == NULL || resourceNode == NULL) {
        HDF_LOGE("%s %d:device or resourceNode is NULL\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("%s %d: invalid parameter\r\n", __func__, __LINE__);
        return HDF_ERR_INVALID_OBJECT;
    }
    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (dri->GetUint32(resourceNode, "port", &resource->port, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config port fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "speed", &resource->speed, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config speed fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "mode", &resource->mode, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config mode fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "useDma", &resource->useDma, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config useDma fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "useSync", &resource->useSync, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config useSync fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "asMaster", &resource->asMaster, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config asMaster fail\r\n");
        return HDF_FAILURE;
    }

    return GetI2cDeviceIomuxConfig(resource, resourceNode);
}

static int32_t AttachI2cDevice(struct I2cCntlr *host, struct HdfDeviceObject *device)
{
    int32_t ret;
    struct I2cDevice *i2cDevice = NULL;
    struct I2cResource *resource = NULL;
    struct HAL_I2C_CONFIG_T *i2cConfig = NULL;
    if (device == NULL || host == NULL) {
        HDF_LOGE("%s: device or host is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    i2cDevice = (struct I2cDevice *)OsalMemAlloc(sizeof(struct I2cDevice));
    if (i2cDevice == NULL) {
        HDF_LOGE("%s: OsalMemAlloc i2cDevice error\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    (void)memset_s(i2cDevice, sizeof(struct I2cDevice), 0, sizeof(struct I2cDevice));
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    ret = GetI2cDeviceResource(i2cDevice, device->deviceMatchAttr);
#else
    ret = GetI2cDeviceResource(i2cDevice, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        OsalMemFree(i2cDevice);
        return HDF_FAILURE;
    }
    resource = &i2cDevice->resource;
    if (resource == NULL) {
        HDF_LOGE("%s %d: invalid parameter\r\n", __func__, __LINE__);
        OsalMemFree(i2cDevice);
        return HDF_ERR_INVALID_OBJECT;
    }
    i2cConfig = &i2cDevice->i2cCfg;
    if (i2cConfig == NULL) {
        HDF_LOGE("%s %d: invalid parameter\r\n", __func__, __LINE__);
        OsalMemFree(i2cDevice);
        return HDF_ERR_INVALID_OBJECT;
    }
    i2cDevice->port = resource->port;
    i2cConfig->mode = resource->mode;
    i2cConfig->use_sync = resource->useSync;
    i2cConfig->use_dma = resource->useDma;
    i2cConfig->as_master = resource->asMaster;
    i2cConfig->speed = resource->speed;
    i2cConfig->addr_as_slave = 0;
    i2cConfig->rising_time_ns = 0;

    host->priv = i2cDevice;
    host->busId = i2cDevice->port;

    return InitI2cDevice(i2cDevice);
}

static int32_t i2cDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct I2cCntlr *host = NULL;
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    host = (struct I2cCntlr *)OsalMemAlloc(sizeof(struct I2cCntlr));
    if (host == NULL) {
        HDF_LOGE("%s: host is NULL\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    (void)memset_s(host, sizeof(struct I2cCntlr), 0, sizeof(struct I2cCntlr));
    host->ops = &g_i2cHostMethod;
    device->priv = (void *)host;
    ret = AttachI2cDevice(host, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: attach error\r\n", __func__);
        return HDF_DEV_ERR_ATTACHDEV_FAIL;
    }
    ret = I2cCntlrAdd(host);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    return ret;
}

static int32_t i2cDriverBind(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        HDF_LOGE("%s: I2c device object is NULL\r\n", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static void i2cDriverRelease(struct HdfDeviceObject *device)
{
    struct I2cCntlr *i2cCntrl = NULL;
    struct I2cDevice *i2cDevice = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return;
    }
    i2cCntrl = device->priv;
    if (i2cCntrl == NULL || i2cCntrl->priv == NULL) {
        HDF_LOGE("%s: i2cCntrl is NULL\r\n", __func__);
        return;
    }
    i2cCntrl->ops = NULL;
    i2cDevice = (struct I2cDevice *)i2cCntrl->priv;
    OsalMemFree(i2cCntrl);

    if (i2cDevice != NULL) {
        OsalMutexDestroy(&i2cDevice->mutex);
        OsalMemFree(i2cDevice);
    }
}

static int32_t i2c_transfer(struct I2cDevice *device, struct I2cMsg *msgs, int16_t count)
{
    int ret;
    struct I2cMsg *msg = NULL;
    struct I2cMsg *msg2 = NULL;
    uint32_t i2cPort;
    if (device == NULL || msgs == NULL) {
        HDF_LOGE("%s: device or  msgs is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    i2cPort = (uint32_t)device->port;
    if (i2cPort > HAL_I2C_ID_NUM) {
        HDF_LOGE("i2c port %u not support\r\n", i2cPort);
        return HDF_ERR_NOT_SUPPORT;
    }
    if (HDF_SUCCESS != OsalMutexLock(&device->mutex)) {
        HDF_LOGE("%s %d OsalMutexTimedLock fail\r\n", __func__, __LINE__);
        return HDF_ERR_TIMEOUT;
    }
    for (int32_t i = 0; i < count; i++) {
        msg = &msgs[i];
        if (msg->flags == I2C_FLAG_READ) {
            ret = hal_i2c_task_recv(i2cPort, msg->addr, msg->buf, 0, msg->buf, msg->len, 0, NULL);
            if (ret) {
                HDF_LOGE("%s:%d,i2c recev fail, dev_addr = 0x%x, ret = %d\r\n", __func__, __LINE__, msg->addr, ret);
                OsalMutexUnlock(&device->mutex);
                return i;
            }
        } else if (msg->flags == I2C_FLAG_STOP) {
            i++;
            msg2 = &msgs[i];
            ret = hal_i2c_task_recv(i2cPort, msg->addr, msg->buf, msg->len, msg2->buf, msg2->len, 0, NULL);
            if (ret) {
                HDF_LOGE("%s:%d,i2c recev fail, dev_addr = 0x%x, ret = %d\r\n", __func__, __LINE__, msg->addr, ret);
                OsalMutexUnlock(&device->mutex);
                return i;
            }
        } else {
            ret = hal_i2c_task_send(i2cPort, msg->addr, msg->buf, msg->len, 0, NULL);
            if (ret) {
                HDF_LOGE("%s:%d,i2c send fail, dev_addr = 0x%x, ret = %d\r\n", __func__, __LINE__, msg->addr, ret);
                OsalMutexUnlock(&device->mutex);
                return i;
            }
        }
    }
    OsalMutexUnlock(&device->mutex);
    return count;
}

static int32_t i2cHostTransfer(struct I2cCntlr *cntlr, struct I2cMsg *msgs, int16_t count)
{
    struct I2cDevice *device = NULL;
    if (cntlr == NULL || msgs == NULL || cntlr->priv == NULL) {
        HDF_LOGE("%s: I2cCntlr or msgs is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    device = (struct I2cDevice *)cntlr->priv;
    if (device == NULL) {
        HDF_LOGE("%s: I2cDevice is NULL\r\n", __func__);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    return i2c_transfer(device, msgs, count);
}
