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
#include "pwm_core.h"
#include "hdf_log.h"
#include "hdf_base_hal.h"
#include "stm32f4xx_ll_tim.h"

#define GPIO_STR_MAX_LENGTH 32

typedef enum {
    PWM_CH1 = 0,
    PWM_CH2,
    PWM_CH3,
    PWM_CH4,
    PWM_CH_MAX
} PWM_CH;

typedef enum {
    PWM_TIM1 = 0,
    PWM_TIM2,
    PWM_TIM3,
    PWM_TIM4,
    PWM_TIM5,
    PWM_TIM6,
    PWM_TIM7,
    PWM_TIM8,
    PWM_TIM9,
    PWM_TIM10,
    PWM_TIM11,
    PWM_TIM12,
    PWM_TIM13,
    PWM_TIM14,
    PWM_TIM_MAX
} PWM_TIM;

typedef struct {
    PWM_CH pwmCh;
    PWM_TIM pwmTim;
    uint32_t prescaler;
    uint32_t timPeroid;
    uint32_t realHz;
} PwmResource;

typedef struct {
    LL_TIM_InitTypeDef timInitStruct;
    LL_TIM_OC_InitTypeDef timOcInitStruct;
} PwmConfig;

typedef struct {
    struct IDeviceIoService ioService;
    PwmConfig stPwmCfg;
    struct PwmConfig *cfg;
    PwmResource resource;
} PwmDevice;

typedef struct {
    uint32_t period;
    uint32_t duty;
    PWM_TIM pwmCh;
    PWM_TIM pwmTim;
} PwmFreqArg;

static TIM_TypeDef* g_stTimMap[PWM_TIM_MAX] = {
    TIM1,
    TIM2,
    TIM3,
    TIM4,
    TIM5,
    TIM6,
    TIM7,
    TIM8,
    TIM9,
    TIM10,
    TIM11,
    TIM12,
    TIM13,
    TIM14,
};

static uint32_t g_stChannelMap[PWM_CH_MAX] = {
    LL_TIM_CHANNEL_CH1,
    LL_TIM_CHANNEL_CH2,
    LL_TIM_CHANNEL_CH3,
    LL_TIM_CHANNEL_CH4,
};

static uint32_t g_stTimIrqMap[PWM_TIM_MAX] = {
    TIM1_CC_IRQn,
    TIM2_IRQn,
    TIM3_IRQn,
    TIM4_IRQn,
    TIM5_IRQn,
    TIM6_DAC_IRQn,
    TIM7_IRQn,
    TIM8_CC_IRQn,
    TIM1_BRK_TIM9_IRQn,
    TIM1_UP_TIM10_IRQn,
    TIM1_TRG_COM_TIM11_IRQn,
    TIM8_BRK_TIM12_IRQn,
    TIM8_UP_TIM13_IRQn,
    TIM8_TRG_COM_TIM14_IRQn,
};

static uint32_t g_stTimFreq[PWM_TIM_MAX] = { // tim2-tim7, tim12-tim14 is 84M，TIM1、TIM8~TIM11 is 168M
    168000000,
    84000000,
    84000000,
    84000000,
    84000000,
    84000000,
    84000000,
    168000000,
    168000000,
    168000000,
    168000000,
    84000000,
    84000000,
};

#define PER_SEC_NSEC          1000000000

static int32_t PwmDevSetConfig(struct PwmDev *pwm, struct PwmConfig *config);
static int32_t PwmDevOpen(struct PwmDev *pwm);
static int32_t PwmDevClose(struct PwmDev *pwm);

struct PwmMethod g_pwmmethod = {
    .setConfig = PwmDevSetConfig,
    .open = PwmDevOpen,
    .close = PwmDevClose,
};

