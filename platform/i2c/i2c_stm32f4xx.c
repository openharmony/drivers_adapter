/*
 * Copyright (c) 2022 Talkweb Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <stdlib.h>
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#include "hcs_macro.h"
#include "hdf_config_macro.h"
#else
#include "device_resource_if.h"
#endif
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "i2c_core.h"
#include "i2c_if.h"
#include "osal_mutex.h"
#include "hdf_base_hal.h"
#include "stm32f4xx_ll_i2c.h"

#define HDF_LOG_TAG "hdf_i2c"

typedef enum {
    I2C_HANDLE_NULL = 0,
    I2C_HANDLE_1 = 1,
    I2C_HANDLE_2 = 2,
    I2C_HANDLE_3 = 3,
    I2C_HANDLE_MAX = I2C_HANDLE_3
} I2C_HANDLE;

struct RealI2cResource {
    uint8_t port;
    uint8_t devMode;
    uint32_t devAddr;
    uint32_t speed;
    struct OsalMutex mutex;
};

static bool g_I2cEnableFlg[I2C_HANDLE_MAX] = {0};

static void HdfI2cInit(I2C_HANDLE i2cx, unsigned int i2cRate, unsigned int addr);
static void HdfI2cWrite(I2C_HANDLE i2cx, unsigned char devAddr, unsigned char *buf, unsigned int len);
static void HdfI2cRead(I2C_HANDLE i2cx, unsigned char devAddr, unsigned char *buf, unsigned int len);

static int32_t I2cDriverBind(struct HdfDeviceObject *device);
static int32_t I2cDriverInit(struct HdfDeviceObject *device);
static void I2cDriverRelease(struct HdfDeviceObject *device);
static int32_t I2cDataTransfer(struct I2cCntlr *cntlr, struct I2cMsg *msgs, int16_t count);

struct HdfDriverEntry gI2cHdfDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "HDF_I2C",
    .Bind = I2cDriverBind,
    .Init = I2cDriverInit,
    .Release = I2cDriverRelease,
};
HDF_INIT(gI2cHdfDriverEntry);

struct I2cMethod gI2cHostMethod = {
    .transfer = I2cDataTransfer,
};

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#define I2C_FIND_CONFIG(node, name, resource) \
    do { \
        if (strcmp(HCS_PROP(node, match_attr), name) == 0) { \
            resource->port = HCS_PROP(node, port); \
            resource->devMode = HCS_PROP(node, devMode); \
            resource->devAddr = HCS_PROP(node, devAddr); \
            resource->speed = HCS_PROP(node, speed); \
            result = HDF_SUCCESS; \
        } \
    } while (0)
#define PLATFORM_CONFIG HCS_NODE(HCS_ROOT, platform)
#define PLATFORM_I2C_CONFIG HCS_NODE(HCS_NODE(HCS_ROOT, platform), i2c_config)
static uint32_t GetI2cDeviceResource(struct RealI2cResource *i2cResource, const char *deviceMatchAttr)
{
    int32_t result = HDF_FAILURE;
    struct RealI2cResource *resource = NULL;
    if (i2cResource == NULL || deviceMatchAttr == NULL) {
        HDF_LOGE("device or deviceMatchAttr is NULL\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    resource = i2cResource;
#if HCS_NODE_HAS_PROP(PLATFORM_CONFIG, i2c_config)
    HCS_FOREACH_CHILD_VARGS(PLATFORM_I2C_CONFIG, I2C_FIND_CONFIG, deviceMatchAttr, resource);
#endif
    if (result != HDF_SUCCESS) {
        HDF_LOGE("resourceNode %s is NULL\r\n", deviceMatchAttr);
    } else {
        HdfI2cInit(i2cResource->port, i2cResource->speed, i2cResource->devAddr);
    }
    return result;
}
#else
static int32_t GetI2cDeviceResource(struct RealI2cResource *i2cResource, const struct DeviceResourceNode *resourceNode)
{
    if (i2cResource == NULL || resourceNode == NULL) {
        HDF_LOGE("[%s]: param is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct DeviceResourceIface *dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint8 == NULL || dri->GetUint32 == NULL || dri->GetUint32Array == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (dri->GetUint8(resourceNode, "port", &i2cResource->port, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config port fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "devMode", &i2cResource->devMode, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config devMode fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "devAddr", &i2cResource->devAddr, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config devAddr fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "speed", &i2cResource->speed, 0) != HDF_SUCCESS) {
        HDF_LOGE("i2c config speed fail\r\n");
        return HDF_FAILURE;
    }

    HdfI2cInit(i2cResource->port, i2cResource->speed, i2cResource->devAddr);

    return HDF_SUCCESS;
}
#endif

static int32_t AttachI2cDevice(struct I2cCntlr *host, struct HdfDeviceObject *device)
{
    int32_t ret = HDF_FAILURE;

    if (host == NULL || device == NULL) {
        HDF_LOGE("[%s]: param is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct RealI2cResource *i2cResource = (struct RealI2cResource *)OsalMemAlloc(sizeof(struct RealI2cResource));
    if (i2cResource == NULL) {
        HDF_LOGE("[%s]: OsalMemAlloc RealI2cResource fail\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    memset_s(i2cResource, sizeof(struct RealI2cResource), 0, sizeof(struct RealI2cResource));
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    ret = GetI2cDeviceResource(i2cResource, device->deviceMatchAttr);
#else
    ret = GetI2cDeviceResource(i2cResource, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        OsalMemFree(i2cResource);
        return HDF_FAILURE;
    }

    host->busId = i2cResource->port;
    host->priv = i2cResource;

    return HDF_SUCCESS;
}

static int32_t I2cDataTransfer(struct I2cCntlr *cntlr, struct I2cMsg *msgs, int16_t count)
{
    if (cntlr == NULL || msgs == NULL || cntlr->priv == NULL) {
        HDF_LOGE("[%s]: I2cDataTransfer param is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (count <= 0) {
        HDF_LOGE("[%s]: I2cDataTransfer count err\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct RealI2cResource *device = (struct I2cDevice *)cntlr->priv;
    if (device == NULL) {
        HDF_LOGE("%s: I2cDevice is NULL\r\n", __func__);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    struct I2cMsg *msg = NULL;
    if (HDF_SUCCESS != OsalMutexLock(&device->mutex)) {
        HDF_LOGE("[%s]: OsalMutexLock fail\r\n", __func__);
        return HDF_ERR_TIMEOUT;
    }

    for (int32_t i = 0; i < count; i++) {
        msg = &msgs[i];
        if (msg->flags == I2C_FLAG_READ) {
            HdfI2cRead(device->port, msg->addr, msg->buf, msg->len);
        } else {
            HdfI2cWrite(device->port, msg->addr, msg->buf, msg->len);
        }
    }
    OsalMutexUnlock(&device->mutex);

    return count;
}

static int32_t I2cDriverBind(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        HDF_LOGE("[%s]: I2c device is NULL\r\n", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t I2cDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret = HDF_FAILURE;
    struct I2cCntlr *host = NULL;
    if (device == NULL) {
        HDF_LOGE("[%s]: I2c device is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    host = (struct I2cCntlr *)OsalMemAlloc(sizeof(struct I2cCntlr));
    if (host == NULL) {
        HDF_LOGE("[%s]: malloc host is NULL\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    memset_s(host, sizeof(struct I2cCntlr), 0, sizeof(struct I2cCntlr));
    host->ops = &gI2cHostMethod;
    device->priv = (void *)host;

    ret = AttachI2cDevice(host, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("[%s]: AttachI2cDevice error, ret = %d\r\n", __func__, ret);
        I2cDriverRelease(device);
        return HDF_DEV_ERR_ATTACHDEV_FAIL;
    }

    ret = I2cCntlrAdd(host);
    if (ret != HDF_SUCCESS) {
        I2cDriverRelease(device);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void I2cDriverRelease(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return;
    }

    struct I2cCntlr *i2cCntrl = device->priv;
    if (i2cCntrl == NULL || i2cCntrl->priv == NULL) {
        HDF_LOGE("%s: i2cCntrl is NULL\r\n", __func__);
        return;
    }
    i2cCntrl->ops = NULL;
    struct RealI2cResource *i2cDevice = (struct I2cDevice *)i2cCntrl->priv;
    OsalMemFree(i2cCntrl);

    if (i2cDevice != NULL) {
        OsalMutexDestroy(&i2cDevice->mutex);
        OsalMemFree(i2cDevice);
    }
}

static I2C_TypeDef *GetLLI2cHandlerMatch(I2C_HANDLE i2cx)
{
    if (i2cx > I2C_HANDLE_MAX) {
        printf("ERR: GetLLI2cClkMatch fail, param match fail\r\n");
        return NULL;
    }

    switch (i2cx) {
        case I2C_HANDLE_1:
            return (I2C_TypeDef *)I2C1;
        case I2C_HANDLE_2:
            return (I2C_TypeDef *)I2C2;
        case I2C_HANDLE_3:
            return (I2C_TypeDef *)I2C3;
        default:
            printf("ERR: GetLLI2cClkMatch fail, handler match fail\r\n");
            return NULL;
    }
}

static bool EnableLLI2cClock(I2C_TypeDef *i2cx)
{
    if (i2cx == I2C1) {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
        return true;
    } else if (i2cx == I2C2) {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
        return true;
    } else if (i2cx == I2C3) {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3);
        return true;
    } else {
        printf("EnableI2cClock fail, i2cx match fail\r\n");
        return false;
    }
}

static void HdfI2cInit(I2C_HANDLE i2cx, unsigned int i2cRate, unsigned int addr)
{
    LL_I2C_InitTypeDef I2C_InitStruct = {0};
    I2C_TypeDef *myI2c = GetLLI2cHandlerMatch(i2cx);
    if (myI2c == NULL) {
        return;
    }

    EnableLLI2cClock(myI2c);
    LL_I2C_DisableOwnAddress2(myI2c);
    LL_I2C_DisableGeneralCall(myI2c);
    LL_I2C_EnableClockStretching(myI2c);
    I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
    I2C_InitStruct.ClockSpeed = i2cRate;
    I2C_InitStruct.DutyCycle = LL_I2C_DUTYCYCLE_2;
    I2C_InitStruct.OwnAddress1 = addr;
    I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
    I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
    LL_I2C_Init(myI2c, &I2C_InitStruct);
    LL_I2C_SetOwnAddress2(myI2c, 0);

    g_I2cEnableFlg[i2cx] = true;
}

static void HdfI2cWrite(I2C_HANDLE i2cx, unsigned char devAddr, unsigned char *buf, unsigned int len)
{
    if (g_I2cEnableFlg[i2cx] != true) {
        printf("I2C_WriteByte err, Please initialize first!");
        return;
    }

    I2C_TypeDef *myI2c = GetLLI2cHandlerMatch(i2cx);
    if (myI2c == NULL) {
        return;
    }

    while (LL_I2C_IsActiveFlag_BUSY(myI2c));

    LL_I2C_GenerateStartCondition(myI2c);
    while (LL_I2C_IsActiveFlag_SB(myI2c) == RESET);

    LL_I2C_TransmitData8(myI2c, (devAddr << 1));
    while (LL_I2C_IsActiveFlag_TXE(myI2c) == RESET);

    LL_I2C_ClearFlag_ADDR(myI2c);
    while (LL_I2C_IsActiveFlag_TXE(myI2c) == RESET);

    for (unsigned int i = 0; i < len; i++) {
        LL_I2C_TransmitData8(myI2c, buf[i]);
        while (LL_I2C_IsActiveFlag_TXE(myI2c) == RESET);
    }

    LL_I2C_GenerateStopCondition(myI2c);
}

static void HdfI2cRead(I2C_HANDLE i2cx, unsigned char devAddr, unsigned char *buf, unsigned int len)
{
    if (g_I2cEnableFlg[i2cx] != true) {
        printf("I2C_ReadByte err, Please initialize first!");
        return;
    }

    I2C_TypeDef *myI2c = GetLLI2cHandlerMatch(i2cx);
    if (myI2c == NULL) {
        return;
    }

    while (LL_I2C_IsActiveFlag_BUSY(myI2c));

    LL_I2C_GenerateStartCondition(myI2c);
    while (LL_I2C_IsActiveFlag_SB(myI2c) == RESET);

    LL_I2C_TransmitData8(myI2c, ((devAddr << 1) | 1));
    while ((LL_I2C_IsActiveFlag_ADDR(myI2c) == RESET) || (LL_I2C_IsActiveFlag_MSL(myI2c) == RESET) ||
        (LL_I2C_IsActiveFlag_BUSY(myI2c) == RESET));

    for (unsigned int i = 0; i < len; i++) {
        if (i < len - 1) {
            LL_I2C_AcknowledgeNextData(myI2c, LL_I2C_ACK);
        } else {
            LL_I2C_AcknowledgeNextData(myI2c, LL_I2C_NACK);
        }
        while (LL_I2C_IsActiveFlag_RXNE(myI2c) == RESET);
        buf[i] = LL_I2C_ReceiveData8(myI2c);
    }
    LL_I2C_GenerateStopCondition(myI2c);
}