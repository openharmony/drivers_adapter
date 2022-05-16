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
#include <hdf_log.h>
#include <hdf_sbuf_ipc.h>
#include <hdi_support.h>
#include <object_collector.h>
#include <string_ex.h>

#include "foo_service.h"

#include "sample_service_stub.h"

#define HDF_LOG_TAG sample_stub

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
sptr<ISample> ISample::Get(const std::string &serviceName, bool isStub)
{
    if (!isStub) {
        return nullptr;
    }

    std::string desc = Str16ToStr8(ISample::GetDescriptor());
    void *impl = LoadHdiImpl(desc.data(), serviceName.c_str());
    if (impl == nullptr) {
        HDF_LOGE("failed to load hdi impl %{public}s", desc.data());
        return nullptr;
    }

    return reinterpret_cast<ISample *>(impl);
}

sptr<ISample> ISample::Get(bool isStub)
{
    return ISample::Get("sample_service", isStub);
}

SampleServiceStub::SampleServiceStub(const sptr<ISample> &serviceImpl) :
    IPCObjectStub(ISample::GetDescriptor()), impl_(serviceImpl)
{
}

SampleServiceStub::~SampleServiceStub()
{
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t SampleServiceStub::SampleStubGetInterface(
    MessageParcel &data, MessageParcel &reply, MessageOption &option) const
{
    if (data.ReadInterfaceToken() != ISample::GetDescriptor()) {
        HDF_LOGE("failed to check interface");
        return HDF_ERR_INVALID_PARAM;
    }
    sptr<IFoo> interfaceObj;
    auto ret = impl_->GetInterface(interfaceObj);
    if (ret != HDF_SUCCESS || interfaceObj == nullptr) {
        HDF_LOGE("%{public}s: call failed", __func__);
        return HDF_FAILURE;
    }

    sptr<IRemoteObject> stubObject = ObjectCollector::GetInstance().GetOrNewObject(interfaceObj, IFoo::GetDescriptor());
    if (stubObject == nullptr) {
        HDF_LOGE("%{public}s: failed to cast interface to stub object", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    reply.WriteRemoteObject(stubObject);
    return HDF_SUCCESS;
}

int32_t SampleServiceStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    switch (code) {
        case CMD_INTERFACE_TRANS_TEST:
            return SampleStubGetInterface(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: not support cmd %{public}d", __func__, code);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    return HDF_SUCCESS;
}
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS
