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

#ifndef HDI_SAMPLE_CPP_CLIENT_H
#define HDI_SAMPLE_CPP_CLIENT_H

#include <hdi_base.h>
#include <iproxy_broker.h>

#include "ifoo.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
class FooProxy : public IProxyBroker<IFoo> {
public:
    explicit FooProxy(const sptr<IRemoteObject> &impl) : IProxyBroker<IFoo>(impl) {}
    virtual ~FooProxy() = default;

    int32_t PingTest(const bool input, bool &output) override;

private:
    static inline BrokerDelegator<FooProxy> delegator_;
};
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS

#endif // HDI_SAMPLE_CPP_CLIENT_H