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

#include "stub_collector.h"
#include <hdf_log.h>
#include <map>
#include <mutex>

static std::map<std::string, StubConstructor *> g_constructorMap;
static std::map<void *, HdfRemoteService **> g_stubMap;
static std::mutex g_consMapLock;
static std::mutex g_stubMapLock;

void StubConstructorRegister(const char *ifDesc, struct StubConstructor *constructor)
{
    if (ifDesc == nullptr || constructor == nullptr) {
        return;
    }

    const std::lock_guard<std::mutex> lock(g_consMapLock);
    if (g_constructorMap.find(ifDesc) != g_constructorMap.end()) {
        HDF_LOGE("repeat registration stub constructor for if %{public}s", ifDesc);
        return;
    }
    g_constructorMap.emplace(std::make_pair(ifDesc, constructor));
    return;
}

void StubConstructorUnregister(const char *ifDesc, struct StubConstructor *constructor)
{
    if (ifDesc == nullptr || constructor == nullptr) {
        return;
    }

    const std::lock_guard<std::mutex> lock(g_consMapLock);
    g_constructorMap.erase(ifDesc);
    return;
}

struct HdfRemoteService **StubCollectorGetOrNewObject(const char *ifDesc, void *servPtr)
{
    if (ifDesc == nullptr || servPtr == nullptr) {
        return nullptr;
    }
    const std::lock_guard<std::mutex> stublock(g_stubMapLock);
    auto stub = g_stubMap.find(servPtr);
    if (stub != g_stubMap.end()) {
        return stub->second;
    }

    HDF_LOGI("g_constructorMap size %{public}zu", g_constructorMap.size());
    for (auto &consruct : g_constructorMap) {
        HDF_LOGI("g_constructorMap it: %{public}s", consruct.first.c_str());
    }

    const std::lock_guard<std::mutex> lock(g_consMapLock);
    auto constructor = g_constructorMap.find(ifDesc);
    if (constructor == g_constructorMap.end()) {
        HDF_LOGE("no stub constructor for %{public}s", ifDesc);
        return nullptr;
    }

    if (constructor->second->constructor == nullptr) {
        HDF_LOGE("no stub constructor method for %{public}s", ifDesc);
        return nullptr;
    }

    HdfRemoteService **stubObject = constructor->second->constructor(servPtr);
    if (stubObject == nullptr) {
        HDF_LOGE("failed to construct stub obj %{public}s", ifDesc);
        return nullptr;
    }
    g_stubMap.insert(std::make_pair(servPtr, stubObject));
    return stubObject;
}

void StubCollectorRemoveObject(const char *ifDesc, void *servPtr)
{
    if (ifDesc == nullptr || servPtr == nullptr) {
        return;
    }

    const std::lock_guard<std::mutex> stublock(g_stubMapLock);
    auto stub = g_stubMap.find(servPtr);
    if (stub == g_stubMap.end()) {
        return;
    }
    const std::lock_guard<std::mutex> lock(g_consMapLock);
    auto constructor = g_constructorMap.find(ifDesc);
    if (constructor == g_constructorMap.end()) {
        HDF_LOGE("no stub constructor for %{public}s", ifDesc);
        return;
    }

    if (constructor->second->destructor != nullptr) {
        constructor->second->destructor(stub->second);
    }

    g_stubMap.erase(servPtr);
}
