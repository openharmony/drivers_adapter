/*
 * Copyright (c) 2022 Talkweb Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <stdlib.h>
#include "hal_gpio.h"
#include "hal_exti.h"
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#include "hcs_macro.h"
#include "hdf_config_macro.h"
#else
#include "device_resource_if.h"
#endif
#include "gpio_core.h"
#include "hdf_log.h"
#include "osal_irq.h"

#define HDF_LOG_TAG gpio_stm_c

static const uint16_t g_stmRealPinMaps[STM32_GPIO_PIN_MAX] = {
    LL_GPIO_PIN_0,
    LL_GPIO_PIN_1,
    LL_GPIO_PIN_2,
    LL_GPIO_PIN_3,
    LL_GPIO_PIN_4,
    LL_GPIO_PIN_5,
    LL_GPIO_PIN_6,
    LL_GPIO_PIN_7,
    LL_GPIO_PIN_8,
    LL_GPIO_PIN_9,
    LL_GPIO_PIN_10,
    LL_GPIO_PIN_11,
    LL_GPIO_PIN_12,
    LL_GPIO_PIN_13,
    LL_GPIO_PIN_14,
    LL_GPIO_PIN_15,
};

typedef struct {
    uint32_t group;
    uint32_t realPin;
    uint32_t pin;
} GpioInflectInfo;

GpioInflectInfo g_gpioPinsMap[STM32_GPIO_PIN_MAX * STM32_GPIO_GROUP_MAX] = {0};

static const GPIO_TypeDef* g_gpioxMaps[STM32_GPIO_GROUP_MAX] = {
    GPIOA,
    GPIOB,
    GPIOC,
    GPIOD,
    GPIOE,
    GPIOF,
    GPIOG,
    GPIOH,
    GPIOI,
};

static const uint32_t g_gpioExitLineMap[STM32_GPIO_PIN_MAX] = {
    LL_EXTI_LINE_0,
    LL_EXTI_LINE_1,
    LL_EXTI_LINE_2,
    LL_EXTI_LINE_3,
    LL_EXTI_LINE_4,
    LL_EXTI_LINE_5,
    LL_EXTI_LINE_6,
    LL_EXTI_LINE_7,
    LL_EXTI_LINE_8,
    LL_EXTI_LINE_9,
    LL_EXTI_LINE_10,
    LL_EXTI_LINE_11,
    LL_EXTI_LINE_12,
    LL_EXTI_LINE_13,
    LL_EXTI_LINE_14,
    LL_EXTI_LINE_15,
};

typedef struct {
    uint32_t pin;
    uint32_t realPin;
    uint32_t mode;
    uint32_t group;
    uint32_t pull;
    uint32_t speed;
    uint32_t outputType;
    uint32_t alternate;
} GpioResource;

enum GpioDeviceState {
    GPIO_DEVICE_UNINITIALIZED = 0x0u,
    GPIO_DEVICE_INITIALIZED = 0x1u,
};

typedef struct {
    uint32_t pinNums;
    GpioResource resource;
    STM32_GPIO_GROUP group; /* gpio config */
} GpioDevice;

static struct GpioCntlr g_stmGpioCntlr;

static HAL_GPIO_EXIT_CFG_T g_gpioExitCfg[STM32_GPIO_PIN_MAX * STM32_GPIO_GROUP_MAX] = {0};

static void OemGpioIrqHdl(uint32_t pin)
{
    GpioCntlrIrqCallback(&g_stmGpioCntlr, pin);
    return;
}

/* HdfDriverEntry method definitions */
static int32_t GpioDriverInit(struct HdfDeviceObject *device);
static void GpioDriverRelease(struct HdfDeviceObject *device);

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_GpioDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "ST_GPIO_MODULE_HDF",
    .Init = GpioDriverInit,
    .Release = GpioDriverRelease,
};
HDF_INIT(g_GpioDriverEntry);

/* GpioMethod method definitions */
static int32_t GpioDevWrite(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t val);
static int32_t GpioDevRead(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t *val);
static int32_t GpioDevSetDir(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t dir);
static int32_t GpioDevGetDir(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t *dir);
static int32_t GpioDevSetIrq(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t mode);
static int32_t GpioDevUnSetIrq(struct GpioCntlr *cntlr, uint16_t gpio);
static int32_t GpioDevEnableIrq(struct GpioCntlr *cntlr, uint16_t gpio);
static int32_t GpioDevDisableIrq(struct GpioCntlr *cntlr, uint16_t gpio);

