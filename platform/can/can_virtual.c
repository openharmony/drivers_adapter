/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 *
 * This file is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <securec.h>

#include "can/can_core.h"
#include "device_resource_if.h"
#include "hdf_log.h"
#include "osal_mem.h"

#define HDF_LOG_TAG             can_virtual_c
#define CAN_VIRTUAL_BUS_NUM_DFT 31

struct VirtualCanCntlr {
    struct CanCntlr cntlr;
    uint8_t workMode;
    uint32_t bitRate;
    uint32_t syncJumpWidth;
    uint32_t timeSeg1;
    uint32_t timeSeg2;
    uint32_t prescaler;
    int32_t busState;
};

enum VIRTUAL_CAN_SPEED {
    CAN_SPEED_1M = 1000UL * 1000,  /* 1 MBit/sec   */
    CAN_SPEED_800K = 1000UL * 800, /* 800 kBit/sec */
    CAN_SPEED_500K = 1000UL * 500, /* 500 kBit/sec */
    CAN_SPEED_250K = 1000UL * 250, /* 250 kBit/sec */
    CAN_SPEED_125K = 1000UL * 125, /* 125 kBit/sec */
    CAN_SPEED_100K = 1000UL * 100, /* 100 kBit/sec */
    CAN_SPEED_50K = 1000UL * 50,   /* 50 kBit/sec  */
    CAN_SPEED_20K = 1000UL * 20,   /* 20 kBit/sec  */
    CAN_SPEED_10K = 1000UL * 10    /* 10 kBit/sec  */
};

