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
#include "hal_spi.h"
#include "osal_mutex.h"
#include "osal_sem.h"
#include "spi_core.h"
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#include "hcs_macro.h"
#include "hdf_config_macro.h"
#else
#include "device_resource_if.h"
#include "hdf_log.h"
#endif
#include "hdf_base_hal.h"

#define BITWORD_EIGHT 8
#define BITWORD_SIXTEEN 16
#define GPIO_STR_MAX 32
#define PER_MS_IN_SEC 1000
typedef enum {
    SPI_WORK_MODE_0, // CPOL = 0; CPHA = 0
    SPI_WORK_MODE_2, // CPOL = 1; CPHA = 0
    SPI_WORK_MODE_1, // CPOL = 0; CPHA = 1
    SPI_WORK_MODE_3, // CPOL = 1; CPHA = 1
    SPI_WORD_MODE_MAX,
} SPI_CLK_MODE;

typedef enum {
    SPI_TRANSFER_DMA,
    SPI_TRANSFER_NORMAL,
    SPI_TRANSFER_MAX,
} SPI_TRANS_MODE;

typedef enum {
    FULL_DUPLEX = 0,
    SIMPLE_RX,
    HALF_RX,
    HALF_TX,
    SPI_TRANS_DIR_MAX,
} SPI_TRANS_DIR;

typedef enum {
    SPI_SLAVE_MODE = 0,
    SPI_MASTER_MODE,
    SPI_MASTER_SLAVE_MAX,
} SPI_SLAVE_MASTER;

typedef enum {
    SPI_DATA_WIDTH_8 = 0,
    SPI_DATA_WIDTH_16,
    SPI_DATA_WIDTH_MAX,
} SPI_DATA_WIDTH;

typedef enum {
    SPI_NSS_SOFT_MODE = 0,
    SPI_NSS_HARD_INPUT_MODE,
    SPI_NSS_HARD_OUTPUT_MODE,
    SPI_NSS_MODE_MAX,
} SPI_NSS;

typedef enum {
    BAUD_RATE_DIV2 = 0,
    BAUD_RATE_DIV4,
    BAUD_RATE_DIV8,
    BAUD_RATE_DIV16,
    BAUD_RATE_DIV32,
    BAUD_RATE_DIV64,
    BAUD_RATE_DIV128,
    BAUD_RATE_DIV256,
    BAUD_RATE_DIV_MAX,
} SPI_BAUD_RATE;

typedef enum {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST,
    SPI_MLSB_MAX,
} SPI_BYTE_ORDER;

typedef enum {
    CRC_DISABLE = 0,
    CRC_ENABLE,
    CRC_STATE_MAX,
} CRC_CALULATION;

typedef enum {
    SPI_PORT1 = 1,
    SPI_PORT2,
    SPI_PORT3,
} SPI_GROUPS;

typedef enum {
    SPI_PROTO_MOTOROLA = 0,
    SPI_PROTO_TI,
    SPI_PROTO_MAX,
} SPI_PROTO_STANDARD;

typedef struct {
    uint8_t busNum;
    uint8_t csNum;
    SPI_TRANS_DIR transDir;
    SPI_TRANS_MODE transMode;

    SPI_SLAVE_MASTER smMode;
    SPI_CLK_MODE clkMode;
    SPI_DATA_WIDTH dataWidth;
    SPI_NSS nss;

    SPI_BAUD_RATE baudRate;
    SPI_BYTE_ORDER bitOrder;
    CRC_CALULATION crcEnable;
    SPI_GROUPS spix;

    STM32_GPIO_PIN csPin;
    STM32_GPIO_GROUP csGroup;
    SPI_PROTO_STANDARD standard;
    uint8_t dummyByte;

    uint16_t crcPoly;
} SpiResource;

#define HCS_UINT8_PARSE_NUM 16
static const char *g_parseHcsMap[HCS_UINT8_PARSE_NUM] = {
    "busNum",
    "csNum",
    "transDir",
    "transMode",
    "smMode",
    "clkMode",
    "dataWidth",
    "nss",
    "baudRate",
    "bitOrder",
    "crcEnable",
    "spix",
    "csPin",
    "csGpiox",
    "standard",
    "dummyByte",
};

typedef struct {
    struct OsalSem* sem;
    struct OsalMutex* mutex;
    SPI_TypeDef* spix;
} SPI_CONTEXT_T;

typedef struct {
    uint32_t spiId;
    SpiResource resource;
} SpiDevice;

