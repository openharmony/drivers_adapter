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

#ifndef HDI_STUB_COLLECTOR_H
#define HDI_STUB_COLLECTOR_H

#include <hdf_remote_service.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct StubConstructor {
    struct HdfRemoteService **(*constructor)(void *);
    void (*destructor)(struct HdfRemoteService **);
};
void StubConstructorRegister(const char *ifDesc, struct StubConstructor *constructor);
void StubConstructorUnregister(const char *ifDesc, struct StubConstructor *constructor);
struct HdfRemoteService **StubCollectorGetOrNewObject(const char *ifDesc, void *servPtr);
void StubCollectorRemoveObject(const char *ifDesc, void *servPtr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // HDI_STUB_COLLECTOR_H
