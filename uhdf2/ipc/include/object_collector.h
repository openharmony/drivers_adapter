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

#ifndef HDI_OBJECT_MAPPER_H
#define HDI_OBJECT_MAPPER_H

#include <hdi_base.h>
#include <iremote_object.h>
#include <map>
#include <mutex>
#include <refbase.h>
#include <string>

namespace OHOS {
namespace HDI {
class ObjectCollector {
public:
    using Constructor = std::function<sptr<IRemoteObject>(const sptr<HdiBase> &interface)>;
    ObjectCollector() = default;

    static const ObjectCollector &GetInstance();

    bool ConstructorRegister(const std::u16string &interfaceName, const Constructor &constructor) const;
    void ConstructorUnRegister(const std::u16string &interfaceName) const;
    sptr<IRemoteObject> NewObject(const sptr<HdiBase> &interface, const std::u16string &interfaceName) const;
    sptr<IRemoteObject> GetOrNewObject(const sptr<HdiBase> &interface, const std::u16string &interfaceName) const;
    bool RemoveObject(const sptr<HdiBase> &interface) const;

private:
    sptr<IRemoteObject> NewObjectLocked(const sptr<HdiBase> &interface, const std::u16string &interfaceName) const;
    static std::map<const std::u16string, const Constructor> constructorMapper_;
    static std::map<HdiBase *, IRemoteObject *> interfaceObjectCollector_;
    static std::mutex mutex_;
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
