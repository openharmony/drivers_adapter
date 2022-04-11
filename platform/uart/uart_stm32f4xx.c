/*
 * Copyright (c) 2022 Talkweb Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <stdlib.h>
#include <string.h>
#include "hal_gpio.h"
#include "uart_if.h"
#include "stm32f4xx_ll_bus.h"
#include "uart_core.h"
#include "hal_usart.h"
#include "osal_sem.h"
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#include "hcs_macro.h"
#include "hdf_config_macro.h"
#else
#include "device_resource_if.h"
#endif
#include "hdf_log.h"

#define HDF_UART_TMO 1000
#define HDF_LOG_TAG uartDev
#define GPIO_MAX_LENGTH 32
#define UART_FIFO_MAX_BUFFER 2048
#define UART_DMA_RING_BUFFER_SIZE 256 // mast be 2^n

#define UART_IRQ_PRO 5
#define UART_DEV_SERVICE_NAME_PREFIX "HDF_PLATFORM_UART%d"
#define MAX_DEV_NAME_SIZE 32
typedef enum {
    USART_NUM_1 = 1,
    USART_NUM_2,
    USART_NUM_3,
    USART_NUM_4,
    USART_NUM_5,
    USART_NUM_6,
} USART_NUM;

typedef enum {
    USART_DATAWIDTH_8 = 0,
    USART_DATAWIDTH_9,
    USART_DATAWIDTH_MAX,
} USART_DATAWIDTH;

typedef enum {
    USART_STOP_BIT_0_5 = 0,
    USART_STOP_BIT_1,
    USART_STOP_BIT_1_5,
    USART_STOP_BIT_2,
    USART_STOP_BIT_MAX,
} USART_STOP_BIT;

typedef enum {
    USART_PARITY_CHECK_NONE = 0,
    USART_PARITY_CHECK_EVENT,
    USART_PARITY_CHECK_ODD,
    USART_PARITY_CHECK_MAX,
} USART_PARITY_CHECK;

typedef enum {
    USART_TRANS_DIR_NONE = 0,
    USART_TRANS_DIR_RX,
    USART_TRANS_DIR_TX,
    USART_TRANS_DIR_RX_TX,
    USART_TRANS_DIR_MAX,
} USART_TRANS_DIR;

typedef enum {
    USART_FLOW_CTRL_NONE = 0,
    USART_FLOW_CTRL_RTS,
    USART_FLOW_CTRL_CTS,
    USART_FLOW_CTRL_RTS_CTS,
    USART_FLOW_CTRL_MAX,
} USART_FLOW_CTRL;

typedef enum {
    USART_OVER_SIMPLING_16 = 0,
    USART_OVER_SIMPLING_8,
    USART_OVER_SIMPLING_MAX,
} USART_OVER_SIMPLING;

typedef enum {
    USART_TRANS_BLOCK = 0, // block
    USART_TRANS_NOBLOCK,
    USART_TRANS_TX_DMA, // TX DMA RX NORMAL
    USART_TRANS_RX_DMA, // TX NORMAL  RX DMA
    USART_TRANS_TX_RX_DMA, // TX DMA  RX DMA
    USART_TRANS_MODE_MAX,
} USART_TRANS_MODE;

typedef enum {
    USART_IDLE_IRQ_DISABLE = 0,
    USART_IDLE_IRQ_ENABLE,
    USART_IDLE_IRQ_MAX,
} USART_IDLE_IRQ;

typedef enum {
    USART_232 = 0,
    USART_485,
    USART_TYPE_MAX,
} USART_TYPE;

typedef struct {
    USART_NUM num;
    uint32_t baudRate;
    USART_DATAWIDTH dataWidth;
    USART_STOP_BIT stopBit;
    USART_PARITY_CHECK parity;
    USART_TRANS_DIR transDir;
    USART_FLOW_CTRL flowCtrl;
    USART_OVER_SIMPLING overSimpling;
    USART_TRANS_MODE transMode;
    USART_TYPE uartType;
    STM32_GPIO_PIN dePin;
    STM32_GPIO_GROUP deGroup;
} UartResource;

typedef enum {
    UART_DEVICE_UNINITIALIZED = 0x0u,
    UART_DEVICE_INITIALIZED = 0x1u,
} UartDeviceState;

typedef enum {
    UART_ATTR_HDF_TO_NIOBE = 0,
    UART_ATTR_NIOBE_TO_HDF,
} UART_ATTR_TRAN_TYPE;

typedef void (*HWI_UART_IDLE_IRQ)(void);

typedef struct {
    bool txDMA;
    bool rxDMA;
    bool isBlock;
    uint8_t dePin;
    uint8_t deGroup;
    uint8_t uartType;
} UsartContextObj;

typedef struct {
    struct IDeviceIoService ioService;
    UartResource resource;
    LL_USART_InitTypeDef initTypedef;
    uint32_t uartId;
    bool initFlag;
    USART_TRANS_MODE transMode;
} UartDevice;

static const uint32_t g_dataWidthMap[USART_DATAWIDTH_MAX] = {
    LL_USART_DATAWIDTH_8B,
    LL_USART_DATAWIDTH_9B,
};

static const uint32_t g_stopBitMap[USART_STOP_BIT_MAX] = {
    LL_USART_STOPBITS_0_5,
    LL_USART_STOPBITS_1,
    LL_USART_STOPBITS_1_5,
    LL_USART_STOPBITS_2,
};

static const uint32_t g_flowControlMap[USART_FLOW_CTRL_MAX] = {
    LL_USART_HWCONTROL_NONE,
    LL_USART_HWCONTROL_RTS,
    LL_USART_HWCONTROL_CTS,
    LL_USART_HWCONTROL_RTS_CTS,
};

static const uint32_t g_overSimplingMap[USART_OVER_SIMPLING_MAX] = {
    LL_USART_OVERSAMPLING_16,
    LL_USART_OVERSAMPLING_8,
};

static const uint32_t g_transDirMap[USART_TRANS_DIR_MAX] = {
    LL_USART_DIRECTION_NONE,
    LL_USART_DIRECTION_RX,
    LL_USART_DIRECTION_TX,
    LL_USART_DIRECTION_TX_RX,
};

static const uint32_t g_parityMap[USART_PARITY_CHECK_MAX] = {
    LL_USART_PARITY_NONE,
    LL_USART_PARITY_EVEN,
    LL_USART_PARITY_ODD,
};

static const USART_TypeDef* g_usartRegMap[USART_NUM_6] = {
    USART1,
    USART2,
    USART3,
    UART4,
    UART5,
    USART6,
};
static UsartContextObj g_uartCtx[USART_NUM_6] = {0};

static void USARTTxMode(STM32_GPIO_PIN gpioPin, STM32_GPIO_GROUP group)
{
    uint32_t pinReg = LL_GET_HAL_PIN(gpioPin);
    GPIO_TypeDef* gpiox = NULL;
    gpiox = LL_GET_GPIOX(group);
    if (gpiox == NULL) {
        return;
    }
    LL_GPIO_SetOutputPin(gpiox, pinReg);
}

static void USARTRxMode(STM32_GPIO_PIN gpioPin, STM32_GPIO_GROUP group)
{
    uint32_t pinReg = LL_GET_HAL_PIN(gpioPin);
    GPIO_TypeDef* gpiox = NULL;
    gpiox = LL_GET_GPIOX(group);
    if (gpiox == NULL) {
        return;
    }
    LL_GPIO_ResetOutputPin(gpiox, pinReg);
}

static const uint32_t g_exitIrqnMap[USART_NUM_6] = {
    USART1_IRQn,
    USART2_IRQn,
    USART3_IRQn,
    UART4_IRQn,
    UART5_IRQn,
    USART6_IRQn,
};

static void InitUsartClock(USART_NUM num)
{
    switch (num) {
        case USART_NUM_1:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
            break;
        case USART_NUM_2:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
            break;
        case USART_NUM_3:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);
            break;
        case USART_NUM_4:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4);
            break;
        case USART_NUM_5:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5);
            break;
        case USART_NUM_6:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART6);
            break;
        default:
            break;
    }
}

static void InitContextTransMode(UsartContextObj* ctx, USART_TRANS_MODE mode)
{
    switch (mode) {
        case USART_TRANS_BLOCK:
            ctx->isBlock = true;
            ctx->rxDMA = false;
            ctx->txDMA = false;
            break;
        case USART_TRANS_NOBLOCK:
            ctx->isBlock = false;
            ctx->rxDMA = false;
            ctx->txDMA = false;
            break;
        case  USART_TRANS_TX_DMA: // TX DMA RX NORMAL
            ctx->isBlock = false;
            ctx->rxDMA = false;
            ctx->txDMA = true;
            break;
        case USART_TRANS_RX_DMA: // TX NORMAL  RX DMA
            ctx->isBlock = false;
            ctx->rxDMA = true;
            ctx->txDMA = false;
            break;
        case USART_TRANS_TX_RX_DMA: // TX DMA  RX DMA
            ctx->isBlock = false;
            ctx->rxDMA = true;
            ctx->txDMA = true;
            break;
        default:
            break;
    }
}

static int32_t UartDriverBind(struct HdfDeviceObject *device);
static int32_t UartDriverInit(struct HdfDeviceObject *device);
static void UartDriverRelease(struct HdfDeviceObject *device);

struct HdfDriverEntry g_UartDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "ST_UART_MODULE_HDF",
    .Bind = UartDriverBind,
    .Init = UartDriverInit,
    .Release = UartDriverRelease,
};
HDF_INIT(g_UartDriverEntry);

static int32_t UartHostDevInit(struct UartHost *host);
static int32_t UartHostDevDeinit(struct UartHost *host);
static int32_t UartHostDevWrite(struct UartHost *host, uint8_t *data, uint32_t size);
static int32_t UartHostDevSetBaud(struct UartHost *host, uint32_t baudRate);
static int32_t UartHostDevGetBaud(struct UartHost *host, uint32_t *baudRate);
static int32_t UartHostDevRead(struct UartHost *host, uint8_t *data, uint32_t size);
static int32_t UartHostDevSetAttribute(struct UartHost *host, struct UartAttribute *attribute);
static int32_t UartHostDevGetAttribute(struct UartHost *host, struct UartAttribute *attribute);
static int32_t UartHostDevSetTransMode(struct UartHost *host, enum UartTransMode mode);

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
    uint8_t initRet = 0;
    UartDevice *uartDevice = NULL;
    LL_USART_InitTypeDef *initTypedef = NULL;
    UartResource *resource = NULL;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    resource = &uartDevice->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    initTypedef = &uartDevice->initTypedef;
    if (initTypedef == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    InitContextTransMode(&g_uartCtx[uartDevice->uartId - 1], resource->transMode);
    LL_USART_Disable(g_usartRegMap[uartDevice->uartId - 1]);
    LL_USART_DeInit(g_usartRegMap[uartDevice->uartId - 1]);
    InitUsartClock(uartDevice->uartId);

    if (!uartDevice->initFlag) {
        initRet = LL_USART_Init(g_usartRegMap[uartDevice->uartId - 1], initTypedef);
        if (initRet) {
            HDF_LOGE("uart %ld device init failed\r\n", uartDevice->uartId);
            return HDF_FAILURE;
        }
        LL_USART_ConfigAsyncMode(g_usartRegMap[uartDevice->uartId - 1]);
        LL_USART_Enable(g_usartRegMap[uartDevice->uartId - 1]);
        UART_IRQ_INIT(g_usartRegMap[uartDevice->uartId - 1], uartDevice->uartId,
            g_exitIrqnMap[uartDevice->uartId - 1], g_uartCtx[uartDevice->uartId - 1].isBlock);

        uartDevice->initFlag = true;
    }

    return HDF_SUCCESS;
}

#ifndef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
static int32_t GetUartHcs(struct DeviceResourceIface *dri,
    const struct DeviceResourceNode *resourceNode, UartResource *resource)
{
    if (dri->GetUint8(resourceNode, "num", &resource->num, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "baudRate", &resource->baudRate, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "dataWidth", &resource->dataWidth, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "stopBit", &resource->stopBit, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "parity", &resource->parity, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "transDir", &resource->transDir, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    
    if (dri->GetUint8(resourceNode, "flowCtrl", &resource->flowCtrl, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "overSimpling", &resource->overSimpling, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "transMode", &resource->transMode, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "uartType", &resource->uartType, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "uartDePin", &resource->dePin, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (dri->GetUint8(resourceNode, "uartDeGroup", &resource->deGroup, 0) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}
#endif

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#define UART_FIND_CONFIG(node, name, resource) \
    do { \
        if (strcmp(HCS_PROP(node, match_attr), name) == 0) { \
            resource->num = HCS_PROP(node, num); \
            resource->baudRate = HCS_PROP(node, baudRate); \
            resource->dataWidth = HCS_PROP(node, dataWidth); \
            resource->stopBit = HCS_PROP(node, stopBit); \
            resource->parity = HCS_PROP(node, parity); \
            resource->transDir = HCS_PROP(node, transDir); \
            resource->flowCtrl = HCS_PROP(node, flowCtrl); \
            resource->overSimpling = HCS_PROP(node, overSimpling); \
            resource->transMode = HCS_PROP(node, transMode); \
            resource->uartType = HCS_PROP(node, uartType); \
            resource->dePin = HCS_PROP(node, uartDePin); \
            resource->deGroup = HCS_PROP(node, uartDeGroup); \
            result = HDF_SUCCESS; \
        } \
    } while (0)
#define PLATFORM_CONFIG HCS_NODE(HCS_ROOT, platform)
#define PLATFORM_UART_CONFIG HCS_NODE(HCS_NODE(HCS_ROOT, platform), uart_config)
static uint32_t GetUartDeviceResource(UartDevice *device, const char *deviceMatchAttr)
{
    UartResource *resource = NULL;
    int32_t result = HDF_FAILURE;
    if (device == NULL || deviceMatchAttr == NULL) {
        HDF_LOGE("device or deviceMatchAttr is NULL\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &device->resource;
#if HCS_NODE_HAS_PROP(PLATFORM_CONFIG, uart_config)
    HCS_FOREACH_CHILD_VARGS(PLATFORM_UART_CONFIG, UART_FIND_CONFIG, deviceMatchAttr, resource);
#endif
    if (result != HDF_SUCCESS) {
        HDF_LOGE("resourceNode %s is NULL\r\n", deviceMatchAttr);
        return result;
    }
    device->uartId = resource->num;
    g_uartCtx[device->uartId - 1].dePin = resource->dePin;
    g_uartCtx[device->uartId - 1].deGroup = resource->deGroup;
    g_uartCtx[device->uartId - 1].uartType = resource->uartType;
    device->initFlag = false;
    device->initTypedef.BaudRate = resource->baudRate;
    device->initTypedef.DataWidth = g_dataWidthMap[resource->dataWidth];
    device->initTypedef.HardwareFlowControl = g_flowControlMap[resource->flowCtrl];
    device->initTypedef.StopBits = g_stopBitMap[resource->stopBit];
    device->initTypedef.OverSampling = g_overSimplingMap[resource->overSimpling];
    device->initTypedef.TransferDirection = g_transDirMap[resource->transDir];
    device->initTypedef.Parity = g_parityMap[resource->parity];
    return HDF_SUCCESS;
}
#else
static int32_t GetUartDeviceResource(UartDevice *device, const struct DeviceResourceNode *resourceNode)
{
    struct DeviceResourceIface *dri = NULL;
    UartResource *resource = NULL;
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
    if (dri == NULL || dri->GetUint32 == NULL || dri->GetUint8 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid");
        return HDF_ERR_INVALID_PARAM;
    }

    if (GetUartHcs(dri, resourceNode, resource) != HDF_SUCCESS) {
        HDF_LOGE("Get uart hcs failed\n");
        return HDF_ERR_INVALID_PARAM;
    }

    device->uartId = resource->num;
    g_uartCtx[device->uartId - 1].dePin = resource->dePin;
    g_uartCtx[device->uartId - 1].deGroup = resource->deGroup;
    g_uartCtx[device->uartId - 1].uartType = resource->uartType;
    device->initFlag = false;
    device->initTypedef.BaudRate = resource->baudRate;
    device->initTypedef.DataWidth = g_dataWidthMap[resource->dataWidth];
    device->initTypedef.HardwareFlowControl = g_flowControlMap[resource->flowCtrl];
    device->initTypedef.StopBits = g_stopBitMap[resource->stopBit];
    device->initTypedef.OverSampling = g_overSimplingMap[resource->overSimpling];
    device->initTypedef.TransferDirection = g_transDirMap[resource->transDir];
    device->initTypedef.Parity = g_parityMap[resource->parity];

    return HDF_SUCCESS;
}
#endif

static int32_t AttachUartDevice(struct UartHost *uartHost, struct HdfDeviceObject *device)
{
    int32_t ret;
    UartDevice *uartDevice = NULL;

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    if (device == NULL || uartHost == NULL) {
#else
    if (uartHost == NULL || device == NULL || device->property == NULL) {
#endif
        HDF_LOGE("%s: property is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)OsalMemAlloc(sizeof(UartDevice));
    if (uartDevice == NULL) {
        HDF_LOGE("%s: OsalMemCalloc uartDevice error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    ret = GetUartDeviceResource(uartDevice, device->deviceMatchAttr);
#else
    ret = GetUartDeviceResource(uartDevice, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(uartDevice);
        return HDF_FAILURE;
    }

    uartHost->priv = uartDevice;

    return HDF_SUCCESS;
}

static int32_t UartDriverBind(struct HdfDeviceObject *device)
{
    struct UartHost *devService;
    if (device == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return (UartHostCreate(device) == NULL) ? HDF_FAILURE : HDF_SUCCESS;
}

static void UartDriverRelease(struct HdfDeviceObject *device)
{
    HDF_LOGI("Enter %s:", __func__);
    uint32_t uartId;
    struct UartHost *host = NULL;
    UartDevice *uartDevice = NULL;
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return;
    }

    host = UartHostFromDevice(device);
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: host is NULL", __func__);
        return;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return;
    }
    uartId = uartDevice->uartId;
    host->method = NULL;

    OsalMemFree(uartDevice);
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

    return InitUartDevice(host);
}

static int32_t UartHostDevDeinit(struct UartHost *host)
{
    HDF_LOGI("%s: Enter", __func__);
    uint32_t uartId;
    UartDevice *uartDevice = NULL;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: INVALID OBJECT", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;
    uartDevice->initFlag = false;
    UART_IRQ_DEINIT(g_usartRegMap[uartDevice->uartId - 1], g_exitIrqnMap[uartId -1]);
    LL_USART_Disable(g_usartRegMap[uartDevice->uartId - 1]);

    return LL_USART_DeInit(g_usartRegMap[uartId - 1]);
}

static int32_t UartHostDevWrite(struct UartHost *host, uint8_t *data, uint32_t size)
{
    UartDevice *device = NULL;
    uint32_t portId;

    if (host == NULL || data == NULL || size == 0 || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    device = (UartDevice*)host->priv;
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    portId = device->uartId;
    USART_TypeDef* usartx = g_usartRegMap[portId - 1];
    if (g_uartCtx[portId - 1].uartType == USART_485) {
        USARTTxMode(g_uartCtx[portId - 1].dePin, g_uartCtx[portId - 1].deGroup);
    }
    if (g_uartCtx[portId - 1].txDMA) {
        HDF_LOGE("unsupport dma now\n");
        return HDF_ERR_INVALID_PARAM;
    } else {
        USART_TxData(usartx, data, size);
    }
    if (g_uartCtx[portId - 1].uartType == USART_485) {
        USARTRxMode(g_uartCtx[portId - 1].dePin, g_uartCtx[portId - 1].deGroup);
    }

    return HDF_SUCCESS;
}

static int32_t UartHostDevRead(struct UartHost *host, uint8_t *data, uint32_t size)
{
    uint32_t recvSize = 0;
    int32_t ret;
    uint32_t uartId;
    UartDevice *uartDevice = NULL;
    if (host == NULL || data == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    uartId = uartDevice->uartId;
    USART_TypeDef* usartx = g_usartRegMap[uartId - 1];
    if (g_uartCtx[uartId - 1].uartType == USART_485) {
        USARTRxMode(g_uartCtx[uartId - 1].dePin, g_uartCtx[uartId - 1].deGroup);
    }

    if (g_uartCtx[uartId - 1].rxDMA) {
        HDF_LOGE("unsupport dma now\n");
        return HDF_ERR_INVALID_PARAM;
    } else {
        ret = USART_RxData(uartId, data, size, g_uartCtx[uartId - 1].isBlock);
    }

    return ret;
}

static int32_t UartHostDevSetBaud(struct UartHost *host, uint32_t baudRate)
{
    HDF_LOGI("%s: Enter", __func__);
    UartDevice *uartDevice = NULL;
    LL_USART_InitTypeDef *uartInit = NULL;

    uint32_t uartId;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    uartId = uartDevice->uartId;

    uartInit = &uartDevice->initTypedef;
    if (uartInit == NULL) {
        HDF_LOGE("%s: device config is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    uartInit->BaudRate = baudRate;
    UartHostDevDeinit(host);
    UartHostDevInit(host);

    return HDF_SUCCESS;
}

static int32_t UartHostDevGetBaud(struct UartHost *host, uint32_t *baudRate)
{
    HDF_LOGI("%s: Enter", __func__);
    UartDevice *uartDevice = NULL;
    LL_USART_InitTypeDef *uartInit = NULL;
    uint32_t uartId;
    if (host == NULL || baudRate == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;
    uartInit = &uartDevice->initTypedef;
    if (uartInit == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    *baudRate = uartInit->BaudRate;

    return HDF_SUCCESS;
}
static void TransDataWidth(LL_USART_InitTypeDef *uartInit, struct UartAttribute *attribute, UART_ATTR_TRAN_TYPE type)
{
    if (uartInit == NULL || attribute == NULL) {
        HDF_LOGE("null ptr\r\n");
        return;
    }

    if (type == UART_ATTR_NIOBE_TO_HDF) {
        switch (uartInit->DataWidth) { // only support 9bit and 8 bit in niobe407
            case LL_USART_DATAWIDTH_8B:
                attribute->dataBits = UART_ATTR_DATABIT_8;
                break;
            default:
                attribute->dataBits = UART_ATTR_DATABIT_8;
                break;
        }
    } else {
        switch (attribute->dataBits) {
            case UART_ATTR_DATABIT_8:
                uartInit->DataWidth = LL_USART_DATAWIDTH_8B;
                break;
            default:
                uartInit->DataWidth = LL_USART_DATAWIDTH_8B;
                break;
        }
    }

    return;
}

static void TransParity(LL_USART_InitTypeDef *uartInit, struct UartAttribute *attribute, UART_ATTR_TRAN_TYPE type)
{
    if (uartInit == NULL || attribute == NULL) {
        HDF_LOGE("null ptr\r\n");
        return;
    }

    if (type == UART_ATTR_NIOBE_TO_HDF) {
        switch (uartInit->Parity) {
            case LL_USART_PARITY_NONE:
                attribute->parity = UART_ATTR_PARITY_NONE;
                break;
            case LL_USART_PARITY_EVEN:
                attribute->parity = UART_ATTR_PARITY_EVEN;
                break;
            case LL_USART_PARITY_ODD:
                attribute->parity = UART_ATTR_PARITY_ODD;
                break;
            default:
                attribute->parity = UART_ATTR_PARITY_NONE;
                break;
        }
    } else {
        switch (attribute->parity) {
            case UART_ATTR_PARITY_NONE:
                uartInit->Parity = LL_USART_PARITY_NONE;
                break;
            case UART_ATTR_PARITY_EVEN:
                uartInit->Parity = LL_USART_PARITY_EVEN;
                break;
            case UART_ATTR_PARITY_ODD:
                uartInit->Parity = LL_USART_PARITY_ODD;
                break;
            default:
                uartInit->Parity = LL_USART_PARITY_NONE;
                break;
        }
    }

    return;
}

static void TransStopbit(LL_USART_InitTypeDef *uartInit, struct UartAttribute *attribute, UART_ATTR_TRAN_TYPE type)
{
    if (uartInit == NULL || attribute == NULL) {
        HDF_LOGE("null ptr\r\n");
        return;
    }

    if (type == UART_ATTR_NIOBE_TO_HDF) {
        switch (uartInit->StopBits) {
            case LL_USART_STOPBITS_1:
                attribute->stopBits = UART_ATTR_STOPBIT_1;
                break;
            case LL_USART_STOPBITS_1_5:
                attribute->stopBits = UART_ATTR_STOPBIT_1P5;
                break;
            case LL_USART_STOPBITS_2:
                attribute->stopBits = UART_ATTR_STOPBIT_2;
                break;
            default:
                attribute->stopBits = UART_ATTR_PARITY_NONE;
                break;
        }
    } else {
        switch (attribute->stopBits) {
            case UART_ATTR_STOPBIT_1:
                uartInit->StopBits = LL_USART_STOPBITS_1;
                break;
            case UART_ATTR_STOPBIT_1P5:
                uartInit->StopBits = LL_USART_STOPBITS_1_5;
                break;
            case UART_ATTR_STOPBIT_2:
                uartInit->StopBits = LL_USART_STOPBITS_2;
                break;
            default:
                uartInit->StopBits = LL_USART_STOPBITS_1;
                break;
        }
    }

    return;
}

static void TransFlowCtrl(LL_USART_InitTypeDef *uartInit, struct UartAttribute *attribute, UART_ATTR_TRAN_TYPE type)
{
    if (uartInit == NULL || attribute == NULL) {
        HDF_LOGE("null ptr\r\n");
        return;
    }

    if (type == UART_ATTR_NIOBE_TO_HDF) {
        switch (uartInit->HardwareFlowControl) {
            case LL_USART_HWCONTROL_NONE:
                attribute->rts = 0;
                attribute->cts = 0;
                break;
            case LL_USART_HWCONTROL_CTS:
                attribute->rts = 0;
                attribute->cts = 1;
                break;
            case LL_USART_HWCONTROL_RTS:
                attribute->rts = 1;
                attribute->cts = 0;
                break;
            case LL_USART_HWCONTROL_RTS_CTS:
                attribute->rts = 1;
                attribute->cts = 1;
                break;
            default:
                attribute->rts = 0;
                attribute->cts = 0;
                break;
        }
    } else {
        if (attribute->rts && attribute->cts) {
            uartInit->HardwareFlowControl = LL_USART_HWCONTROL_RTS_CTS;
        } else if (attribute->rts && !attribute->cts) {
            uartInit->HardwareFlowControl = LL_USART_HWCONTROL_RTS;
        } else if (!attribute->rts && attribute->cts) {
            uartInit->HardwareFlowControl = LL_USART_HWCONTROL_CTS;
        } else {
            uartInit->HardwareFlowControl = LL_USART_HWCONTROL_NONE;
        }
    }

    return;
}

static void TransDir(LL_USART_InitTypeDef *uartInit, struct UartAttribute *attribute, UART_ATTR_TRAN_TYPE type)
{
    if (uartInit == NULL || attribute == NULL) {
        HDF_LOGE("null ptr\r\n");
        return;
    }
    if (type == UART_ATTR_NIOBE_TO_HDF) {
        switch (uartInit->TransferDirection) {
            case LL_USART_DIRECTION_NONE:
                attribute->fifoRxEn = 0;
                attribute->fifoTxEn = 0;
                break;
            case LL_USART_DIRECTION_RX:
                attribute->fifoRxEn = 1;
                attribute->fifoTxEn = 0;
                break;
            case LL_USART_DIRECTION_TX:
                attribute->fifoRxEn = 0;
                attribute->fifoTxEn = 1;
                break;
            case LL_USART_DIRECTION_TX_RX:
                attribute->fifoRxEn = 1;
                attribute->fifoTxEn = 1;
                break;
            default:
                attribute->fifoRxEn = 1;
                attribute->fifoTxEn = 1;
                break;
        }
    } else {
        if (attribute->fifoRxEn && attribute->fifoTxEn) {
            uartInit->TransferDirection = LL_USART_DIRECTION_TX_RX;
        } else if (attribute->fifoRxEn && !attribute->fifoTxEn) {
            uartInit->TransferDirection = LL_USART_DIRECTION_RX;
        } else if (!attribute->fifoRxEn && attribute->fifoTxEn) {
            uartInit->TransferDirection = LL_USART_DIRECTION_TX;
        } else {
            uartInit->TransferDirection = LL_USART_DIRECTION_NONE;
        }
    }

    return;
}

static int32_t UartHostDevSetAttribute(struct UartHost *host, struct UartAttribute *attribute)
{
    HDF_LOGI("%s: Enter", __func__);
    UartDevice *uartDevice = NULL;
    LL_USART_InitTypeDef *uartInit = NULL;
    uint32_t uartId;
    if (host == NULL || attribute == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;
    uartInit = &uartDevice->initTypedef;
    if (uartInit == NULL) {
        HDF_LOGE("%s: config is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    TransDataWidth(uartInit, attribute, UART_ATTR_HDF_TO_NIOBE);
    TransParity(uartInit, attribute, UART_ATTR_HDF_TO_NIOBE);
    TransStopbit(uartInit, attribute, UART_ATTR_HDF_TO_NIOBE);
    TransFlowCtrl(uartInit, attribute, UART_ATTR_HDF_TO_NIOBE);
    TransDir(uartInit, attribute, UART_ATTR_HDF_TO_NIOBE);

    UartHostDevDeinit(host);
    UartHostDevInit(host);

    return HDF_SUCCESS;
}

static int32_t UartHostDevGetAttribute(struct UartHost *host, struct UartAttribute *attribute)
{
    HDF_LOGI("%s: Enter", __func__);
    UartDevice *uartDevice = NULL;
    LL_USART_InitTypeDef *uartInit = NULL;
    if (host == NULL || attribute == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartInit = &uartDevice->initTypedef;
    if (uartInit == NULL) {
        HDF_LOGE("%s: uartInit is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    TransDataWidth(uartInit, attribute, UART_ATTR_NIOBE_TO_HDF);
    TransParity(uartInit, attribute, UART_ATTR_NIOBE_TO_HDF);
    TransStopbit(uartInit, attribute, UART_ATTR_NIOBE_TO_HDF);
    TransFlowCtrl(uartInit, attribute, UART_ATTR_NIOBE_TO_HDF);
    TransDir(uartInit, attribute, UART_ATTR_NIOBE_TO_HDF);

    return HDF_SUCCESS;
}

static int32_t UartHostDevSetTransMode(struct UartHost *host, enum UartTransMode mode)
{
    HDF_LOGI("%s: Enter", __func__);
    UartDevice *uartDevice = NULL;
    uint32_t uartId;
    if (host == NULL || host->priv == NULL) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uartDevice = (UartDevice *)host->priv;
    if (uartDevice == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    uartId = uartDevice->uartId;

    switch (mode) {
        case UART_MODE_RD_BLOCK:
            g_uartCtx[uartId - 1].isBlock = true;
            break;
        case UART_MODE_RD_NONBLOCK:
            g_uartCtx[uartId - 1].isBlock = false;
            break;
        case UART_MODE_DMA_RX_EN:
            g_uartCtx[uartId - 1].rxDMA = true;
            break;
        case UART_MODE_DMA_RX_DIS:
            g_uartCtx[uartId - 1].rxDMA = false;
            break;
        case UART_MODE_DMA_TX_EN:
            g_uartCtx[uartId - 1].txDMA = true;
            break;
        case UART_MODE_DMA_TX_DIS:
            g_uartCtx[uartId - 1].txDMA = false;
            break;
        default:
            HDF_LOGE("%s: UartTransMode(%d) invalid", __func__, mode);
            break;
    }

    return HDF_SUCCESS;
}