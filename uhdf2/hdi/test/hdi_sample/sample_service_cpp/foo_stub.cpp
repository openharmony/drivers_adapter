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
#include <object_collector.h>
#include <string_ex.h>

#include "ifoo.h"

#include "foo_stub.h"


namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
FooStub::FooStub(const sptr<IFoo> serviceImpl) : IPCObjectStub(IFoo::GetDescriptor()), impl_(serviceImpl) {}

FooStub::~FooStub()
{
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t FooStub::StubPingTest(OHOS::MessageParcel &data, OHOS::MessageParcel &reply, OHOS::MessageOption &option)
{
    if (data.ReadInterfaceToken() != IFoo::GetDescriptor()) {
        HDF_LOGE("failed to check interface token");
        return HDF_ERR_INVALID_PARAM;
    }
    bool input = data.ReadBool();
    bool output = false;

    if (impl_ == nullptr) {
        HDF_LOGE("invalid service impl");
        return HDF_ERR_INVALID_OBJECT;
    }

    int ret = impl_->PingTest(input, output);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    reply.WriteBool(output);
    return ret;
}

int32_t FooStub::OnRemoteRequest(
    uint32_t code, OHOS::MessageParcel &data, OHOS::MessageParcel &reply, OHOS::MessageOption &option)
{
    switch (code) {
        case CMD_FOO_PING:
            return StubPingTest(data, reply, option);
        default:
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
}

sptr<IFoo> FooStub::ToInterface()
{
    return impl_;
}
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS