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
#include "uart_bes.h"
#include <stdlib.h>
#include <string.h>
#include "hal_iomux.h"
#include "hal_timer.h"
#include "device_resource_if.h"
#include "hal_trace.h"
#include "hal_cache.h"
#include "hdf_log.h"

#define HDF_UART_TMO 1000
#define HDF_LOG_TAG uartDev

#define UART_FIFO_MAX_BUFFER 2048
#define UART_DMA_RING_BUFFER_SIZE 256 // mast be 2^n

static __SRAMBSS unsigned char g_halUartBuf[UART_DMA_RING_BUFFER_SIZE];
static __SRAMBSS unsigned char g_halUart1Buf[UART_DMA_RING_BUFFER_SIZE];
static __SRAMBSS unsigned char g_halUart2Buf[UART_DMA_RING_BUFFER_SIZE];

static struct UART_CTX_OBJ g_uartCtx[4] = {0};
static unsigned char *g_uartKfifoBuffer[4] = {NULL, NULL, NULL, NULL};
struct HAL_UART_CFG_T g_lowUartCfg = {
    // used for tgdb cli console
    .parity = HAL_UART_PARITY_NONE,
    .stop = HAL_UART_STOP_BITS_1,
    .data = HAL_UART_DATA_BITS_8,
    .flow = HAL_UART_FLOW_CONTROL_NONE,
    .tx_level = HAL_UART_FIFO_LEVEL_7_8,
    .rx_level = HAL_UART_FIFO_LEVEL_1_8,
    .baud = 0,
    .dma_rx = false,
    .dma_tx = false,
    .dma_rx_stop_on_err = false,
};

static void HalSetUartIomux(enum HAL_UART_ID_T uartId)
{
    if (uartId == HAL_UART_ID_0) {
        hal_iomux_set_uart0();
    } else if (uartId == HAL_UART_ID_1) {
        hal_iomux_set_uart1();
    } else if (uartId == HAL_UART_ID_2) {
        hal_iomux_set_uart2();
    } else {
        hal_iomux_set_uart3();
    }
}

static void HalUartRxStart(uint32_t uartId)
{
    struct HAL_DMA_DESC_T dmaDescRx;
    unsigned int descCnt = 1;
    union HAL_UART_IRQ_T mask;

    mask.reg = 0;
    mask.BE = 0;
    mask.FE = 0;
    mask.OE = 0;
    mask.PE = 0;
    mask.RT = 1;

    hal_uart_dma_recv_mask(uartId, g_uartCtx[uartId].buffer, UART_DMA_RING_BUFFER_SIZE, &dmaDescRx, &descCnt, &mask);
}

static void UartRxHandler(enum HAL_UART_ID_T id, union HAL_UART_IRQ_T status)
{
    int32_t ret;

    if (status.TX) {
        ret = OsalSemPost(&g_uartCtx[id].txSem);
        ASSERT(ret == HDF_SUCCESS, "%s: Failed to release write_sem: %d", __func__, ret);
    }

    if (status.RX || status.RT) {
        ret = OsalSemPost(&g_uartCtx[id].rxSem);
        ASSERT(ret == HDF_SUCCESS, "%s: Failed to release read_sem: %d", __func__, ret);
    }
}

static void UartDmaRxHandler(uint32_t xferSize, int dmaError, union HAL_UART_IRQ_T status)
{
    uint32_t len = 0;
    uint32_t uartid = 0;

    len = kfifo_put(&g_uartCtx[uartid].fifo, g_uartCtx[uartid].buffer, xferSize);
    if (len < xferSize) {
        HDF_LOGE("%s ringbuf is full have %d need %d\r", __FUNCTION__, (int)len, (int)xferSize);
        return;
    }

    memset_s(g_uartCtx[uartid].buffer, UART_DMA_RING_BUFFER_SIZE, 0, UART_DMA_RING_BUFFER_SIZE);
    OsalSemPost(&g_uartCtx[uartid].rxSem);
    HalUartRxStart(uartid);
}

