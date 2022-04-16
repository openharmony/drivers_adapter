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

#ifndef HDI_FOO_STUB_CPP_INF_H
#define HDI_FOO_STUB_CPP_INF_H

#include <hdi_base.h>
#include <ipc_object_stub.h>
#include <object_collector.h>

#include "ifoo.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
class FooStub : public OHOS::IPCObjectStub {
public:
    explicit FooStub(const sptr<IFoo> serviceImpl);
    virtual ~FooStub();

    // function handle start
    int32_t StubPingTest(OHOS::MessageParcel &data, OHOS::MessageParcel &reply, OHOS::MessageOption &option);
    // function handle end

    // base functions
    int32_t OnRemoteRequest(
        uint32_t code, OHOS::MessageParcel &data, OHOS::MessageParcel &reply, OHOS::MessageOption &option);

    sptr<IFoo> ToInterface();

private:
    // Add constructor function to constructor map, only stub object need this
    static inline ObjectDelegator<FooStub, IFoo> objDelegator_;

    sptr<IFoo> impl_;
};
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS

#endif // HDI_FOO_STUB_CPP_INF_H
