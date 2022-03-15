/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gpio_wm.h"
#include <stdlib.h>
#include "gpio_if.h"
#include "device_resource_if.h"
#include "osal_irq.h"
#include "hdf_log.h"
#include "wm_io.h"
#include "wm_gpio.h"

#define HDF_LOG_TAG gpioDriver
#define WM_IO_MAX_GPIO_PIN_NUM 48

static struct GpioCntlr gpioCntlr;
struct wmGpioIrqHandler {
    uint8_t port;
};

enum tls_io_name g_gpioPinReflectionMap[WM_IO_MAX_GPIO_PIN_NUM] = {0};
static struct wmGpioIrqHandler g_wmGpioIrqHandler[WM_IO_MAX_GPIO_PIN_NUM] = {0};
enum tls_gpio_irq_trig g_gpioIrqCfg[WM_IO_MAX_GPIO_PIN_NUM] = {0};


GpioIrqFunc GpioIrqHdl()
{
    uint16_t ret;

    for (size_t i = 0; i < WM_IO_MAX_GPIO_PIN_NUM; i++) {
        ret = tls_get_gpio_irq_status(g_wmGpioIrqHandler[i].port);
        if ((enum tls_io_name)g_gpioPinReflectionMap[i] && (ret != 0)) {
            GpioCntlrIrqCallback(&gpioCntlr, i);
            return;
        }
    }
}

/* dispatch */
int32_t GpioDispatch(struct HdfDeviceIoClient *client, int cmdId, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || client->device == NULL || data == NULL || reply == NULL) {
        HDF_LOGE("%s: client or client->device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    return HDF_SUCCESS;
}

/* dev api */
static int32_t GpioDevWrite(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t val)
{
    (void)cntlr;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error wmGpio:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    tls_gpio_write((enum tls_io_name)wmGpio, val);
    return HDF_SUCCESS;
}

static int32_t GpioDevRead(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t *val)
{
    (void)cntlr;
    uint16_t value;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error wmGpio:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    value = tls_gpio_read((enum tls_io_name)wmGpio);
    *val = value;
    return HDF_SUCCESS;
}

static int32_t GpioDevSetDir(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t dir)
{
    (void)cntlr;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error wmGpio:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    switch (dir) {
        case GPIO_DIR_OUT:
            dir = WM_GPIO_DIR_OUTPUT;
            break;
        case GPIO_DIR_IN:
            dir = WM_GPIO_DIR_INPUT;
            break;
    }

    tls_gpio_cfg((enum tls_io_name)wmGpio, (enum tls_gpio_dir)dir, WM_GPIO_ATTR_FLOATING);

    return HDF_SUCCESS;
}

static int32_t GpioDevSetIrq(struct GpioCntlr *cntlr, uint16_t gpio, uint16_t mode)
{
    (void)cntlr;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error wmGpio:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    g_wmGpioIrqHandler[wmGpio].port = gpio;
    tls_gpio_isr_register((enum tls_io_name)wmGpio, (tls_gpio_irq_callback)GpioIrqHdl, NULL);

    switch (mode) {
        case OSAL_IRQF_TRIGGER_RISING:
            g_gpioIrqCfg[wmGpio] = WM_GPIO_IRQ_TRIG_RISING_EDGE;
            break;
        case OSAL_IRQF_TRIGGER_FALLING:
            g_gpioIrqCfg[wmGpio] = WM_GPIO_IRQ_TRIG_FALLING_EDGE;
            break;
        case OSAL_IRQF_TRIGGER_HIGH:
            g_gpioIrqCfg[wmGpio] = WM_GPIO_IRQ_TRIG_HIGH_LEVEL;
            break;
        case OSAL_IRQF_TRIGGER_LOW:
            g_gpioIrqCfg[wmGpio] = WM_GPIO_IRQ_TRIG_LOW_LEVEL;
            break;
    }

    return HDF_SUCCESS;
}

static int32_t GpioDevUnSetIrq(struct GpioCntlr *cntlr, uint16_t gpio)
{
    (void)cntlr;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error wmGpio:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }
    return HDF_SUCCESS;
}

static int32_t GpioDevEnableIrq(struct GpioCntlr *cntlr, uint16_t gpio)
{
    (void)cntlr;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    // config gpio interrupt
    tls_gpio_irq_enable((enum tls_io_name)wmGpio, g_gpioIrqCfg[wmGpio]);
    return HDF_SUCCESS;
}

