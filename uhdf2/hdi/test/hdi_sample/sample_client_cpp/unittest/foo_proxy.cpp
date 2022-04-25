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
#include <hdi_base.h>
#include <message_parcel.h>

#include "ifoo.h"

#include "foo_proxy.h"

namespace OHOS {
namespace HDI {
namespace Sample {
namespace V1_0 {
int32_t FooProxy::PingTest(const bool input, bool &output)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(IFoo::GetDescriptor())) {
        return HDF_FAILURE;
    }
    if (!data.WriteBool(input)) {
        HDF_LOGE("%{public}s: write bool input failed!", __func__);
        return HDF_FAILURE;
    }

    int32_t ret = Remote()->SendRequest(CMD_FOO_PING, data, reply, option);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: SendRequest failed, error code is %{public}d", __func__, ret);
        return ret;
    }
    output = reply.ReadBool();

    return HDF_SUCCESS;
}
} // namespace V1_0
} // namespace Sample
} // namespace HDI
} // namespace OHOS