static void InitPwmClock(PWM_TIM tim)
{
    switch (tim) {
        case PWM_TIM1:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
            break;
        case PWM_TIM2:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
            break;
        case PWM_TIM3:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
            break;
        case PWM_TIM4:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);
            break;
        case PWM_TIM5:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM5);
            break;
        case PWM_TIM6:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);
            break;
        case PWM_TIM7:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);
            break;
        case PWM_TIM8:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8);
            break;
        case PWM_TIM9:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM9);
            break;
        case PWM_TIM10:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM10);
            break;
        case PWM_TIM11:
            LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM11);
            break;
        case PWM_TIM12:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM12);
            break;
        case PWM_TIM13:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM13);
            break;
        case PWM_TIM14:
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM14);
            break;
        default:
            break;
    }
}

#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
#define PWM_FIND_CONFIG(node, name, resource) \
    do { \
        if (strcmp(HCS_PROP(node, match_attr), name) == 0) { \
            uint8_t tim = HCS_PROP(node, pwmTim); \
            uint8_t ch = HCS_PROP(node, pwmCh); \
            uint8_t prescaler = HCS_PROP(node, prescaler); \
            resource->pwmCh = ch; \
            resource->pwmTim = tim; \
            resource->prescaler = prescaler; \
            result = HDF_SUCCESS; \
        } \
    } while (0)
#define PLATFORM_CONFIG HCS_NODE(HCS_ROOT, platform)
#define PLATFORM_PWM_CONFIG HCS_NODE(HCS_NODE(HCS_ROOT, platform), pwm_config)
static uint32_t GetPwmDeviceResource(PwmDevice *device, const char *deviceMatchAttr)
{
    int32_t result = HDF_FAILURE;
    PwmResource *resource = NULL;
    if (device == NULL || deviceMatchAttr == NULL) {
        HDF_LOGE("%s: device or deviceMatchAttr is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &device->resource;
#if HCS_NODE_HAS_PROP(PLATFORM_CONFIG, pwm_config)
    HCS_FOREACH_CHILD_VARGS(PLATFORM_PWM_CONFIG, PWM_FIND_CONFIG, deviceMatchAttr, resource);
#endif
    if (result != HDF_SUCCESS) {
        HDF_LOGE("resourceNode %s is NULL\r\n", deviceMatchAttr);
    }

    return result;
}
#else
static int32_t GetPwmDeviceResource(PwmDevice *device, const struct DeviceResourceNode *resourceNode)
{
    struct DeviceResourceIface *dri = NULL;
    PwmResource *resource = NULL;

    if (device == NULL || resourceNode == NULL) {
        HDF_LOGE("resource or device is NULL\r\n");
        return HDF_ERR_INVALID_PARAM;
    }

    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("resource is NULL\r\n");
        return HDF_ERR_INVALID_OBJECT;
    }

    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint8 == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid\r\n");
        return HDF_ERR_INVALID_PARAM;
    }

    if (dri->GetUint8(resourceNode, "pwmTim", &resource->pwmTim, 0) != HDF_SUCCESS) {
        HDF_LOGE("read pwmPin fail\r\n");
        return HDF_ERR_INVALID_PARAM;
    }

    if (resource->pwmTim == PWM_TIM6 || resource->pwmTim == PWM_TIM7) {
        HDF_LOGE("unsupport tim\r\n");
        return HDF_ERR_INVALID_PARAM;
    }

    if (dri->GetUint8(resourceNode, "pwmCh", &resource->pwmCh, 0) != HDF_SUCCESS) {
        HDF_LOGE("read pwmCh fail\r\n");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "prescaler", &resource->prescaler, 0) != HDF_SUCCESS) {
        HDF_LOGE("read prescaler fail\r\n");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}
#endif

