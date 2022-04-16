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

#ifndef HDI_SAMPLE_SERVICE_CPP_INF_H
#define HDI_SAMPLE_SERVICE_CPP_INF_H

#include "isample.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
class SampleService : public ISample {
public:
    SampleService() {}
    ~SampleService() override {}

    int32_t GetInterface(sptr<IFoo> &output) override;
};
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS

#endif // HDI_SAMPLE_SERVICE_CPP_INF_H