static uint32_t g_transDirMaps[SPI_TRANS_DIR_MAX] = {
    LL_SPI_FULL_DUPLEX,
    LL_SPI_SIMPLEX_RX,
    LL_SPI_HALF_DUPLEX_RX,
    LL_SPI_HALF_DUPLEX_TX,
};

static uint32_t g_nssMaps[SPI_NSS_MODE_MAX] = {
    LL_SPI_NSS_SOFT,
    LL_SPI_NSS_HARD_INPUT,
    LL_SPI_NSS_HARD_OUTPUT,
};

static uint32_t g_baudMaps[BAUD_RATE_DIV_MAX] = {
    LL_SPI_BAUDRATEPRESCALER_DIV2,
    LL_SPI_BAUDRATEPRESCALER_DIV4,
    LL_SPI_BAUDRATEPRESCALER_DIV8,
    LL_SPI_BAUDRATEPRESCALER_DIV16,
    LL_SPI_BAUDRATEPRESCALER_DIV32,
    LL_SPI_BAUDRATEPRESCALER_DIV64,
    LL_SPI_BAUDRATEPRESCALER_DIV128,
    LL_SPI_BAUDRATEPRESCALER_DIV256,
};

static SPI_TypeDef* g_spiGroupMaps[SPI_PORT3] = {
    SPI1,
    SPI2,
    SPI3,
};

static void EnableSpiClock(uint32_t spiNum)
{
    switch (spiNum) {
        case SPI_PORT1:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
            break;
        case SPI_PORT2:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);
            break;
        case SPI_PORT3:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI3);
            break;
        default:
            break;
    }
}

static SPI_CONTEXT_T spiContext[SPI_PORT3] = {
    {
        .sem = {NULL},
        .mutex = {NULL},
        .spix = {NULL},
    },
    {
        .sem = {NULL},
        .mutex = {NULL},
        .spix = {NULL},
    },
    {
        .sem = {NULL},
        .mutex = {NULL},
        .spix = {NULL},
    },
};