static int32_t VirtualReadConfigFromHcs(struct VirtualCanCntlr *virtualCan)
{
    uint32_t value;
    const struct DeviceResourceNode *node = NULL;
    struct DeviceResourceIface *iface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);

    if (virtualCan == NULL) {
        HDF_LOGE("%s: virtual cntlr is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    node = PlatformDeviceGetDrs(&virtualCan->cntlr.device);
    if (node == NULL) {
        HDF_LOGE("%s: properity node null", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (iface == NULL || iface->GetUint32 == NULL) {
        HDF_LOGE("%s: face is invalid", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    if (iface->GetUint32(node, "bus_num", &value, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read bus number failed", __func__);
        return HDF_FAILURE;
    }
    virtualCan->cntlr.number = (int32_t)value;

    if (iface->GetUint8(node, "work_mode", &virtualCan->workMode, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read work mode failed", __func__);
        return HDF_FAILURE;
    }

    if (iface->GetUint32(node, "bit_rate", &virtualCan->bitRate, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read bit reate failed", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t VirtualCanMsgLoopBack(struct VirtualCanCntlr *virtualCan, const struct CanMsg *msg)
{
    struct CanMsg *new = NULL;

    new = CanMsgObtain();
    if (new == NULL) {
        return HDF_ERR_MALLOC_FAIL;
    }

    *new = *msg; // Yeah! this is loop back ...

    virtualCan->busState = CAN_BUS_READY;
    return CanCntlrOnNewMsg(&virtualCan->cntlr, new);
}

static int32_t VirtualCanSendMsg(struct CanCntlr *cntlr, const struct CanMsg *msg)
{
    struct VirtualCanCntlr *virtualCan = NULL;

    if (cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }

    if (msg == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    virtualCan = (struct VirtualCanCntlr *)cntlr->device.priv;
    if (virtualCan == NULL) {
        HDF_LOGE("%s: private data is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    virtualCan->busState = CAN_BUS_BUSY;

    return VirtualCanMsgLoopBack(virtualCan, msg);
}

struct VirtualSpeedConfigMap {
    uint32_t speed;
    uint32_t sjw;
    uint32_t tsg1;
    uint32_t tsg2;
    uint32_t prescaler;
};

#define CAN_CLK 80M

#define CAN_SJW_2TQ  2
#define CAN_BS1_5TQ  5
#define CAN_BS1_7TQ  7
#define CAN_BS1_13TQ 13
#define CAN_BS1_14TQ 14
#define CAN_BS2_2TQ  2
#define CAN_BS2_5TQ  5

/*
 * S(bit rate) = 1 / ((1 + BS1 + BS2) * TQ)
 * TQ = Prescaler / Fclk
 * S(bit rate) = Fclk / (Prescaler * (1 + BS1 + BS2))
 */

static const struct VirtualSpeedConfigMap g_speedMaps[] = {
    {CAN_SPEED_1M,   CAN_SJW_2TQ, CAN_BS1_5TQ,  CAN_BS2_2TQ, 10 },
    {CAN_SPEED_800K, CAN_SJW_2TQ, CAN_BS1_14TQ, CAN_BS2_5TQ, 5  },
    {CAN_SPEED_500K, CAN_SJW_2TQ, CAN_BS1_7TQ,  CAN_BS2_2TQ, 16 },
    {CAN_SPEED_250K, CAN_SJW_2TQ, CAN_BS1_13TQ, CAN_BS2_2TQ, 20 },
    {CAN_SPEED_125K, CAN_SJW_2TQ, CAN_BS1_13TQ, CAN_BS2_2TQ, 40 },
    {CAN_SPEED_100K, CAN_SJW_2TQ, CAN_BS1_13TQ, CAN_BS2_2TQ, 50 },
    {CAN_SPEED_50K,  CAN_SJW_2TQ, CAN_BS1_13TQ, CAN_BS2_2TQ, 100},
    {CAN_SPEED_20K,  CAN_SJW_2TQ, CAN_BS1_13TQ, CAN_BS2_2TQ, 250},
    {CAN_SPEED_10K,  CAN_SJW_2TQ, CAN_BS1_13TQ, CAN_BS2_2TQ, 500}
};

static int32_t VirtualCanSetBitRate(struct VirtualCanCntlr *virtualCan, uint32_t speed)
{
    int32_t i;
    const struct VirtualSpeedConfigMap *cfgMap;

    for (i = 0; i < sizeof(g_speedMaps) / sizeof(g_speedMaps[0]); i++) {
        if (g_speedMaps[i].speed == speed) {
            break;
        }
    }

    if (i >= (sizeof(g_speedMaps) / sizeof(g_speedMaps[0]))) {
        HDF_LOGE("%s: speed: %u not support", __func__, speed);
        return HDF_ERR_NOT_SUPPORT;
    }

    cfgMap = &g_speedMaps[i];
    virtualCan->syncJumpWidth = cfgMap->sjw;
    virtualCan->timeSeg1 = cfgMap->tsg1;
    virtualCan->timeSeg2 = cfgMap->tsg2;
    virtualCan->prescaler = cfgMap->prescaler;
    virtualCan->bitRate = speed;
    return HDF_SUCCESS;
}

static int32_t VirtualCanSetMode(struct VirtualCanCntlr *virtualCan, int32_t mode)
{
    switch (mode) {
        case CAN_BUS_LOOPBACK:
            virtualCan->workMode = CAN_BUS_LOOPBACK;
            break;
        default:
            return HDF_ERR_NOT_SUPPORT;
    }
    return HDF_SUCCESS;
}

static int32_t VirtualCanSetCfg(struct CanCntlr *cntlr, const struct CanConfig *cfg)
{
    int32_t ret;
    struct VirtualCanCntlr *virtualCan = NULL;

    virtualCan = (struct VirtualCanCntlr *)cntlr->device.priv;
    if (virtualCan == NULL) {
        HDF_LOGE("%s: private data is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    virtualCan->busState = CAN_BUS_RESET;
    if ((ret = VirtualCanSetBitRate(virtualCan, cfg->speed) != HDF_SUCCESS)) {
        HDF_LOGE("%s: set speed failed", __func__);
        return ret;
    }

    if ((ret = VirtualCanSetMode(virtualCan, cfg->mode) != HDF_SUCCESS)) {
        HDF_LOGE("%s: set mode failed", __func__);
        return ret;
    }

    virtualCan->busState = CAN_BUS_READY;
    return HDF_SUCCESS;
}

static int32_t VirtualCanGetCfg(struct CanCntlr *cntlr, struct CanConfig *cfg)
{
    struct VirtualCanCntlr *virtualCan = NULL;

    virtualCan = (struct VirtualCanCntlr *)cntlr->device.priv;
    if (virtualCan == NULL) {
        HDF_LOGE("%s: private data is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    cfg->speed = virtualCan->bitRate;
    cfg->mode = virtualCan->workMode;
    return HDF_SUCCESS;
}

struct CanCntlrMethod g_virtualCanMethod = {
    .sendMsg = VirtualCanSendMsg,
    .setCfg = VirtualCanSetCfg,
    .getCfg = VirtualCanGetCfg,
};

/*
 * ! this function will not be invoked when polciy is 0
 * ! no need to make any changes here
 */
static int32_t HdfVirtualCanBind(struct HdfDeviceObject *device)
{
    return CanServiceBind(device);
}

static void VirtualCanSetDefault(struct VirtualCanCntlr *virtualCan)
{
    virtualCan->cntlr.number = CAN_VIRTUAL_BUS_NUM_DFT;
    virtualCan->workMode = CAN_BUS_LOOPBACK;
    virtualCan->bitRate = CAN_SPEED_10K;
}

static int32_t VirtualCanInit(struct VirtualCanCntlr *virtualCan)
{
    int32_t ret;

    HDF_LOGI("%s: enter", __func__);
    if (virtualCan == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }

    VirtualCanSetDefault(virtualCan);

    ret = VirtualReadConfigFromHcs(virtualCan);
    if (ret != HDF_SUCCESS) {
        HDF_LOGW("VirtualUartInit: read hcs config failed");
    }

    if ((ret = VirtualCanSetBitRate(virtualCan, virtualCan->bitRate) != HDF_SUCCESS)) {
        HDF_LOGE("%s: set bit rate failed", __func__);
        return ret;
    }

    if ((ret = VirtualCanSetMode(virtualCan, virtualCan->workMode) != HDF_SUCCESS)) {
        HDF_LOGE("%s: set mode failed", __func__);
        return ret;
    }

    return HDF_SUCCESS;
}

static int32_t HdfVirtualCanInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct VirtualCanCntlr *virtualCan = NULL;

    HDF_LOGI("%s: entry", __func__);
    if (device == NULL) {
        HDF_LOGE("%s: device is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    virtualCan = (struct VirtualCanCntlr *)OsalMemCalloc(sizeof(*virtualCan));
    if (virtualCan == NULL) {
        return HDF_ERR_MALLOC_FAIL;
    }

    virtualCan->busState = CAN_BUS_RESET;
    ret = VirtualCanInit(virtualCan);
    if (ret != HDF_SUCCESS) {
        OsalMemFree(virtualCan);
        HDF_LOGE("%s: can init error: %d", __func__, ret);
        return ret;
    }
    virtualCan->busState = CAN_BUS_READY;

    virtualCan->cntlr.ops = &g_virtualCanMethod;
    virtualCan->cntlr.device.priv = virtualCan;
    ret = CanCntlrAdd(&virtualCan->cntlr);
    if (ret != HDF_SUCCESS) {
        OsalMemFree(virtualCan);
        HDF_LOGE("%s: add cntlr failed: %d", __func__, ret);
        return ret;
    }

    ret = CanCntlrSetHdfDev(&virtualCan->cntlr, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGW("%s: can controller attach failed:%d", __func__, ret);
    }

    return HDF_SUCCESS;
}

static int32_t VirtualCanDeinit(struct VirtualCanCntlr *virtualCan)
{
    (void)virtualCan;
    return HDF_SUCCESS;
}

static void HdfVirtualCanRelease(struct HdfDeviceObject *device)
{
    struct VirtualCanCntlr *virtualCan = NULL;

    HDF_LOGI("%s: entry", __func__);
    if (device == NULL) {
        HDF_LOGE("%s: device is null", __func__);
        return;
    }

    virtualCan = (struct VirtualCanCntlr *)CanCntlrFromHdfDev(device);
    if (virtualCan == NULL) {
        HDF_LOGE("%s: platform device not bind", __func__);
        return;
    }

    CanServiceRelease(device);
    (void)CanCntlrDel(&virtualCan->cntlr);
    VirtualCanDeinit(virtualCan);
    OsalMemFree(virtualCan);
}

static struct HdfDriverEntry g_hdfCanDriver = {
    .moduleVersion = 1,
    .moduleName = "can_driver_virtual",
    .Bind = HdfVirtualCanBind,
    .Init = HdfVirtualCanInit,
    .Release = HdfVirtualCanRelease,
};

struct HdfDriverEntry *CanVirtualGetEntry(void)
{
    return &g_hdfCanDriver;
}

HDF_INIT(g_hdfCanDriver);
