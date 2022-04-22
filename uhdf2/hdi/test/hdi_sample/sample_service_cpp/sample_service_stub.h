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

#ifndef HDI_SAMPLE_SERVICE_STUB_CPP_INF_H
#define HDI_SAMPLE_SERVICE_STUB_CPP_INF_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>

#include "sample_service.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
class SampleServiceStub : public OHOS::IPCObjectStub {
public:
    explicit SampleServiceStub(const sptr<ISample> &serviceImpl);
    virtual ~SampleServiceStub();

    int32_t SampleStubGetInterface(MessageParcel &data, MessageParcel &reply, MessageOption &option) const;

    int32_t OnRemoteRequest(
uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

private:
    static inline ObjectDelegator<SampleServiceStub, ISample> objDelegator_;
    sptr<ISample> impl_;
};
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS

#endif // HDI_SAMPLE_SERVICE_STUB_CPP_INF_H