static void UartDmaTxHandler(uint32_t xferSize, int dmaError)
{
    OsalSemPost(&g_uartCtx[0].txSem);
}

static void Uart1DmaRxHandler(uint32_t xferSize, int dmaError, union HAL_UART_IRQ_T status)
{
    uint32_t len = 0;
    uint32_t uartid = HAL_UART_ID_1;
    len = kfifo_put(&g_uartCtx[uartid].fifo, g_uartCtx[uartid].buffer, xferSize);
    if (len < xferSize) {
        HDF_LOGE("%s ringbuf is full have %d need %d\r", __FUNCTION__, (int)len, (int)xferSize);
        return;
    }
    memset_s(g_uartCtx[uartid].buffer, UART_DMA_RING_BUFFER_SIZE, 0, UART_DMA_RING_BUFFER_SIZE);
    OsalSemPost(&g_uartCtx[uartid].rxSem);
    HalUartRxStart(uartid);
}

static void Uart1DmaTxHandler(uint32_t xferSize, int dmaError)
{
    OsalSemPost(&g_uartCtx[HAL_UART_ID_1].txSem);
}

/* uart2 */
static void Uart2DmaRxHandler(uint32_t xferSize, int dmaError, union HAL_UART_IRQ_T status)
{
    uint32_t len = 0;
    uint32_t uartid = HAL_UART_ID_2;
    len = kfifo_put(&g_uartCtx[uartid].fifo, g_uartCtx[uartid].buffer, xferSize);
    if (len < xferSize) {
        HDF_LOGE("%s ringbuf is full have %d need %d\r", __FUNCTION__, (int)len, (int)xferSize);
        return;
    }

    memset_s(g_uartCtx[uartid].buffer, UART_DMA_RING_BUFFER_SIZE, 0, UART_DMA_RING_BUFFER_SIZE);
    OsalSemPost(&g_uartCtx[uartid].rxSem);
    HalUartRxStart(uartid);
}

static void Uart2DmaTxHandler(uint32_t xferSize, int dmaError)
{
    OsalSemPost(&g_uartCtx[HAL_UART_ID_2].txSem);
}

static void HalUartStartRx(uint32_t uartId)
{
    union HAL_UART_IRQ_T mask;
    mask.reg = 0;
    mask.RT = 1;
    mask.RX = 1;

    hal_uart_irq_set_mask(uartId, mask);
    hal_uart_irq_set_handler(uartId, UartRxHandler);
}

