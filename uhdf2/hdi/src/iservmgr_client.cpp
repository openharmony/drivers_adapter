/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <iproxy_broker.h>
#include <iremote_stub.h>
#include <iservice_registry.h>
#include <object_collector.h>

#include "iservmgr_hdi.h"

namespace OHOS {
namespace HDI {
namespace ServiceManager {
namespace V1_0 {
constexpr int DEVICE_SERVICE_MANAGER_SA_ID = 5100;
constexpr int DEVSVC_MANAGER_GET_SERVICE = 3;
constexpr int DEVSVC_MANAGER_REGISTER_SVCLISTENER = 4;
constexpr int DEVSVC_MANAGER_UNREGISTER_SVCLISTENER = 5;
constexpr int DEVSVC_MANAGER_LIST_ALL_SERVICE = 6;

class ServiceManagerProxy : public IProxyBroker<IServiceManager> {
public:
    explicit ServiceManagerProxy(const sptr<IRemoteObject> &impl) : IProxyBroker<IServiceManager>(impl) {}
    ~ServiceManagerProxy() {}

    sptr<IRemoteObject> GetService(const char *serviceName) override;
    int32_t ListAllService(std::vector<HdiServiceInfo> &serviceInfos) override;
    int32_t RegisterServiceStatusListener(sptr<IServStatListener> listener, uint16_t deviceClass) override;
    int32_t UnregisterServiceStatusListener(sptr<IServStatListener> listener) override;

private:
    static inline BrokerDelegator<ServiceManagerProxy> delegator_;
};

sptr<IServiceManager> IServiceManager::Get()
{
    auto saManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (saManager == nullptr) {
        HDF_LOGE("failed to get sa manager");
        return nullptr;
    }
    sptr<IRemoteObject> remote = saManager->GetSystemAbility(DEVICE_SERVICE_MANAGER_SA_ID);
    if (remote != nullptr) {
        return new ServiceManagerProxy(remote);
    }

    HDF_LOGE("failed to get sa hdf service manager");
    return nullptr;
}

int32_t ServiceManagerProxy::RegisterServiceStatusListener(
    ::OHOS::sptr<IServStatListener> listener, uint16_t deviceClass)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteUint16(deviceClass) ||
        !data.WriteRemoteObject(listener->AsObject())) {
        return HDF_FAILURE;
    }

    int status = Remote()->SendRequest(DEVSVC_MANAGER_REGISTER_SVCLISTENER, data, reply, option);
    if (status) {
        HDF_LOGE("failed to register servstat listener, %{public}d", status);
    }
    return status;
}

int32_t ServiceManagerProxy::UnregisterServiceStatusListener(::OHOS::sptr<IServStatListener> listener)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteRemoteObject(listener->AsObject())) {
        return HDF_FAILURE;
    }

    int status = Remote()->SendRequest(DEVSVC_MANAGER_UNREGISTER_SVCLISTENER, data, reply, option);
    if (status) {
        HDF_LOGE("failed to unregister servstat listener, %{public}d", status);
    }
    return status;
}

sptr<IRemoteObject> ServiceManagerProxy::GetService(const char *serviceName)
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteCString(serviceName)) {
        return nullptr;
    }

    MessageOption option;
    int status = Remote()->SendRequest(DEVSVC_MANAGER_GET_SERVICE, data, reply, option);
    if (status) {
        HDF_LOGE("get hdi service %{public}s failed, %{public}d", serviceName, status);
        return nullptr;
    }
    HDF_LOGD("get hdi service %{public}s success ", serviceName);
    return reply.ReadRemoteObject();
}

static void HdfDevMgrDbgFillServiceInfo(std::vector<HdiServiceInfo> &serviceInfos, MessageParcel &reply)
{
    while (true) {
        HdiServiceInfo info;
        const char *servName = reply.ReadCString();
        if (servName == nullptr) {
            break;
        }
        info.serviceName = servName;
        info.devClass = reply.ReadUint16();
        serviceInfos.push_back(info);
    }
    return;
}

int32_t ServiceManagerProxy::ListAllService(std::vector<HdiServiceInfo> &serviceInfos)
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        return HDF_FAILURE;
    }

    MessageOption option;
    int status = Remote()->SendRequest(DEVSVC_MANAGER_LIST_ALL_SERVICE, data, reply, option);
    if (status != HDF_SUCCESS) {
        HDF_LOGE("list all service info failed, %{public}d", status);
        return status;
    } else {
        HdfDevMgrDbgFillServiceInfo(serviceInfos, reply);
    }
    HDF_LOGD("get all service info success");
    return status;
}
} // namespace V1_0
} // namespace ServiceManager
} // namespace HDI
} // namespace OHOS