static int32_t GpioDevDisableIrq(struct GpioCntlr *cntlr, uint16_t gpio)
{
    (void)cntlr;
    uint16_t wmGpio = g_gpioPinReflectionMap[gpio];
    if ((enum tls_io_name)wmGpio > WM_IO_MAX_GPIO_PIN_NUM) {
        HDF_LOGE("%s %d, error pin:%d", __func__, __LINE__, wmGpio);
        return HDF_ERR_NOT_SUPPORT;
    }

    tls_gpio_irq_disable((enum tls_io_name)wmGpio);
    return HDF_SUCCESS;
}

/* GpioMethod definitions */
struct GpioMethod g_GpioCntlrMethod = {
    .request = NULL,
    .release = NULL,
    .write = GpioDevWrite,
    .read = GpioDevRead,
    .setDir = GpioDevSetDir,
    .getDir = NULL,
    .toIrq = NULL,
    .setIrq = GpioDevSetIrq,
    .unsetIrq = GpioDevUnSetIrq,
    .enableIrq = GpioDevEnableIrq,
    .disableIrq = GpioDevDisableIrq,
};

static uint32_t GetGpioDeviceResource(struct GpioDevice *device,
    const struct DeviceResourceNode *resourceNode)
{
    uint32_t relPin;
    int32_t ret;
    struct GpioResource *resource = NULL;
    struct DeviceResourceIface *dri = NULL;
    if (device == NULL || resourceNode == NULL) {
        HDF_LOGE("%s: device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    resource = &device->resource;
    if (resource == NULL) {
        HDF_LOGE("%s: resource is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    dri = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (dri == NULL || dri->GetUint32 == NULL) {
        HDF_LOGE("DeviceResourceIface is invalid");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (dri->GetUint32(resourceNode, "groupNum", &resource->groupNum, 0) != HDF_SUCCESS) {
        HDF_LOGE("gpio config read groupNum fail");
        return HDF_FAILURE;
    }

    if (dri->GetUint32(resourceNode, "pinNum", &resource->pinNum, 0) != HDF_SUCCESS) {
        HDF_LOGE("gpio config read pinNum fail");
        return HDF_FAILURE;
    }

    for (size_t i = 0; i < resource->groupNum; i++) {
        if (dri->GetUint32ArrayElem(resourceNode, "config", i, &resource->config, 0) != HDF_SUCCESS) {
            HDF_LOGE("gpio config read config fail");
            return HDF_FAILURE;
        }
    }
    device->config = resource->config;
    return HDF_SUCCESS;
}

static int32_t AttachGpioDevice(struct GpioCntlr *gpioCntlr, struct HdfDeviceObject *device)
{
    int32_t ret;

    struct GpioDevice *gpioDevice = NULL;
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: property is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    gpioDevice = (struct GpioDevice *)OsalMemAlloc(sizeof(struct GpioDevice));
    if (gpioDevice == NULL) {
        HDF_LOGE("%s: OsalMemAlloc gpioDevice error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = GetGpioDeviceResource(gpioDevice, device->property);
    if (ret != HDF_SUCCESS) {
        (void)OsalMemFree(gpioDevice);
        return HDF_FAILURE;
    }

    gpioCntlr->count = gpioDevice->resource.pinNum;

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

    gpioCntlr = GpioCntlrFromHdfDev(device);
    if (gpioCntlr == NULL) {
        HDF_LOGE("GpioCntlrFromHdfDev fail\r\n");
        return HDF_DEV_ERR_NO_DEVICE_SERVICE;
    }

    ret = AttachGpioDevice(gpioCntlr, device); // GpioCntlr add GpioDevice to priv
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("AttachGpioDevice fail\r\n");
        return HDF_DEV_ERR_ATTACHDEV_FAIL;
    }

    gpioCntlr->ops = &g_GpioCntlrMethod; // register callback
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
        HDF_LOGE("Sample device object is null!");
        return HDF_ERR_INVALID_PARAM;
    }

    gpioCntlr.device.hdfDev = device;
    device->service = gpioCntlr.device.hdfDev;

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

    gpioCntlr->ops = NULL;
    OsalMemFree(gpioCntlr);
}

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_GpioDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "WM_GPIO_MODULE_HDF",
    .Bind = GpioDriverBind,
    .Init = GpioDriverInit,
    .Release = GpioDriverRelease,
};
HDF_INIT(g_GpioDriverEntry);