static int32_t HalUartSend(uint32_t uartId, const void *data, uint32_t size, uint32_t timeOut)
{
    int32_t ret = HDF_FAILURE;
    struct HAL_DMA_DESC_T dmaSescTx;
    unsigned int descCnt = 1;

    if (data == NULL || size == 0) {
        HDF_LOGE("%s %d Invalid input \r\n", __FILE__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (uartId > HAL_UART_ID_2) {
        HDF_LOGE("%s %d Invalid input \r\n", __FILE__, __LINE__);
        return HDF_ERR_NOT_SUPPORT;
    }

    hal_uart_dma_send(uartId, data, size, &dmaSescTx, &descCnt);
    OsalSemWait(&g_uartCtx[uartId].txSem, timeOut);

    return HDF_SUCCESS;
}

static int32_t HalUartRecv(uint8_t uartId, void *data, uint32_t expectSize,
                            uint32_t *recvSize, uint32_t timeOut)
{
    int32_t ret = HDF_FAILURE;
    uint32_t beginTime = 0;
    uint32_t nowTime = 0;
    uint32_t fifoPopLen = 0;
    uint32_t recvedLen = 0;
    uint32_t expectLen = expectSize;

    if (data == NULL || expectSize == 0 || recvSize == NULL) {
        HDF_LOGE("%s %d Invalid input \r\n", __FILE__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (uartId > HAL_UART_ID_2) {
        HDF_LOGE("%s %d Invalid input \r\n", __FILE__, __LINE__);
        return HDF_ERR_NOT_SUPPORT;
    }
    beginTime = TICKS_TO_MS(hal_sys_timer_get());
    do {
        fifoPopLen = kfifo_get(&g_uartCtx[uartId].fifo, (uint8_t *)data + recvedLen, expectLen);
        recvedLen += fifoPopLen;
        expectLen -= fifoPopLen;
        if (recvedLen >= expectSize) {
            break;
        }
        /* haven't get any data from fifo */
        if (recvedLen == 0) {
            break;
        }
        /* if reaches here, it means need to wait for more data come */
        OsalSemWait(&g_uartCtx[uartId].rxSem, timeOut);
        /* time out break */
        nowTime = TICKS_TO_MS(hal_sys_timer_get());
        if ((uint32_t)(nowTime - beginTime) >= timeOut) {
            break;
        }
    } while (1);

    if (recvSize != NULL) {
        *recvSize = recvedLen;
    }

    return HDF_SUCCESS;
}

static void HalUartHandlerInit(struct UartDevice *device)
{
    uint32_t uartId;
    if (device == NULL) {
        HDF_LOGE("%s: INVALID PARAM", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    uartId = device->uartId;
    HDF_LOGI("%s %ld\r\n", __func__, uartId);
    if (uartId == HAL_UART_ID_0) {
        g_uartCtx[uartId].UartDmaRxHandler = UartDmaRxHandler;
        g_uartCtx[uartId].UartDmaTxHandler = UartDmaTxHandler;
        g_uartCtx[uartId].buffer = g_halUartBuf;
    }

    if (uartId == HAL_UART_ID_1) {
        g_uartCtx[uartId].UartDmaRxHandler = Uart1DmaRxHandler;
        g_uartCtx[uartId].UartDmaTxHandler = Uart1DmaTxHandler;
        g_uartCtx[uartId].buffer = g_halUart1Buf;
    }

    if (uartId == HAL_UART_ID_2) {
        g_uartCtx[uartId].UartDmaRxHandler = Uart2DmaRxHandler;
        g_uartCtx[uartId].UartDmaTxHandler = Uart2DmaTxHandler;
        g_uartCtx[uartId].buffer = g_halUart2Buf;
    }

    if (!g_uartKfifoBuffer[uartId]) {
        g_uartKfifoBuffer[uartId] = (char *)OsalMemAlloc(UART_FIFO_MAX_BUFFER);
        if (!g_uartKfifoBuffer[uartId]) {
            HDF_LOGE("kfifo OsalMemAlloc failed!");
            return;
        }
        kfifo_init(&g_uartCtx[uartId].fifo, g_uartKfifoBuffer[uartId], UART_FIFO_MAX_BUFFER);
    }

    OsalSemInit(&g_uartCtx[uartId].rxSem, 0);
    OsalSemInit(&g_uartCtx[uartId].txSem, 0);

    if (g_uartCtx[uartId].rxDMA) {
        HDF_LOGE("uart %ld start dma rx\r\n", uartId);
        hal_uart_irq_set_dma_handler(uartId, g_uartCtx[uartId].UartDmaRxHandler, g_uartCtx[uartId].UartDmaTxHandler);
        HalUartRxStart(uartId);
    } else {
        HalUartStartRx(uartId);
    }
}

static void UartStart(struct UartDevice *device)
{
    uint32_t uartId;
    struct HAL_UART_CFG_T *uartCfg = NULL;
    if (device == NULL) {
        HDF_LOGE("%s: INVALID PARAM", __func__);
        return;
    }
    uartId = device->uartId;
    uartCfg = &device->config;
    if (uartCfg == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return;
    }
    hal_uart_open(uartId, uartCfg);
    HDF_LOGI("%s %ld\r\n", __FUNCTION__, uartId);
    HalUartHandlerInit(device);
}

/* HdfDriverEntry method definitions */
static int32_t UartDriverBind(struct HdfDeviceObject *device);
static int32_t UartDriverInit(struct HdfDeviceObject *device);
static void UartDriverRelease(struct HdfDeviceObject *device);

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_UartDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "BES_UART_MODULE_HDF",
    .Bind = UartDriverBind,
    .Init = UartDriverInit,
    .Release = UartDriverRelease,
};

/* Initialize HdfDriverEntry */
HDF_INIT(g_UartDriverEntry);

/* UartHostMethod method definitions */
static int32_t UartHostDevInit(struct UartHost *host);
static int32_t UartHostDevDeinit(struct UartHost *host);
static int32_t UartHostDevWrite(struct UartHost *host, uint8_t *data, uint32_t size);
static int32_t UartHostDevSetBaud(struct UartHost *host, uint32_t baudRate);
static int32_t UartHostDevGetBaud(struct UartHost *host, uint32_t *baudRate);
static int32_t UartHostDevRead(struct UartHost *host, uint8_t *data, uint32_t size);
static int32_t UartHostDevSetAttribute(struct UartHost *host, struct UartAttribute *attribute);
static int32_t UartHostDevGetAttribute(struct UartHost *host, struct UartAttribute *attribute);
static int32_t UartHostDevSetTransMode(struct UartHost *host, enum UartTransMode mode);

/* UartHostMethod definitions */
struct UartHostMethod g_uartHostMethod = {
    .Init = UartHostDevInit,
    .Deinit = UartHostDevDeinit,
    .Read = UartHostDevRead,
    .Write = UartHostDevWrite,
    .SetBaud = UartHostDevSetBaud,
    .GetBaud = UartHostDevGetBaud,
    .SetAttribute = UartHostDevSetAttribute,
    .GetAttribute = UartHostDevGetAttribute,
    .SetTransMode = UartHostDevSetTransMode,
};

static int InitUartDevice(struct UartHost *host)
{
    HDF_LOGI("%s: Enter", __func__);
    struct UartDevice *uartDevice = NULL;
    struct HAL_UART_CFG_T *uartCfg = NULL;
    struct UartResource *resource = NULL;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    resource = &uartDevice->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartCfg = &uartDevice->config;
    if (uartCfg == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartDevice->uartId = resource->num;
    uartCfg->parity = resource->parity;
    uartCfg->stop = resource->stopBit;
    uartCfg->data = resource->wLen;
    uartCfg->flow = HAL_UART_FLOW_CONTROL_NONE;
    uartCfg->tx_level = HAL_UART_FIFO_LEVEL_1_2;
    uartCfg->rx_level = HAL_UART_FIFO_LEVEL_1_2;
    uartCfg->baud = resource->baudRate;
    uartCfg->dma_rx_stop_on_err = false;
    uartCfg->dma_rx = resource->rxDMA;
    uartCfg->dma_tx = resource->txDMA;

    g_uartCtx[uartDevice->uartId].txDMA = resource->txDMA;
    g_uartCtx[uartDevice->uartId].rxDMA = resource->rxDMA;

    if (!uartDevice->initFlag) {
        HDF_LOGE("uart %ld device init\r\n", uartDevice->uartId);
        HalSetUartIomux(uartDevice->uartId);
        UartStart(uartDevice);
        uartDevice->initFlag = true;
    }

    return HDF_SUCCESS;
}

static uint32_t GetUartDeviceResource(
    struct UartDevice *device, const struct DeviceResourceNode *resourceNode)
{
    struct DeviceResourceIface *dri = NULL;
    struct UartResource *resource = NULL;
    if (device == NULL || resourceNode == NULL) {
        HDF_LOGE("%s: INVALID PARAM", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid");
        return HDF_ERR_INVALID_PARAM;
    }
    if (dri->GetUint32(resourceNode, "num", &resource->num, 0) != HDF_SUCCESS) {
        HDF_LOGE("uart config read num fail");
        return HDF_FAILURE;
    }
    if (dri->GetUint32(resourceNode, "baudrate", &resource->baudRate, 0) != HDF_SUCCESS) {
        HDF_LOGE("uart config read baudrate fail");
        return HDF_FAILURE;
    }
    if (dri->GetUint32(resourceNode, "parity", &resource->parity, 0) != HDF_SUCCESS) {
        HDF_LOGE("uart config read parity fail");
        return HDF_FAILURE;
    }
    if (dri->GetUint32(resourceNode, "stopBit", &resource->stopBit, 0) != HDF_SUCCESS) {
        HDF_LOGE("uart config read stopBit fail");
        return HDF_FAILURE;
    }
    if (dri->GetUint32(resourceNode, "data", &resource->wLen, 0) != HDF_SUCCESS) {
        HDF_LOGE("uart config read data fail");
        return HDF_FAILURE;
    }

    resource->txDMA = dri->GetBool(resourceNode, "txDMA");
    resource->rxDMA = dri->GetBool(resourceNode, "rxDMA");

    // copy config
    device->uartId = resource->num;
    device->config.baud = resource->baudRate;
    device->config.parity = resource->parity;
    device->config.stop = resource->stopBit;
    device->config.data = resource->wLen;
    device->config.dma_rx = resource->rxDMA;
    device->config.dma_tx = resource->txDMA;
    return HDF_SUCCESS;
}

static int32_t AttachUartDevice(struct UartHost *uartHost, struct HdfDeviceObject *device)
{
    int32_t ret;
    struct UartDevice *uartDevice = NULL;

    if (uartHost == NULL || device == NULL || device->property == NULL) {
        HDF_LOGE("%s: property is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)OsalMemAlloc(sizeof(struct UartDevice));
    if (uartDevice == NULL) {
        HDF_LOGE("%s: OsalMemCalloc uartDevice error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = GetUartDeviceResource(uartDevice, device->property);
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(uartDevice);
        return HDF_FAILURE;
    }

    uartHost->priv = uartDevice;

    return InitUartDevice(uartHost);
}

static int32_t UartDriverBind(struct HdfDeviceObject *device)
{
    struct UartHost *devService;
    if (device == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    devService = (struct UartHost *)OsalMemAlloc(sizeof(*devService));
    if (devService == NULL) {
        HDF_LOGE("%s: OsalMemCalloc error", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    devService->device = device;
    device->service = &(devService->service);
    devService->priv = NULL;
    devService->method = NULL;
    return HDF_SUCCESS;
}

static void UartDriverRelease(struct HdfDeviceObject *device)
{
    HDF_LOGI("Enter %s:", __func__);
    uint32_t uartId;
    struct UartHost *host = NULL;
    struct UartDevice *uartDevice = NULL;
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return;
    }

    host = UartHostFromDevice(device);
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: host is NULL", __func__);
        return;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return;
    }
    uartId = uartDevice->uartId;
    host->method = NULL;

    OsalSemDestroy(&g_uartCtx[uartId].rxSem);
    OsalSemDestroy(&g_uartCtx[uartId].txSem);
    OsalMemFree(uartDevice);
    OsalMemFree(host);
}

static int32_t UartDriverInit(struct HdfDeviceObject *device)
{
    HDF_LOGI("Enter %s:", __func__);
    int32_t ret;
    struct UartHost *host = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    host = UartHostFromDevice(device);
    if (host == NULL) {
        HDF_LOGE("%s: host is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = AttachUartDevice(host, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: attach error", __func__);
        return HDF_FAILURE;
    }

    host->method = &g_uartHostMethod;

    return ret;
}

/* UartHostMethod implementations */
static int32_t UartHostDevInit(struct UartHost *host)
{
    HDF_LOGI("%s: Enter\r\n", __func__);
    if (host == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    InitUartDevice(host);
    return HDF_SUCCESS;
}

static int32_t UartHostDevDeinit(struct UartHost *host)
{
    HDF_LOGI("%s: Enter", __func__);
    uint32_t uartId;
    struct UartDevice *uartDevice = NULL;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;
    uartDevice->initFlag = false;

    hal_uart_close(uartId);

    return HDF_SUCCESS;
}

static int32_t UartHostDevWrite(struct UartHost *host, uint8_t *data, uint32_t size)
{
    struct UartDevice *device = NULL;
    uint32_t portId;

    if (host == NULL || data == NULL || size == 0 || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    device = (struct UartDevice *)host->priv;
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    portId = device->uartId;
    if (g_uartCtx[portId].txDMA) {
        HalUartSend(portId, data, size, HDF_UART_TMO);
    } else {
        for (uint32_t idx = 0; idx < size; idx++) {
            if (g_uartCtx[portId].isBlock) {
                hal_uart_blocked_putc(portId, data[idx]);
            } else {
                hal_uart_putc(portId, data[idx]);
            }
        }
    }

    return HDF_SUCCESS;
}

static int32_t UartHostDevRead(struct UartHost *host, uint8_t *data, uint32_t size)
{
    uint32_t recvSize = 0;
    int32_t ret;
    uint32_t uartId;
    struct UartDevice *uartDevice = NULL;
    if (host == NULL || data == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    uartId = uartDevice->uartId;
    if (g_uartCtx[uartId].rxDMA) {
        ret = HalUartRecv(uartId, data, size, &recvSize, HDF_UART_TMO);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("uart %ld recev error\r\n", uartId);
            return ret;
        }
        ret = recvSize;
    } else {
        if (g_uartCtx[uartId].isBlock) {
            data[0] = hal_uart_blocked_getc(uartId);
        } else {
            data[0] = hal_uart_getc(uartId);
        }
        ret = 1;
    }

    return ret;
}

static int32_t UartHostDevSetBaud(struct UartHost *host, uint32_t baudRate)
{
    HDF_LOGI("%s: Enter", __func__);
    struct UartDevice *uartDevice = NULL;
    struct HAL_UART_CFG_T *uartCfg = NULL;
    uint32_t uartId;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;

    uartCfg = &uartDevice->config;
    if (uartCfg == NULL) {
        HDF_LOGE("%s: device config is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartCfg->baud = baudRate;

    hal_uart_reopen(uartId, uartCfg);

    return HDF_SUCCESS;
}

static int32_t UartHostDevGetBaud(struct UartHost *host, uint32_t *baudRate)
{
    HDF_LOGI("%s: Enter", __func__);
    struct UartDevice *uartDevice = NULL;
    struct HAL_UART_CFG_T *uartCfg = NULL;
    uint32_t uartId;
    if (host == NULL || baudRate == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;
    uartCfg = &uartDevice->config;
    if (uartCfg == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    baudRate = &uartCfg->baud;
    if (baudRate == NULL) {
        HDF_LOGE("%s: baudRate is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    return HDF_SUCCESS;
}

static int32_t UartHostDevSetAttribute(struct UartHost *host, struct UartAttribute *attribute)
{
    HDF_LOGI("%s: Enter", __func__);
    struct UartDevice *uartDevice = NULL;
    struct HAL_UART_CFG_T *uartCfg = NULL;
    uint32_t uartId;
    if (host == NULL || attribute == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;
    uartCfg = &uartDevice->config;
    if (uartCfg == NULL) {
        HDF_LOGE("%s: config is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    switch (attribute->dataBits) {
        case UART_ATTR_DATABIT_8:
            uartCfg->data = HAL_UART_DATA_BITS_8;
            break;
        case UART_ATTR_DATABIT_7:
            uartCfg->data = HAL_UART_DATA_BITS_7;
            break;
        case UART_ATTR_DATABIT_6:
            uartCfg->data = HAL_UART_DATA_BITS_6;
            break;
        case UART_ATTR_DATABIT_5:
            uartCfg->data = HAL_UART_DATA_BITS_5;
            break;
        default:
            uartCfg->data = HAL_UART_DATA_BITS_8;
            break;
    }

    uartCfg->parity = attribute->parity;

    switch (attribute->stopBits) {
        case UART_ATTR_STOPBIT_1:
        case UART_ATTR_STOPBIT_2:
            uartCfg->stop = attribute->stopBits;
            break;
        default:
            uartCfg->stop = UART_ATTR_STOPBIT_1;
            break;
    }

    if (attribute->rts && attribute->cts) {
        uartCfg->flow = HAL_UART_FLOW_CONTROL_RTSCTS;
    } else if (attribute->rts && !attribute->cts) {
        uartCfg->flow = HAL_UART_FLOW_CONTROL_RTS;
    } else if (!attribute->rts && attribute->cts) {
        uartCfg->flow = HAL_UART_FLOW_CONTROL_CTS;
    } else {
        uartCfg->flow = HAL_UART_FLOW_CONTROL_NONE;
    }

    hal_uart_reopen(uartId, uartCfg);

    return HDF_SUCCESS;
}

static int32_t UartHostDevGetAttribute(struct UartHost *host, struct UartAttribute *attribute)
{
    HDF_LOGI("%s: Enter", __func__);
    struct UartDevice *uartDevice = NULL;
    struct HAL_UART_CFG_T *uartCfg = NULL;
    if (host == NULL || attribute == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartCfg = &uartDevice->config;
    if (uartCfg == NULL) {
        HDF_LOGE("%s: config is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    switch (uartCfg->data) {
        case HAL_UART_DATA_BITS_8:
            attribute->dataBits = UART_ATTR_DATABIT_8;
            break;
        case HAL_UART_DATA_BITS_7:
            attribute->dataBits = UART_ATTR_DATABIT_7;
            break;
        case HAL_UART_DATA_BITS_6:
            attribute->dataBits = UART_ATTR_DATABIT_6;
            break;
        case HAL_UART_DATA_BITS_5:
            attribute->dataBits = UART_ATTR_DATABIT_5;
            break;
        default:
            attribute->dataBits = UART_ATTR_DATABIT_8;
            break;
    }

    attribute->parity = uartCfg->parity;
    attribute->stopBits = uartCfg->stop;

    switch (uartCfg->flow) {
        case HAL_UART_FLOW_CONTROL_NONE:
            attribute->rts = 0;
            attribute->cts = 0;
            break;
        case HAL_UART_FLOW_CONTROL_CTS:
            attribute->rts = 0;
            attribute->cts = 1;
            break;
        case HAL_UART_FLOW_CONTROL_RTS:
            attribute->rts = 1;
            attribute->cts = 0;
            break;
        case HAL_UART_FLOW_CONTROL_RTSCTS:
            attribute->rts = 1;
            attribute->cts = 1;
            break;
        default:
            attribute->rts = 0;
            attribute->cts = 0;
            break;
    }

    return HDF_SUCCESS;
}

static int32_t UartHostDevSetTransMode(struct UartHost *host, enum UartTransMode mode)
{
    HDF_LOGI("%s: Enter", __func__);
    struct UartDevice *uartDevice = NULL;
    uint32_t uartId;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (struct UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;

    switch (mode) {
        case UART_MODE_RD_BLOCK:
            g_uartCtx[uartId].isBlock = true;
            break;
        case UART_MODE_RD_NONBLOCK:
            g_uartCtx[uartId].isBlock = false;
            break;
        case UART_MODE_DMA_RX_EN:
            g_uartCtx[uartId].rxDMA = true;
            break;
        case UART_MODE_DMA_RX_DIS:
            g_uartCtx[uartId].rxDMA = false;
            break;
        case UART_MODE_DMA_TX_EN:
            g_uartCtx[uartId].txDMA = true;
            break;
        case UART_MODE_DMA_TX_DIS:
            g_uartCtx[uartId].txDMA = false;
            break;
        default:
            HDF_LOGE("%s: UartTransMode(%d) invalid", __func__, mode);
            break;
    }
    return HDF_SUCCESS;
}