int32_t HalSpiSend(SpiDevice *spiDevice, const uint8_t *data, uint16_t size)
{
    uint32_t spiId;
    SpiResource *resource = NULL;

    if (spiDevice == NULL || data == NULL || size == 0) {
        HDF_LOGE("spi input para err\r\n");
        return HDF_ERR_INVALID_PARAM;
    }

    spiId = spiDevice->spiId;
    resource = &spiDevice->resource;
    if (resource == NULL) {
        HDF_LOGE("resource is null\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (resource->transMode == SPI_TRANSFER_DMA) {
        return HDF_ERR_INVALID_PARAM; // unsupport now
    } else {
        uint8_t readData;
        while (size--) {
            readData = LL_SPI_Transmit(spiContext[spiId].spix, *data);
            data++;
        }
    }

    return HDF_SUCCESS;
}

int32_t HalSpiRecv(SpiDevice *spiDevice, uint8_t *data, uint16_t size)
{
    uint32_t len = size;
    uint32_t remainder = 0;
    uint8_t *cmd = NULL;
    uint32_t spiId;
    SpiResource *resource = NULL;
    if (spiDevice == NULL || data == NULL || size == 0) {
        HDF_LOGE("spi input para err\r\n");
        return HDF_ERR_INVALID_PARAM;
    }

    spiId = spiDevice->spiId;
    resource = &spiDevice->resource;
    if (resource == NULL) {
        HDF_LOGE("resource is null\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }
    cmd = (uint8_t *)OsalMemAlloc(len);
    if (cmd == NULL) {
        HDF_LOGE("%s OsalMemAlloc size %ld error\r\n", __FUNCTION__, len);
        return HDF_ERR_MALLOC_FAIL;
    }

    memset_s(cmd, len, resource->dummyByte, len);

    if (resource->transMode == SPI_TRANSFER_DMA) {
        return HDF_ERR_INVALID_PARAM; // unsupport now
    } else {
        while (len--) {
            *data = LL_SPI_Transmit(spiContext[spiId].spix, *cmd);
            data++;
            cmd++;
        }
    }

    OsalMemFree(cmd);
    return HDF_SUCCESS;
}

int32_t HalSpiSendRecv(SpiDevice *spiDevice, uint8_t *txData,
    uint16_t txSize, uint8_t *rxData, uint16_t rxSize)
{
    uint32_t spiId;
    uint8_t* data = rxData;
    int16_t dropSize = 0;
    SpiResource *resource = NULL;
    if (spiDevice == NULL || txData == NULL || txSize == 0 || rxData == NULL || rxSize == 0) {
        HDF_LOGE("spi input para err\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    spiId = spiDevice->spiId;
    resource = &spiDevice->resource;

    if (resource->transMode == SPI_TRANSFER_DMA) {
        return HDF_ERR_INVALID_PARAM; // unsupport now
    } else {
        while (rxSize--) {
            *rxData = LL_SPI_Transmit(spiContext[spiId].spix, *txData);
            rxData++;
            txData++;
        }
    }

    return HDF_SUCCESS;
}

static void InitSpiInitStruct(LL_SPI_InitTypeDef *spiInitStruct, const SpiResource *resource)
{
    spiInitStruct->TransferDirection = g_transDirMaps[resource->transDir];
    if (resource->smMode == SPI_SLAVE_MODE) {
        spiInitStruct->Mode = LL_SPI_MODE_SLAVE;
    } else {
        spiInitStruct->Mode = LL_SPI_MODE_MASTER;
    }

    if (resource->dataWidth == SPI_DATA_WIDTH_8) {
        spiInitStruct->DataWidth = LL_SPI_DATAWIDTH_8BIT;
    } else {
        spiInitStruct->DataWidth = LL_SPI_DATAWIDTH_16BIT;
    }

    switch (resource->clkMode) {
        case SPI_WORK_MODE_0:
            spiInitStruct->ClockPolarity = LL_SPI_POLARITY_LOW;
            spiInitStruct->ClockPhase = LL_SPI_PHASE_1EDGE;
            break;
        case SPI_WORK_MODE_1:
            spiInitStruct->ClockPolarity = LL_SPI_POLARITY_HIGH;
            spiInitStruct->ClockPhase = LL_SPI_PHASE_1EDGE;
            break;
        case SPI_WORK_MODE_2:
            spiInitStruct->ClockPolarity = LL_SPI_POLARITY_LOW;
            spiInitStruct->ClockPhase = LL_SPI_PHASE_2EDGE;
            break;
        case SPI_WORK_MODE_3:
            spiInitStruct->ClockPolarity = LL_SPI_POLARITY_HIGH;
            spiInitStruct->ClockPhase = LL_SPI_PHASE_2EDGE;
            break;
        default:
            spiInitStruct->ClockPolarity = LL_SPI_POLARITY_LOW;
            spiInitStruct->ClockPhase = LL_SPI_PHASE_1EDGE;
    }

    spiInitStruct->NSS = g_nssMaps[resource->nss];
    spiInitStruct->BaudRate = g_baudMaps[resource->baudRate];

    if (resource->bitOrder == SPI_MSB_FIRST) {
        spiInitStruct->BitOrder = LL_SPI_MSB_FIRST;
    } else {
        spiInitStruct->BitOrder = LL_SPI_LSB_FIRST;
    }

    if (resource->crcEnable == CRC_DISABLE) {
        spiInitStruct->CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    } else {
        spiInitStruct->CRCCalculation = LL_SPI_CRCCALCULATION_ENABLE;
    }
    spiInitStruct->CRCPoly = resource->crcPoly;

    return;
}

static int32_t InitSpiDevice(SpiDevice *spiDevice)
{
    uint32_t spiPort;
    SpiResource *resource = NULL;
    if (spiDevice == NULL) {
        HDF_LOGE("%s: invalid parameter\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    LL_SPI_InitTypeDef spiInitStruct = {0};
    resource = &spiDevice->resource;
    spiPort = spiDevice->spiId;
    EnableSpiClock(spiPort + 1);
    InitSpiInitStruct(&spiInitStruct, resource);
    SPI_TypeDef* spix = g_spiGroupMaps[resource->spix];

    spiContext[spiPort].spix = spix;
    LL_SPI_Disable(spix);
    uint8_t ret = LL_SPI_Init(spix, &spiInitStruct);
    if (ret != 0) {
        HDF_LOGE("HAL INIT SPI FAILED\r\n");
        return HDF_FAILURE;
    }
    if (resource->standard == SPI_PROTO_MOTOROLA) {
        LL_SPI_SetStandard(spix, LL_SPI_PROTOCOL_MOTOROLA);
    } else {
        LL_SPI_SetStandard(spix, LL_SPI_PROTOCOL_TI);
    }

    return HDF_SUCCESS;
}

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#define SPI_FIND_CONFIG(node, name, resource, spiDevice) \
    do { \
        if (strcmp(HCS_PROP(node, match_attr), name) == 0) { \
            resource->busNum = HCS_PROP(node, busNum); \
            spiDevice->spiId = resource->busNum; \
            resource->csNum = HCS_PROP(node, csNum); \
            resource->transDir = HCS_PROP(node, transDir); \
            resource->transMode = HCS_PROP(node, transMode); \
            resource->smMode = HCS_PROP(node, smMode); \
            resource->dataWidth = HCS_PROP(node, dataWidth); \
            resource->clkMode = HCS_PROP(node, clkMode); \
            resource->csNum = HCS_PROP(node, csNum); \
            resource->nss = HCS_PROP(node, nss); \
            resource->baudRate = HCS_PROP(node, baudRate); \
            resource->bitOrder = HCS_PROP(node, bitOrder); \
            resource->crcEnable = HCS_PROP(node, crcEnable); \
            resource->crcPoly = HCS_PROP(node, crcPoly); \
            resource->spix = HCS_PROP(node, spix); \
            resource->csPin = HCS_PROP(node, csPin); \
            resource->csGroup = HCS_PROP(node, csGpiox); \
            resource->standard = HCS_PROP(node, standard); \
            resource->dummyByte = HCS_PROP(node, dummyByte); \
            result = HDF_SUCCESS; \
        } \
    } while (0)

#define PLATFORM_CONFIG HCS_NODE(HCS_ROOT, platform)
#define PLATFORM_SPI_CONFIG HCS_NODE(HCS_NODE(HCS_ROOT, platform), spi_config)
static int32_t GetSpiDeviceResource(SpiDevice *spiDevice, const char *deviceMatchAttr)
{
    int32_t result = HDF_FAILURE;
    SpiResource *resource = NULL;
    if (spiDevice == NULL || deviceMatchAttr == NULL) {
        HDF_LOGE("device or deviceMatchAttr is NULL\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &spiDevice->resource;
#if HCS_NODE_HAS_PROP(PLATFORM_CONFIG, spi_config)
    HCS_FOREACH_CHILD_VARGS(PLATFORM_SPI_CONFIG, SPI_FIND_CONFIG, deviceMatchAttr, resource, spiDevice);
#endif
    if (result != HDF_SUCCESS) {
        HDF_LOGE("resourceNode %s is NULL\r\n", deviceMatchAttr);
    }
    return result;
}
#else
static int32_t GetSpiDeviceResource(SpiDevice *spiDevice, const struct DeviceResourceNode *resourceNode)
{
    struct DeviceResourceIface *dri = NULL;
    if (spiDevice == NULL || resourceNode == NULL) {
        HDF_LOGE("%s: PARAM is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    SpiResource *resource = NULL;
    resource = &spiDevice->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: resource is NULL\r\n", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE); // open HDF
    if (dri == NULL || dri->GetUint16 == NULL || dri->GetUint8 == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    uint8_t temp[HCS_UINT8_PARSE_NUM] = {0};
    for (int i = 0; i < HCS_UINT8_PARSE_NUM; i++) {
        if (dri->GetUint8(resourceNode, g_parseHcsMap[i], &temp[i], 0) != HDF_SUCCESS) {
            HDF_LOGE("get config %s failed\r\n", g_parseHcsMap[i]);
            return HDF_FAILURE;
        }
    }
    int ret = memcpy_s(resource, HCS_UINT8_PARSE_NUM, temp, HCS_UINT8_PARSE_NUM);
    if (ret != 0) {
        HDF_LOGE("memcpy failed\r\n");
        return HDF_FAILURE;
    }
    if (dri->GetUint16(resourceNode, "crcPoly", &resource->crcPoly, 0) != HDF_SUCCESS) {
        HDF_LOGE("get config %s failed\r\n", "crcPoly");
        return HDF_FAILURE;
    }
    spiDevice->spiId = resource->busNum;

    return HDF_SUCCESS;
}
#endif

int32_t AttachSpiDevice(struct SpiCntlr *spiCntlr, struct HdfDeviceObject *device)
{
    int32_t ret;
    SpiDevice *spiDevice = NULL;

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    if (spiCntlr == NULL || device == NULL) {
#else
    if (spiCntlr == NULL || device == NULL || device->property == NULL) {
#endif
        HDF_LOGE("%s: property is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    spiDevice = (SpiDevice *)OsalMemAlloc(sizeof(SpiDevice));
    if (spiDevice == NULL) {
        HDF_LOGE("%s: OsalMemAlloc spiDevice error\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    ret = GetSpiDeviceResource(spiDevice, device->deviceMatchAttr);
#else
    ret = GetSpiDeviceResource(spiDevice, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(spiDevice);
        return HDF_FAILURE;
    }

    spiCntlr->priv = spiDevice;
    spiCntlr->busNum = spiDevice->spiId;
    InitSpiDevice(spiDevice);

    return HDF_SUCCESS;
}
/* SPI Method */
static int32_t SpiDevGetCfg(struct SpiCntlr *spiCntlr, struct SpiCfg *spiCfg);
static int32_t SpiDevSetCfg(struct SpiCntlr *spiCntlr, struct SpiCfg *spiCfg);
static int32_t SpiDevTransfer(struct SpiCntlr *spiCntlr, struct SpiMsg *spiMsg, uint32_t count);
static int32_t SpiDevOpen(struct SpiCntlr *spiCntlr);
static int32_t SpiDevClose(struct SpiCntlr *spiCntlr);

struct SpiCntlrMethod g_twSpiCntlrMethod = {
    .GetCfg = SpiDevGetCfg,
    .SetCfg = SpiDevSetCfg,
    .Transfer = SpiDevTransfer,
    .Open = SpiDevOpen,
    .Close = SpiDevClose,
};

/* HdfDriverEntry method definitions */
static int32_t SpiDriverBind(struct HdfDeviceObject *device);
static int32_t SpiDriverInit(struct HdfDeviceObject *device);
static void SpiDriverRelease(struct HdfDeviceObject *device);

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_SpiDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "ST_SPI_MODULE_HDF",
    .Bind = SpiDriverBind,
    .Init = SpiDriverInit,
    .Release = SpiDriverRelease,
};

HDF_INIT(g_SpiDriverEntry);

static int32_t SpiDriverBind(struct HdfDeviceObject *device)
{
    struct SpiCntlr *spiCntlr = NULL;
    if (device == NULL) {
        HDF_LOGE("Sample device object is null!\r\n");
        return HDF_ERR_INVALID_PARAM;
    }
    HDF_LOGI("Enter %s:\r\n", __func__);
    spiCntlr = SpiCntlrCreate(device);
    if (spiCntlr == NULL) {
        HDF_LOGE("SpiCntlrCreate object is null!\r\n");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t SpiDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct SpiCntlr *spiCntlr = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    HDF_LOGI("Enter %s:", __func__);
    spiCntlr = SpiCntlrFromDevice(device);
    if (spiCntlr == NULL) {
        HDF_LOGE("%s: spiCntlr is NULL", __func__);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    ret = AttachSpiDevice(spiCntlr, device); // SpiCntlr add TWSpiDevice to priv
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: attach error\r\n", __func__);
        return HDF_DEV_ERR_ATTACHDEV_FAIL;
    }

    spiCntlr->method = &g_twSpiCntlrMethod; // register callback

    return ret;
}

static void SpiDriverRelease(struct HdfDeviceObject *device)
{
    struct SpiCntlr *spiCntlr = NULL;
    SpiDevice *spiDevice = NULL;

    HDF_LOGI("Enter %s\r\n", __func__);

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return;
    }

    spiCntlr = SpiCntlrFromDevice(device);
    if (spiCntlr == NULL || spiCntlr->priv == NULL) {
        HDF_LOGE("%s: spiCntlr is NULL\r\n", __func__);
        return;
    }

    spiDevice = (SpiDevice *)spiCntlr->priv;
    OsalMemFree(spiDevice);

    return;
}

static int32_t SpiDevOpen(struct SpiCntlr *spiCntlr)
{
    HDF_LOGI("Enter %s\r\n", __func__);
    uint32_t spiPort;
    SpiDevice *spiDevice = NULL;

    spiDevice = (SpiDevice*)spiCntlr->priv;
    spiPort = spiDevice->spiId;
    SPI_TypeDef* spix = g_spiGroupMaps[spiDevice->resource.spix];
    LL_SPI_Enable(spix);

    return HDF_SUCCESS;
}

static int32_t SpiDevClose(struct SpiCntlr *spiCntlr)
{
    uint32_t spiPort;
    SpiDevice *spiDevice = NULL;

    spiDevice = (SpiDevice*)spiCntlr->priv;
    spiPort = spiDevice->spiId;
    SPI_TypeDef* spix = g_spiGroupMaps[spiDevice->resource.spix];
    LL_SPI_Disable(spix);

    return HDF_SUCCESS;
}

static int32_t SpiDevGetCfg(struct SpiCntlr *spiCntlr, struct SpiCfg *spiCfg)
{
    SpiDevice *spiDevice = NULL;
    if (spiCntlr == NULL || spiCfg == NULL || spiCntlr->priv == NULL) {
        HDF_LOGE("%s: spiCntlr is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    spiDevice = (SpiDevice *)spiCntlr->priv;
    if (spiDevice == NULL) {
        return HDF_DEV_ERR_NO_DEVICE;
    }
    spiCfg->maxSpeedHz = spiDevice->resource.baudRate;
    spiCfg->mode = spiDevice->resource.clkMode;
    spiCfg->transferMode = spiDevice->resource.transMode;
    spiCfg->bitsPerWord = spiDevice->resource.dataWidth;

    if (spiDevice->resource.dataWidth) {
        spiCfg->bitsPerWord = BITWORD_SIXTEEN;
    } else {
        spiCfg->bitsPerWord = BITWORD_EIGHT;
    }

    return HDF_SUCCESS;
}
static int32_t SpiDevSetCfg(struct SpiCntlr *spiCntlr, struct SpiCfg *spiCfg)
{
    SpiDevice *spiDevice = NULL;
    if (spiCntlr == NULL || spiCfg == NULL || spiCntlr->priv == NULL) {
        HDF_LOGE("%s: spiCntlr is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    spiDevice = (SpiDevice *)spiCntlr->priv;
    if (spiDevice == NULL) {
        HDF_LOGE("%s: spiDevice is NULL\r\n", __func__);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    spiDevice->resource.baudRate = spiCfg->maxSpeedHz;
    spiDevice->resource.clkMode = spiCfg->mode;
    spiDevice->resource.transMode = spiCfg->transferMode;
    if (spiCfg->bitsPerWord == BITWORD_EIGHT) {
        spiDevice->resource.dataWidth  = SPI_DATA_WIDTH_8;
    } else {
        spiDevice->resource.dataWidth  = SPI_DATA_WIDTH_16;
    }

    return InitSpiDevice(spiDevice);
}

static int32_t SpiDevTransfer(struct SpiCntlr *spiCntlr, struct SpiMsg *spiMsg, uint32_t count)
{
    uint32_t spiId;
    SpiDevice *spiDevice = NULL;
    uint32_t ticks = 0;
    uint8_t singleCsChange = 0;
    struct SpiMsg *msg = NULL;
    if (spiCntlr == NULL || spiCntlr->priv == NULL) {
        HDF_LOGE("%s: spiCntlr is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    spiDevice = (SpiDevice *)spiCntlr->priv;
    spiId = spiDevice->spiId;
    for (size_t i = 0; i < count; i++) {
        msg = &spiMsg[i];
        LL_GPIO_ResetOutputPin(LL_GET_GPIOX(spiDevice->resource.csGroup), LL_GET_HAL_PIN(spiDevice->resource.csPin));

        if ((msg->rbuf == NULL) && (msg->wbuf != NULL)) {
            singleCsChange = msg->wbuf[0];

            if (msg->len == 1) {
                goto CS_DOWN;
            }
            HalSpiSend(spiDevice, msg->wbuf + 1, msg->len - 1);
        }

        if ((msg->rbuf != NULL) && (msg->wbuf == NULL)) {
            singleCsChange = msg->rbuf[0];

            if (msg->len == 1) {
                goto CS_DOWN;
            }
            HalSpiRecv(spiDevice, msg->rbuf + 1, msg->len - 1);
        }

        if ((msg->wbuf != NULL) && (msg->rbuf != NULL)) {
            HalSpiSendRecv(spiDevice, msg->wbuf, msg->len, msg->rbuf, msg->len);
        }

        if (msg->keepCs == 0|| singleCsChange) {
            LL_GPIO_SetOutputPin(LL_GET_GPIOX(spiDevice->resource.csGroup), LL_GET_HAL_PIN(spiDevice->resource.csPin));
        }
        if (msg->delayUs > 0) {
            ticks = (msg->delayUs / PER_MS_IN_SEC);
            osDelay(ticks);
        }
    }
    return HDF_SUCCESS;
CS_DOWN:
    if (msg->keepCs == 0 || singleCsChange) {
        LL_GPIO_SetOutputPin(LL_GET_GPIOX(spiDevice->resource.csGroup), LL_GET_HAL_PIN(spiDevice->resource.csPin));
    }
    return HDF_SUCCESS;
}

