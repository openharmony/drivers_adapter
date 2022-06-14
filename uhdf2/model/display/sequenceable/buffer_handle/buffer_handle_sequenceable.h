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

#ifndef OHOS_HDI_BUFFER_HANDLE_SEQUENCE_H
#define OHOS_HDI_BUFFER_HANDLE_SEQUENCE_H

#include <parcel.h>

#include "buffer_handle.h"

namespace OHOS {
namespace HDI {
namespace Sequenceable {
namespace V1_0 {
using OHOS::Parcelable;
using OHOS::Parcel;
using OHOS::sptr;

class BufferHandleSequenceable : public Parcelable {
public:
    BufferHandleSequenceable();
    explicit BufferHandleSequenceable(BufferHandle *bufferHandle);
    virtual ~BufferHandleSequenceable();
    
    bool Marshalling(Parcel &parcel) const override;
    static sptr<BufferHandleSequenceable> Unmarshalling(Parcel &parcel);

    BufferHandle *GetBufferHandle();
    void SetBufferHandle(BufferHandle *bufferHandle);

private:
    BufferHandle *bufferHandle_;
};
} // namespace V1_0
} // namespace Sequenceable
} // namespace HDI
} // namespace OHOS
#endif