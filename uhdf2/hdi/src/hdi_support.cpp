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

#include "hdi_support.h"
#include <dlfcn.h>
#include <securec.h>
#include <string>
#include <string_ex.h>
#include <unistd.h>
#include <vector>

#include "hdf_base.h"
#include "hdf_log.h"

#define HDF_LOG_TAG load_hdi

#ifdef __ARM64__
#define HDI_SO_PATH HDF_LIBRARY_DIR "64"
#else
#define HDI_SO_PATH HDF_LIBRARY_DIR
#endif

namespace {
constexpr int VERSION_SIZE = 2;
constexpr size_t INTERFACE_SIZE = 5;
constexpr size_t INTERFACE_VERSION_INDEX = 3;
constexpr size_t INTERFACE_NAME_INDEX = 2;
} // namespace

static int ParseInterface(
    const std::string &fullName, std::string &interface, uint32_t &versionMajor, uint32_t &versionMinor)
{
    std::vector<std::string> spInfo;
    OHOS::SplitStr(fullName, ".", spInfo, false, true);
    if (spInfo.size() != INTERFACE_SIZE) {
        HDF_LOGE("invlid interface format");
        return HDF_FAILURE;
    }

    interface = spInfo[INTERFACE_NAME_INDEX];
    int ret = sscanf_s(spInfo[INTERFACE_VERSION_INDEX].data(), "V%u_%u", &versionMajor, &versionMinor);
    if (ret != VERSION_SIZE) {
        HDF_LOGE("failed to get interface version\n");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

void *LoadHdiImpl(const char *fullIfName)
{
    char path[PATH_MAX + 1] = {0};
    char resolvedPath[PATH_MAX + 1] = {0};
    // interface name like "OHOS.HDI.Sample.V1_0.IFoo", the last two are version and interface name
    if (fullIfName == nullptr) {
        HDF_LOGE("fullIfName is nullptr");
        return nullptr;
    }

    HDF_LOGD("load interface impl: %{public}s", fullIfName);
    std::string fullName = fullIfName;
    std::string interfaceName;
    uint32_t versionMajor = 0;
    uint32_t versionMinor = 0;
    if (ParseInterface(fullIfName, interfaceName, versionMajor, versionMinor) != HDF_SUCCESS) {
        HDF_LOGE("failed to parse hdi interface info");
        return nullptr;
    }
    // hdi implement name like libsample_service_1.0.z.so
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "%s/lib%s_service_%u.%u.z.so", HDI_SO_PATH,
            OHOS::LowerStr(interfaceName).data(), versionMajor, versionMinor) < 0) {
        HDF_LOGE("%{public}s snprintf_s failed", __func__);
        return nullptr;
    }
    if (realpath(path, resolvedPath) == nullptr || strncmp(resolvedPath, HDI_SO_PATH, strlen(HDI_SO_PATH)) != 0) {
        HDF_LOGE("%{public}s invalid hdi impl so name %{public}s", __func__, path);
        return nullptr;
    }
    void *handler = dlopen(resolvedPath, RTLD_LAZY);
    if (handler == nullptr) {
        HDF_LOGE("%{public}s dlopen failed %{public}s", __func__, dlerror());
        return nullptr;
    }
    std::string symName = interfaceName.append("ImplGetInstance");
    using HdiImplInstanceFunc = void *(*)(void);
    HdiImplInstanceFunc hdiImplInstanceFunc = (HdiImplInstanceFunc)dlsym(handler, symName.data());
    if (hdiImplInstanceFunc == nullptr) {
        HDF_LOGE("%{public}s dlsym failed %{public}s", __func__, dlerror());
        dlclose(handler);
        return nullptr;
    }
    return hdiImplInstanceFunc();
}
