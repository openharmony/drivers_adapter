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

#ifndef HDI_OBJECT_COLLECTOR_H
#define HDI_OBJECT_COLLECTOR_H

#include <string>
#include <hdi_base.h>
#include <iremote_object.h>
#include <map>
#include <mutex>
#include <refbase.h>

namespace OHOS {
namespace HDI {
class ObjectCollector {
public:
    using Constructor = std::function<sptr<IRemoteObject>(const sptr<HdiBase> &interface)>;

    static ObjectCollector &GetInstance();

    bool ConstructorRegister(const std::u16string &interfaceName, const Constructor &constructor);
    void ConstructorUnRegister(const std::u16string &interfaceName);
    sptr<IRemoteObject> NewObject(const sptr<HdiBase> &interface, const std::u16string &interfaceName);
    sptr<IRemoteObject> GetOrNewObject(const sptr<HdiBase> &interface, const std::u16string &interfaceName);
    bool RemoveObject(const sptr<HdiBase> &interface);

private:
    ObjectCollector() = default;
    sptr<IRemoteObject> NewObjectLocked(const sptr<HdiBase> &interface, const std::u16string &interfaceName);
    std::map<const std::u16string, const Constructor> constructorMapper_;
    std::map<HdiBase *, IRemoteObject *> interfaceObjectCollector_;
    std::mutex mutex_;
};

template <typename OBJECT, typename INTERFACE>
class ObjectDelegator {
public:
    ObjectDelegator();
    ~ObjectDelegator();

private:
    ObjectDelegator(const ObjectDelegator &) = delete;
    ObjectDelegator(ObjectDelegator &&) = delete;
    ObjectDelegator &operator=(const ObjectDelegator &) = delete;
    ObjectDelegator &operator=(ObjectDelegator &&) = delete;
};

template <typename OBJECT, typename INTERFACE>
ObjectDelegator<OBJECT, INTERFACE>::ObjectDelegator()
{
    ObjectCollector::GetInstance().ConstructorRegister(
        INTERFACE::GetDescriptor(), [](const sptr<HdiBase> &interface) -> sptr<IRemoteObject> {
            return new OBJECT(static_cast<INTERFACE *>(interface.GetRefPtr()));
        });
}

template <typename OBJECT, typename INTERFACE>
ObjectDelegator<OBJECT, INTERFACE>::~ObjectDelegator()
{
    ObjectCollector::GetInstance().ConstructorUnRegister(INTERFACE::GetDescriptor());
}
} // namespace HDI
} // namespace OHOS

#endif // HDI_OBJECT_MAPPER_H
