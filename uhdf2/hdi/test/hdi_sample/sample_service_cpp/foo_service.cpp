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

#include "ifoo.h"

#include "foo_service.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
int32_t FooService::PingTest(bool input, bool &output)
{
    HDF_LOGI("FooService::PingTest, in=%{public}d", input);
    output = input;
    return HDF_SUCCESS;
}
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS