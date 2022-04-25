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

#ifndef HDI_PROXY_BROKER_H
#define HDI_PROXY_BROKER_H

#include <iremote_broker.h>
#include <peer_holder.h>
#include <refbase.h>

namespace OHOS {
namespace HDI {
template <typename INTERFACE>
class IProxyBroker : public OHOS::PeerHolder, public OHOS::IRemoteBroker, public INTERFACE {
public:
    explicit IProxyBroker(const sptr<IRemoteObject> &object);
    virtual ~IProxyBroker() = default;
    virtual sptr<INTERFACE> AsInterface();
    sptr<IRemoteObject> AsObject() override;
};

template <typename INTERFACE>
IProxyBroker<INTERFACE>::IProxyBroker(const sptr<IRemoteObject> &object) : PeerHolder(object)
{
}

template <typename INTERFACE>
sptr<INTERFACE> IProxyBroker<INTERFACE>::AsInterface()
{
    return this;
}

template <typename INTERFACE>
sptr<IRemoteObject> IProxyBroker<INTERFACE>::AsObject()
{
    return Remote();
}

template <typename INTERFACE>
inline sptr<INTERFACE> hdi_facecast(const sptr<IRemoteObject> &object)
{
    const std::u16string descriptor = INTERFACE::GetDescriptor();
    BrokerRegistration &registration = BrokerRegistration::Get();
    sptr<IRemoteBroker> broker = registration.NewInstance(descriptor, object);
    INTERFACE *proxyBroker = static_cast<IProxyBroker<INTERFACE> *>(broker.GetRefPtr());
    return static_cast<INTERFACE *>(proxyBroker);
}

template <typename INTERFACE>
inline sptr<IRemoteObject> hdi_objcast(const sptr<INTERFACE> &iface)
{
    IProxyBroker<INTERFACE> *brokerObject = static_cast<IProxyBroker<INTERFACE> *>(iface.GetRefPtr());
    return brokerObject->AsObject();
}
} // namespace HDI
} // namespace OHOS

#endif // HDI_PROXY_BROKER_H
