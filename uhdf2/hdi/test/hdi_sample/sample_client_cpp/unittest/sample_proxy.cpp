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
#include <hdi_support.h>
#include <isample.h>
#include <iservmgr_hdi.h>
#include <message_parcel.h>
#include <string_ex.h>

#include "sample_proxy.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
sptr<ISample> ISample::Get(const std::string &serviceName, bool isStub)
{
    if (!isStub) {
        using namespace OHOS::HDI::ServiceManager::V1_0;
        auto servMgr = IServiceManager::Get();
        if (servMgr == nullptr) {
            return nullptr;
        }

        sptr<IRemoteObject> remote = servMgr->GetService(serviceName.c_str());
        if (remote != nullptr) {
            return hdi_facecast<ISample>(remote);
        }

        HDF_LOGE("%{public}s: get %{public}s failed!", __func__, serviceName.c_str());
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

int32_t SampleProxy::GetInterface(sptr<IFoo> &output)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    data.WriteInterfaceToken(ISample::GetDescriptor());
    int32_t ret = Remote()->SendRequest(CMD_INTERFACE_TRANS_TEST, data, reply, option);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: SendRequest failed, error code is %{public}d", __func__, ret);
        return ret;
    }
    auto remote = reply.ReadRemoteObject();
    if (remote == nullptr) {
        HDF_LOGE("%{public}s: failed to read remote object", __func__);
        return HDF_FAILURE;
    }

    output = hdi_facecast<IFoo>(remote);
    if (output == nullptr) {
        HDF_LOGE("%{public}s: failed to cast foo proxy", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS