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

#include "buffer_handle_sequenceable.h"

#include <message_parcel.h>
#include "buffer_handle_parcel.h"

namespace OHOS {
namespace HDI {
namespace Sequenceable {
namespace V1_0 {
BufferHandleSequenceable::BufferHandleSequenceable() : bufferHandle_(nullptr)
{
}

BufferHandleSequenceable::BufferHandleSequenceable(BufferHandle *bufferHandle)
    : bufferHandle_(bufferHandle)
{
}

BufferHandleSequenceable::~BufferHandleSequenceable()
{
}

bool BufferHandleSequenceable::Marshalling(Parcel &parcel) const
{
    if (bufferHandle_ == nullptr) {
        return false;
    }

    MessageParcel &reply = static_cast<MessageParcel&>(parcel);
    return WriteBufferHandle(reply, *bufferHandle_);
}

sptr<BufferHandleSequenceable> BufferHandleSequenceable::Unmarshalling(Parcel &parcel)
{
    MessageParcel &data = static_cast<MessageParcel&>(parcel);
    BufferHandle *bufferHandle = ReadBufferHandle(data);
    if (bufferHandle == nullptr) {
        return nullptr;
    }
    sptr<BufferHandleSequenceable> bufferHandleSeq = new BufferHandleSequenceable(bufferHandle);
    return bufferHandleSeq;
}

BufferHandle *BufferHandleSequenceable::GetBufferHandle()
{
    return bufferHandle_;
}

void BufferHandleSequenceable::SetBufferHandle(BufferHandle *bufferHandle)
{
    bufferHandle_ = bufferHandle;
}
} // namespace V1_0
} // namespace Sequenceable
} // namespace HDI
} // namespace OHOS