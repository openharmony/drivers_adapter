/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "sample_hdi.h"
#include <hdf_base.h>
#include <hdf_device_object.h>
#include <hdf_dlist.h>
#include <hdf_log.h>
#include <hdf_remote_service.h>

#define HDF_LOG_TAG sample_driver

struct SampleDevice {
    struct DListHead listNode;
    struct HdfDeviceObject *devobj;
};

struct DListHead g_sampleDeviceList = { NULL };

int32_t SampleServicePing(struct HdfDeviceObject *device, const char *info, char **infoOut)
{
    (void)device;
    HDF_LOGI("Sample:info is %{public}s", info);
    *infoOut = strdup(info);
    return 0;
}

int32_t SampleServiceSum(struct HdfDeviceObject *device, int32_t x0, int32_t x1, int32_t *result)
{
    (void)device;
    *result = x0 + x1;
    return 0;
}

int32_t SampleServiceCallback(struct HdfDeviceObject *device, struct HdfRemoteService *callback, int32_t code)
{
    (void)device;
    struct HdfSBuf *dataSbuf = HdfSBufTypedObtain(SBUF_IPC);
    HdfSbufWriteInt32(dataSbuf, code);
    int ret = callback->dispatcher->Dispatch(callback, 0, dataSbuf, NULL);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("failed to do callback, ret = %{public}d", ret);
    }
    HdfSBufRecycle(dataSbuf);
    return ret;
}

int32_t SampleServiceRegisterDevice(struct HdfDeviceObject *device, const char *servName)
{
    struct HdfDeviceObject *dev = HdfDeviceObjectAlloc(device, "libsample_driver.z.so");
    if (dev == NULL) {
        HDF_LOGE("failed to alloc device object");
        return HDF_DEV_ERR_NO_DEVICE;
    }

    if (HdfDeviceObjectRegister(dev) != HDF_SUCCESS) {
        HDF_LOGE("failed to register device");
        HdfDeviceObjectRelease(dev);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    if (HdfDeviceObjectPublishService(dev, servName, SERVICE_POLICY_CAPACITY, 0) != HDF_SUCCESS) {
        HDF_LOGE("failed to publish device service %{public}s", servName);
        HdfDeviceObjectRelease(dev);
        return HDF_DEV_ERR_NO_DEVICE;
    }

    HDF_LOGE("publish device service %{public}s success", servName);
    struct SampleDevice *sampleDev = OsalMemAlloc(sizeof(struct SampleDevice));
    if (sampleDev == NULL) {
        HdfDeviceObjectRelease(dev);
        return HDF_DEV_ERR_NO_MEMORY;
    }

    sampleDev->devobj = dev;
    if (g_sampleDeviceList.next == NULL) {
        DListHeadInit(&g_sampleDeviceList);
    }
    DListInsertTail(&sampleDev->listNode, &g_sampleDeviceList);

    HDF_LOGI("register device %{public}s success", servName);
    return HDF_SUCCESS;
}

int32_t SampleServiceUnregisterDevice(struct HdfDeviceObject *device, const char *servName)
{
    struct SampleDevice *sampleDev = NULL;
    struct SampleDevice *sampleDevTmp = NULL;

    DLIST_FOR_EACH_ENTRY_SAFE(sampleDev, sampleDevTmp, &g_sampleDeviceList, struct SampleDevice, listNode) {
        if (sampleDev->devobj == NULL || HdfDeviceGetServiceName(sampleDev->devobj) == NULL) {
            DListRemove(&sampleDev->listNode);
            OsalMemFree(sampleDev);
            continue;
        }

        if (strcmp(HdfDeviceGetServiceName(sampleDev->devobj), servName) == 0) {
            HdfDeviceObjectRelease(sampleDev->devobj);
            DListRemove(&sampleDev->listNode);
            OsalMemFree(sampleDev);
            HDF_LOGI("remove device %{public}s success", servName);
        }
    }

    return HDF_SUCCESS;
}

static const struct SampleHdi g_sampleHdiImpl = {
    .ping = SampleServicePing,
    .sum = SampleServiceSum,
    .callback = SampleServiceCallback,
    .registerDevice = SampleServiceRegisterDevice,
    .unregisterDevice = SampleServiceUnregisterDevice,
};

const struct SampleHdi *SampleHdiImplInstance()
{
    return &g_sampleHdiImpl;
}