/* GpioMethod definitions */
struct GpioMethod g_GpioCntlrMethod = {
    .request = NULL,
    .release = NULL,
    .write = GpioDevWrite,
    .read = GpioDevRead,
    .setDir = GpioDevSetDir,
    .getDir = GpioDevGetDir,
    .toIrq = NULL,
    .setIrq = GpioDevSetIrq,
    .unsetIrq = GpioDevUnSetIrq,
    .enableIrq = GpioDevEnableIrq,
    .disableIrq = GpioDevDisableIrq,
};

static void InitGpioClock(STM32_GPIO_GROUP group)
{
    switch (group) {
        case STM32_GPIO_GROUP_A:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
            break;
        case STM32_GPIO_GROUP_B:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
            break;
        case STM32_GPIO_GROUP_C:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
            break;
        case STM32_GPIO_GROUP_D:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
            break;
        case STM32_GPIO_GROUP_E:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
            break;
        case STM32_GPIO_GROUP_F:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
            break;
        case STM32_GPIO_GROUP_G:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);
            break;
        case STM32_GPIO_GROUP_H:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
            break;
        case STM32_GPIO_GROUP_I:
            LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOI);
            break;
        default:
            break;
    }
}

static int32_t InitGpioDevice(GpioDevice* device)
{
    LL_GPIO_InitTypeDef gpioInitStruct = {0};
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    uint32_t halGpio = g_stmRealPinMaps[device->resource.realPin];
    if (halGpio > LL_GPIO_PIN_15 || halGpio < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, halGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    /* init clock */
    InitGpioClock(device->resource.group);

    GPIO_TypeDef* goiox = g_gpioxMaps[device->resource.group];
    if (device->resource.mode & LL_GPIO_MODE_OUTPUT) {
        LL_GPIO_ResetOutputPin(goiox, halGpio);
    }

    gpioInitStruct.Pin = halGpio;
    gpioInitStruct.Mode = device->resource.mode;
    gpioInitStruct.Pull = device->resource.pull;
    gpioInitStruct.Speed = device->resource.speed;
    gpioInitStruct.OutputType = device->resource.outputType;
    gpioInitStruct.Alternate = device->resource.alternate;
    LL_GPIO_Init(goiox, &gpioInitStruct);

    return HDF_SUCCESS;
}

#ifndef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
static int32_t GpioParseHcs(const struct DeviceResourceIface *dri,
    GpioDevice *device, const struct DeviceResourceNode *resourceNode)
{
    GpioResource *resource = NULL;
    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: resource is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    if (dri->GetUint32(resourceNode, "pinNum", &device->pinNums, 0) != HDF_SUCCESS) {
        HDF_LOGE("gpio config read pinNum fail");
        return HDF_FAILURE;
    }

    for (size_t i = 0; i < device->pinNums; i++) {
        if (dri->GetUint32ArrayElem(resourceNode, "pin", i, &resource->pin, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "realPin", i, &resource->realPin, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "mode", i, &resource->mode, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "speed", i, &resource->speed, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "pull", i, &resource->pull, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "output", i, &resource->outputType, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "group", i, &resource->group, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        if (dri->GetUint32ArrayElem(resourceNode, "alternate", i, &resource->alternate, 0) != HDF_SUCCESS) {
            return HDF_FAILURE;
        }

        g_gpioPinsMap[resource->pin].group = resource->group;
        g_gpioPinsMap[resource->pin].realPin = resource->realPin;
        g_gpioPinsMap[resource->pin].pin = resource->pin;

        if (InitGpioDevice(device) != HDF_SUCCESS) {
            HDF_LOGE("InitGpioDevice FAIL\r\n");
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}
#endif

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#define PLATFORM_GPIO_CONFIG HCS_NODE(HCS_NODE(HCS_ROOT, platform), gpio_config)
static uint32_t GetGpioDeviceResource(GpioDevice *device)
{
    uint32_t relPin;
    int32_t ret;
    GpioResource *resource = NULL;
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: resource is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    device->pinNums = HCS_PROP(PLATFORM_GPIO_CONFIG, pinNum);
    uint32_t pins[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, pin));
    uint32_t realPins[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, realPin));
    uint32_t groups[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, group));
    uint32_t modes[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, mode));
    uint32_t speeds[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, speed));
    uint32_t pulls[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, pull));
    uint32_t outputs[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, output));
    uint32_t alternates[] = HCS_ARRAYS(HCS_NODE(PLATFORM_GPIO_CONFIG, alternate));
    for (size_t i = 0; i < device->pinNums; i++) {
        resource->pin = pins[i];
        resource->realPin = realPins[i];
        resource->group = groups[i];
        resource->mode = modes[i];
        resource->speed = speeds[i];
        resource->pull = pulls[i];
        resource->outputType = outputs[i];
        resource->alternate = alternates[i];
        g_gpioPinsMap[resource->pin].group = resource->group;
        g_gpioPinsMap[resource->pin].realPin = resource->realPin;
        g_gpioPinsMap[resource->pin].pin = resource->pin;

        if (InitGpioDevice(device) != HDF_SUCCESS) {
            HDF_LOGE("InitGpioDevice FAIL\r\n");
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}
#else
static int32_t GetGpioDeviceResource(GpioDevice *device, const struct DeviceResourceNode *resourceNode)
{
    int32_t ret;
    struct DeviceResourceIface *dri = NULL;
    if (device == NULL || resourceNode == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (GpioParseHcs(dri, device, resourceNode) != HDF_SUCCESS) {
        HDF_LOGE("gpio config parse hcs fail");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}
#endif

static int32_t AttachGpioDevice(struct GpioCntlr *gpioCntlr, struct HdfDeviceObject *device)
{
    int32_t ret;

    GpioDevice *gpioDevice = NULL;
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    if (device == NULL) {
#else
    if (device == NULL || device->property == NULL) {
#endif
        HDF_LOGE("%s: property is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    gpioDevice = (GpioDevice *)OsalMemAlloc(sizeof(GpioDevice));
    if (gpioDevice == NULL) {
        HDF_LOGE("%s: OsalMemAlloc gpioDevice error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
        ret = GetGpioDeviceResource(gpioDevice);
#else
    ret = GetGpioDeviceResource(gpioDevice, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(gpioDevice);
        return HDF_FAILURE;
    }
    gpioCntlr->priv = gpioDevice;
    gpioCntlr->count = gpioDevice->pinNums;

    return HDF_SUCCESS;
}

static int32_t GpioDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct GpioCntlr *gpioCntlr = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    ret = PlatformDeviceBind(&g_stmGpioCntlr.device, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: bind hdf device failed:%d", __func__, ret);
        return ret;
    }

    gpioCntlr = GpioCntlrFromHdfDev(device);
    if (gpioCntlr == NULL) {
        HDF_LOGE("GpioCntlrFromHdfDev fail\r\n");
        return HDF_DEV_ERR_NO_DEVICE_SERVICE;
    }

    ret = AttachGpioDevice(gpioCntlr, device); /* GpioCntlr add GpioDevice to priv */
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("AttachGpioDevice fail\r\n");
        return HDF_DEV_ERR_ATTACHDEV_FAIL;
    }

    gpioCntlr->ops = &g_GpioCntlrMethod; /* register callback */
    ret = GpioCntlrAdd(gpioCntlr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("GpioCntlrAdd fail %d\r\n", gpioCntlr->start);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t GpioDriverBind(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        HDF_LOGE("device object is NULL\n");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void GpioDriverRelease(struct HdfDeviceObject *device)
{
    struct GpioCntlr *gpioCntlr = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return;
    }

    gpioCntlr = GpioCntlrFromHdfDev(device);
    if (gpioCntlr == NULL) {
        HDF_LOGE("%s: host is NULL", __func__);
        return;
    }

    gpioCntlr->count = 0;
}

/* dev api */
static int32_t GpioDevWrite(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t val)
{
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }
    HDF_LOGE("%s %d ,write pin num %d", __func__, __LINE__, realPin);
    GPIO_TypeDef* gpiox = g_gpioxMaps[g_gpioPinsMap[gpio].group];
    if (val) {
        LL_GPIO_SetOutputPin(gpiox, pinReg);
    } else {
        LL_GPIO_ResetOutputPin(gpiox, pinReg);
    }

    return HDF_SUCCESS;
}

static int32_t GpioDevRead(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t *val)
{
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    uint16_t value = 0;
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }

    GPIO_TypeDef* gpiox = g_gpioxMaps[g_gpioPinsMap[gpio].group];
    value = LL_GPIO_ReadInputPin(gpiox, pinReg);
    *val = value;

    return HDF_SUCCESS;
}

static int32_t GpioDevSetDir(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t dir)
{
    (void)cntlr;
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    uint16_t value = 0;
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }
    GPIO_TypeDef* gpiox = g_gpioxMaps[g_gpioPinsMap[gpio].group];
    LL_GPIO_SetPinMode(gpiox, pinReg, dir);

    return HDF_SUCCESS;
}

static int32_t GpioDevGetDir(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t *dir)
{
    (void)cntlr;
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    uint16_t value = 0;
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }
    GPIO_TypeDef* gpiox = g_gpioxMaps[g_gpioPinsMap[gpio].group];
    value = LL_GPIO_GetPinMode(gpiox, pinReg);
    *dir = value;

    return HDF_SUCCESS;
}

static int32_t GpioDevSetIrq(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t mode)
{
    (void)cntlr;
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }

    if (mode == OSAL_IRQF_TRIGGER_RISING) {
        g_gpioExitCfg[gpio].trigger = LL_EXTI_TRIGGER_RISING;
    } else if (mode == OSAL_IRQF_TRIGGER_FALLING) {
        g_gpioExitCfg[gpio].trigger = LL_EXTI_TRIGGER_FALLING;
    } else {
        HDF_LOGE("%s %d, error mode:%d", __func__, __LINE__, mode);
        return HDF_ERR_NOT_SUPPORT;
    }

    return HDF_SUCCESS;
}

static int32_t GpioDevUnSetIrq(struct GpioCntlr *cntlr, uint16_t gpio)
{
    (void)cntlr;
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }

    return HDF_SUCCESS;
}

static int32_t GpioDevEnableIrq(struct GpioCntlr *cntlr, uint16_t gpio)
{
    (void)cntlr;
    LL_EXTI_InitConfig exitInitConfig = {0};
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }

    exitInitConfig.Exithandler = OemGpioIrqHdl;
    exitInitConfig.Gpiox = g_gpioxMaps[g_gpioPinsMap[gpio].group];
    exitInitConfig.initType.Line_0_31 = g_gpioExitLineMap[g_gpioPinsMap[gpio].realPin];
    exitInitConfig.initType.LineCommand = ENABLE;
    exitInitConfig.initType.Mode = LL_EXTI_MODE_IT;
    exitInitConfig.PinReg = pinReg;
    exitInitConfig.initType.Trigger = g_gpioExitCfg[gpio].trigger;

    LL_SETUP_EXTI(&exitInitConfig, g_gpioPinsMap[gpio].realPin, gpio, g_gpioPinsMap[gpio].group);

    return HDF_SUCCESS;
}

static int32_t GpioDevDisableIrq(struct GpioCntlr *cntlr, uint16_t gpio)
{
    (void)cntlr;
    LL_EXTI_InitConfig exitInitConfig = {0};
    uint16_t realPin = g_gpioPinsMap[gpio].realPin;
    uint32_t pinReg = g_stmRealPinMaps[realPin];
    if (pinReg > LL_GPIO_PIN_15 || pinReg < LL_GPIO_PIN_0) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, realPin);
        return HDF_ERR_NOT_SUPPORT;
    }

    exitInitConfig.Exithandler = NULL;
    exitInitConfig.Gpiox = g_gpioxMaps[g_gpioPinsMap[gpio].group];
    exitInitConfig.initType.Line_0_31 = g_gpioExitLineMap[g_gpioPinsMap[gpio].realPin];
    exitInitConfig.initType.LineCommand = DISABLE;
    exitInitConfig.initType.Mode = LL_EXTI_MODE_IT;
    exitInitConfig.PinReg = pinReg;
    exitInitConfig.initType.Trigger = g_gpioExitCfg[gpio].trigger;
    LL_SETUP_EXTI(&exitInitConfig, g_gpioPinsMap[gpio].realPin, gpio, g_gpioPinsMap[gpio].group);

    return HDF_SUCCESS;
}