static int32_t AttachPwmDevice(struct PwmDev *host, struct HdfDeviceObject *device)
{
    int32_t ret;
    PwmDevice *pwmDevice = NULL;
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    if (device == NULL || host == NULL) {
#else
    if (device == NULL || device->property == NULL || host == NULL) {
#endif
        HDF_LOGE("%s: param is NULL\r\n", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    pwmDevice = (PwmDevice *)OsalMemAlloc(sizeof(PwmDevice));
    if (pwmDevice == NULL) {
        HDF_LOGE("%s: OsalMemAlloc pwmDevice error\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
#ifdef LOSCFG_DRIVERS_HDF_CONFIG_MACRO
    ret = GetPwmDeviceResource(pwmDevice, device->deviceMatchAttr);
#else
    ret = GetPwmDeviceResource(pwmDevice, device->property);
#endif
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(pwmDevice);
        return HDF_FAILURE;
    }
    host->priv = pwmDevice;
    host->num = pwmDevice->resource.pwmTim;

    return HDF_SUCCESS;
}

static int32_t PwmDriverBind(struct HdfDeviceObject *device);
static int32_t PwmDriverInit(struct HdfDeviceObject *device);
static void PwmDriverRelease(struct HdfDeviceObject *device);

struct HdfDriverEntry g_pwmDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "ST_HDF_PLATFORM_PWM",
    .Bind = PwmDriverBind,
    .Init = PwmDriverInit,
    .Release = PwmDriverRelease,
};
HDF_INIT(g_pwmDriverEntry);

static int32_t PwmDriverBind(struct HdfDeviceObject *device)
{
    struct PwmDev *devService = NULL;
    if (device == NULL) {
        HDF_LOGE("hdfDevice object is null!\r\n");
        return HDF_FAILURE;
    }

    devService = (struct PwmDev *)OsalMemCalloc(sizeof(struct PwmDev));
    if (devService == NULL) {
        HDF_LOGE("malloc pwmDev failed\n");
    }
    device->service = &devService->service;
    devService->device = device;

    return HDF_SUCCESS;
}

static int32_t PwmDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct PwmDev *host = NULL;

    if (device == NULL) {
        HDF_LOGE("%s: device is NULL\r\n", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct PwmDev *)device->service;
    if (host == NULL) {
        HDF_LOGE("%s: host is NULL\r\n", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = AttachPwmDevice(host, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s:attach error\r\n", __func__);
        return HDF_DEV_ERR_ATTACHDEV_FAIL;
    }

    host->method = &g_pwmmethod;
    ret = PwmDeviceAdd(device, host);
    if (ret != HDF_SUCCESS) {
        PwmDeviceRemove(device, host);
        OsalMemFree(host->device);
        OsalMemFree(host);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    return HDF_SUCCESS;
}

static void PwmDriverRelease(struct HdfDeviceObject *device)
{
    struct PwmDev *host = NULL;

    if (device == NULL || device->service == NULL) {
        HDF_LOGE("device is null\r\n");
        return;
    }

    host = (struct PwmDev *)device->service;
    if (host != NULL && host->device != NULL) {
        host->method = NULL;
        OsalMemFree(host->device);
        OsalMemFree(host);
        host->device = NULL;
        host = NULL;
    }

    device->service = NULL;
    host = NULL;

    return;
}

static int32_t InitPwmFreqAndPeriod(const struct PwmConfig *config, PwmFreqArg* arg, const PwmResource *resource)
{
    if (arg == NULL) {
        HDF_LOGE("null ptr\r\n");
        return HDF_FAILURE;
    }

    uint32_t freq = 0;
    uint32_t period = 0;
    uint32_t duty = 0;
    uint32_t realHz = 0;

    if (config->period != 0) {
        freq = (uint32_t)(PER_SEC_NSEC / config->period);
    } else {
        HDF_LOGE("invalid div\r\n");
        return HDF_FAILURE;
    }

    realHz = (uint32_t)(((double)g_stTimFreq[arg->pwmTim]) / ((double)(resource->prescaler + 1)));

    if (freq != 0) {
        period = (uint32_t)(realHz / freq);
    } else {
        HDF_LOGE("invalid div\r\n");
        return HDF_FAILURE;
    }

    if (config->period != 0) {
        duty = (uint32_t)(((double)config->duty / (double)config->period) * period);
    } else {
        HDF_LOGE("invalid div\r\n");
        return HDF_FAILURE;
    }

    arg->period = period;
    arg->duty = duty;

    return HDF_SUCCESS;
}

static void InitTimPwm(const PwmFreqArg* arg, const struct PwmConfig *config,
    PwmConfig *pwmCfg, const PwmResource *resource)
{
    if (arg == NULL) {
        HDF_LOGE("null ptr\r\n");
        return;
    }

    pwmCfg->timInitStruct.Autoreload = arg->period - 1; // if period is 1000 20KHz/1000=20Hz，period is 50ms
    pwmCfg->timInitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    pwmCfg->timInitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    pwmCfg->timInitStruct.Prescaler = resource->prescaler;
    LL_TIM_Init(g_stTimMap[arg->pwmTim], &pwmCfg->timInitStruct);
    LL_TIM_EnableARRPreload(g_stTimMap[arg->pwmTim]);
    LL_TIM_SetClockSource(g_stTimMap[arg->pwmTim], LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_SetTriggerOutput(g_stTimMap[arg->pwmTim], LL_TIM_TRGO_RESET);
    LL_TIM_DisableMasterSlaveMode(g_stTimMap[arg->pwmTim]);

    pwmCfg->timOcInitStruct.OCMode = LL_TIM_OCMODE_PWM1; // PWM1 mode
    pwmCfg->timOcInitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
    pwmCfg->timOcInitStruct.CompareValue = arg->duty;
    if (config->polarity == PWM_NORMAL_POLARITY) {
        pwmCfg->timOcInitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
    } else {
        pwmCfg->timOcInitStruct.OCPolarity = LL_TIM_OCPOLARITY_LOW;
    }
    pwmCfg->timOcInitStruct.OCIdleState = LL_TIM_OCPOLARITY_LOW;

    LL_TIM_OC_Init(g_stTimMap[arg->pwmTim], g_stChannelMap[arg->pwmCh], &pwmCfg->timOcInitStruct);
    LL_TIM_OC_DisableFast(g_stTimMap[arg->pwmTim], g_stChannelMap[arg->pwmCh]);
    
    if (arg->pwmTim == PWM_TIM1 || arg->pwmTim == PWM_TIM8) {
        LL_TIM_BDTR_InitTypeDef bdtrInitStruct = {0};
        bdtrInitStruct.OSSRState = LL_TIM_OSSR_DISABLE;
        bdtrInitStruct.OSSIState = LL_TIM_OSSI_DISABLE;
        bdtrInitStruct.LockLevel = LL_TIM_LOCKLEVEL_OFF;
        bdtrInitStruct.DeadTime = 0; // dead area time 200ns 0x28
        bdtrInitStruct.BreakState = LL_TIM_BREAK_ENABLE;
        bdtrInitStruct.BreakPolarity = LL_TIM_BREAK_POLARITY_HIGH;
        bdtrInitStruct.AutomaticOutput = LL_TIM_AUTOMATICOUTPUT_ENABLE;
        LL_TIM_BDTR_Init(g_stTimMap[arg->pwmTim], &bdtrInitStruct);
    }

    NVIC_SetPriority(g_stTimIrqMap[arg->pwmTim], NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 1, 1));
    NVIC_EnableIRQ(g_stTimIrqMap[arg->pwmTim]);

    if (arg->pwmTim == PWM_TIM1 || arg->pwmTim == PWM_TIM8) {
        LL_TIM_EnableAutomaticOutput(g_stTimMap[arg->pwmTim]);
        LL_TIM_GenerateEvent_UPDATE(g_stTimMap[arg->pwmTim]);
    }
    LL_TIM_EnableCounter(g_stTimMap[arg->pwmTim]);
    LL_TIM_CC_EnableChannel(g_stTimMap[arg->pwmTim], g_stChannelMap[arg->pwmCh]);
    LL_TIM_OC_EnablePreload(g_stTimMap[arg->pwmTim], g_stChannelMap[arg->pwmCh]);

    return;
}

static void DeInitTimPwm(PWM_TIM pwmId, PWM_CH pwmCh)
{
    LL_TIM_DeInit(g_stTimMap[pwmId]);
    LL_TIM_OC_DisableClear(g_stTimMap[pwmId], g_stChannelMap[pwmCh]);
    NVIC_DisableIRQ(g_stTimIrqMap[pwmId]);

    LL_TIM_DisableCounter(g_stTimMap[pwmId]);
    LL_TIM_CC_DisableChannel(g_stTimMap[pwmId], g_stChannelMap[pwmCh]);
    LL_TIM_OC_DisablePreload(g_stTimMap[pwmId], g_stChannelMap[pwmCh]);

    if (pwmId== PWM_TIM1 || pwmId == PWM_TIM8) {
        LL_TIM_DisableAutomaticOutput(g_stTimMap[pwmId]);
    }

    return;
}

static int32_t PwmDevSetConfig(struct PwmDev *pwm, struct PwmConfig *config)
{
    PwmDevice *prvPwm = NULL;
    PwmConfig *pwmCfg = NULL;
    PWM_TIM pwmId;
    PWM_CH pwmCh;
    PwmResource *resource = NULL;

    if (pwm == NULL || config == NULL || (config->period > PER_SEC_NSEC)) {
        HDF_LOGE("%s\r\n", __FUNCTION__);
        return HDF_FAILURE;
    }

    prvPwm = (struct PwmDevice *)PwmGetPriv(pwm);
    if (prvPwm == NULL) {
        return HDF_FAILURE;
    }
    resource = &prvPwm->resource;
    if (resource == NULL) {
        return HDF_FAILURE;
    }
    pwmCfg = &prvPwm->stPwmCfg;
    if (pwmCfg == NULL) {
        return HDF_FAILURE;
    }
    pwmId = prvPwm->resource.pwmTim;
    pwmCh = prvPwm->resource.pwmCh;

    if (config->status == PWM_ENABLE_STATUS) {
        PwmFreqArg arg = {0};
        arg.pwmCh = pwmCh;
        arg.pwmTim = pwmId;
        if (InitPwmFreqAndPeriod(config, &arg, resource) != HDF_SUCCESS) {
            HDF_LOGE("calculate freq and period failed!\r\n");
            return HDF_FAILURE;
        }

        InitPwmClock(pwmId);
        InitTimPwm(&arg, config, pwmCfg, resource);
    } else {
        DeInitTimPwm(pwmId, pwmCh);
    }

    return HDF_SUCCESS;
}

static int32_t PwmDevOpen(struct PwmDev *pwm)
{
    if (pwm == NULL) {
        HDF_LOGE("%s\r\n", __FUNCTION__);
        return HDF_ERR_INVALID_PARAM;
    }

    return HDF_SUCCESS;
}

static int32_t PwmDevClose(struct PwmDev *pwm)
{
    PwmDevice *prvPwm = NULL;
    PWM_TIM pwmId;
    PWM_CH pwmCh;
    if (pwm == NULL) {
        HDF_LOGE("%s\r\n", __FUNCTION__);
        return HDF_ERR_INVALID_PARAM;
    }
    prvPwm = (PwmDevice *)PwmGetPriv(pwm);
    if (prvPwm == NULL) {
        HDF_LOGE("%s\r\n", __FUNCTION__);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    pwmId = prvPwm->resource.pwmTim;
    pwmCh = prvPwm->resource.pwmCh;
    LL_TIM_DeInit(g_stTimMap[pwmId]);
    LL_TIM_OC_DisableClear(g_stTimMap[pwmId], g_stChannelMap[pwmCh]);
    NVIC_DisableIRQ(g_stTimIrqMap[pwmId]);

    return HDF_SUCCESS;
}