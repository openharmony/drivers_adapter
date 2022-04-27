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
#include <iservice_registry.h>
#include <object_collector.h>

#include "idevmgr_hdi.h"
#include "iservmgr_hdi.h"

#define HDF_LOG_TAG idevmgr_client

namespace OHOS {
namespace HDI {
namespace DeviceManager {
namespace V1_0 {
enum DevmgrCmdId : uint32_t {
    DEVMGR_SERVICE_ATTACH_DEVICE_HOST = 1,
    DEVMGR_SERVICE_ATTACH_DEVICE,
    DEVMGR_SERVICE_DETACH_DEVICE,
    DEVMGR_SERVICE_LOAD_DEVICE,
    DEVMGR_SERVICE_UNLOAD_DEVICE,
    DEVMGR_SERVICE_QUERY_DEVICE,
    DEVMGR_SERVICE_LIST_ALL_DEVICE,
};

class DeviceManagerProxy : public IProxyBroker<IDeviceManager> {
public:
    explicit DeviceManagerProxy(const sptr<IRemoteObject> &impl) : IProxyBroker<IDeviceManager>(impl) {}
    ~DeviceManagerProxy() {}
    int32_t LoadDevice(const std::string &serviceName) override;
    int32_t UnloadDevice(const std::string &serviceName) override;
    int32_t ListAllDevice(std::vector<HdiDevHostInfo> &deviceInfos) override;

private:
    static inline BrokerDelegator<DeviceManagerProxy> delegator_;
};

int32_t DeviceManagerProxy::LoadDevice(const std::string &serviceName)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    HDF_LOGI("load device: %{public}s", serviceName.data());
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        return HDF_FAILURE;
    }
    if (!data.WriteCString(serviceName.data())) {
        return HDF_FAILURE;
    }

    int status = Remote()->SendRequest(DEVMGR_SERVICE_LOAD_DEVICE, data, reply, option);
    if (status) {
        HDF_LOGE("load device failed, %{public}d", status);
    }
    return status;
}

int32_t DeviceManagerProxy::UnloadDevice(const std::string &serviceName)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    HDF_LOGI("unload device: %{public}s", serviceName.data());
    if (!data.WriteInterfaceToken(DeviceManagerProxy::GetDescriptor())) {
        return HDF_FAILURE;
    }
    if (!data.WriteCString(serviceName.data())) {
        return HDF_FAILURE;
    }

    int status = Remote()->SendRequest(DEVMGR_SERVICE_UNLOAD_DEVICE, data, reply, option);
    if (status) {
        HDF_LOGE("unload device failed, %{public}d", status);
    }
    return status;
}

static void HdfDevMgrDbgFillDeviceInfo(std::vector<HdiDevHostInfo> &deviceInfos, MessageParcel &reply)
{
    while (true) {
        uint32_t devId;
        uint32_t devCnt;
        struct HdiDevHostInfo info;
        const char *hostName = reply.ReadCString();
        if (hostName == nullptr) {
            break;
        }
        info.hostName = hostName;
        info.hostId = reply.ReadUint32();
        devCnt = reply.ReadUint32();
        for (uint32_t i = 0; i < devCnt; i++) {
            devId = reply.ReadUint32();
            info.devId.push_back(devId);
        }
        deviceInfos.push_back(info);
    }
    return;
}

int32_t DeviceManagerProxy::ListAllDevice(std::vector<HdiDevHostInfo> &deviceInfos)
{
    MessageParcel data;
    MessageParcel reply;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        return HDF_FAILURE;
    }

    MessageOption option;
    int status = Remote()->SendRequest(DEVMGR_SERVICE_LIST_ALL_DEVICE, data, reply, option);
    if (status != HDF_SUCCESS) {
        HDF_LOGE("list all device info failed, %{public}d", status);
    } else {
        HdfDevMgrDbgFillDeviceInfo(deviceInfos, reply);
    }
    return status;
}

sptr<IDeviceManager> IDeviceManager::Get()
{
    auto servmgr = ServiceManager::V1_0::IServiceManager::Get();
    if (servmgr == nullptr) {
        HDF_LOGE("failed to get hdi service manager");
        return nullptr;
    }
    sptr<IRemoteObject> remote = servmgr->GetService("hdf_device_manager");
    if (remote != nullptr) {
        return hdi_facecast<IDeviceManager>(remote);
    }

    HDF_LOGE("hdf device manager not exist");
    return nullptr;
}
} // namespace V1_0
} // namespace DeviceManager
} // namespace HDI
} // namespace OHOS