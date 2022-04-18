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

#ifndef HDI_SAMPLE_CPP_INF_H
#define HDI_SAMPLE_CPP_INF_H

#include <list>
#include <map>
#include <string>
#include <vector>
#include <hdi_base.h>
#include "ifoo.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
struct StructSample {
    int8_t first;
    int16_t second;
};

enum SampleCmdId {
    CMD_INTERFACE_TRANS_TEST,
};

class ISample : public HdiBase {
public:
    DECLARE_HDI_DESCRIPTOR(u"OHOS.HDI.Sample.V1_0.ISample");
    ISample() = default;
    virtual ~ISample() = default;
    virtual int32_t GetInterface(sptr<IFoo> &output) = 0;

    static sptr<ISample> Get(bool isStub = false);
    static sptr<ISample> Get(const std::string &serviceName, bool isStub = false);
};
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS

#endif // HDI_SAMPLE_CPP_INF_H