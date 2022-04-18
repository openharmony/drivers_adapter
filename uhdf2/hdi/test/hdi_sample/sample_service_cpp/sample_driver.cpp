/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <hdf_base.h>
#include <hdf_device_desc.h>
#include <hdf_log.h>
#include <hdf_sbuf_ipc.h>
#include <osal_mem.h>

#include "sample_service_stub.h"

#define HDF_LOG_TAG sample_service_cpp

using namespace OHOS::HDI::Sample::V1_0;

struct HdfSampleService {
    struct IDeviceIoService ioservice;
    OHOS::sptr<OHOS::IRemoteObject> sampleStub;
};

static int32_t SampleServiceDispatch(
    struct HdfDeviceIoClient *client, int cmdId, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    struct HdfSampleService *hdfSampleService =
        CONTAINER_OF(client->device->service, struct HdfSampleService, ioservice);

    OHOS::MessageParcel *dataParcel = nullptr;
    OHOS::MessageParcel *replyParcel = nullptr;
    OHOS::MessageOption option;
    if (SbufToParcel(data, &dataParcel) != HDF_SUCCESS || SbufToParcel(reply, &replyParcel) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: failed trans sbuf to parcel", __func__);
        return HDF_FAILURE;
    }
    return hdfSampleService->sampleStub->SendRequest(cmdId, *dataParcel, *replyParcel, option);
}

static int HdfSampleDriverInit(struct HdfDeviceObject *deviceObject)
{
    (void)deviceObject;
    HDF_LOGI("HdfSampleDriverInit enter, new hdi impl");
    return HDF_SUCCESS;
}

static int HdfSampleDriverBind(struct HdfDeviceObject *deviceObject)
{
    HDF_LOGI("HdfSampleDriverBind enter!");
    struct HdfSampleService *hdfSampleService = new HdfSampleService();
    if (hdfSampleService == nullptr) {
        HDF_LOGE("HdfSampleDriverBind: OsalMemAlloc HdfSampleService failed!");
        return HDF_FAILURE;
    }
    hdfSampleService->sampleStub = nullptr;
    auto sampleImpl = ISample::Get(true);
    if (sampleImpl == nullptr) {
        HDF_LOGE("HdfSampleDriverBind: failed to get ISample implement");
        return HDF_FAILURE;
    }

    hdfSampleService->sampleStub =
        OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(sampleImpl, ISample::GetDescriptor());
    if (hdfSampleService->sampleStub == nullptr) {
        HDF_LOGE("HdfSampleDriverBind: failed to get ISample stub object");
        return HDF_FAILURE;
    }

    hdfSampleService->ioservice.Dispatch = SampleServiceDispatch;
    hdfSampleService->ioservice.Open = NULL;
    hdfSampleService->ioservice.Release = NULL;

    deviceObject->service = &hdfSampleService->ioservice;
    return HDF_SUCCESS;
}

static void HdfSampleDriverRelease(struct HdfDeviceObject *deviceObject)
{
    HDF_LOGI("HdfSampleDriverRelease called");
    if (deviceObject->service == nullptr) {
        HDF_LOGE("HdfSampleDriverRelease not initted");
        return;
    }
    struct HdfSampleService *hdfSampleService = CONTAINER_OF(deviceObject->service, struct HdfSampleService, ioservice);
    hdfSampleService->sampleStub = nullptr;
    delete hdfSampleService;
}

static struct HdfDriverEntry g_sampleDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "sample_service_cpp",
    .Bind = HdfSampleDriverBind,
    .Init = HdfSampleDriverInit,
    .Release = HdfSampleDriverRelease,
};

#ifndef __cplusplus
extern "C" {
#endif

HDF_INIT(g_sampleDriverEntry);

#ifndef __cplusplus
}
#endif