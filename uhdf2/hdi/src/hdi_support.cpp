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
#include <regex>
#include <securec.h>
#include <string>
#include <unistd.h>

#include "hdf_base.h"
#include "hdf_log.h"

#define HDF_LOG_TAG load_hdi

#ifdef __ARM64__
#define HDI_SO_PATH HDF_LIBRARY_DIR "64"
#else
#define HDI_SO_PATH HDF_LIBRARY_DIR
#endif

namespace {
constexpr size_t INTERFACE_MATCH_RESIZE = 4;
constexpr size_t INTERFACE_VERSION_MAJOR_INDEX = 1;
constexpr size_t INTERFACE_VERSION_MINOR_INDEX = 2;
constexpr size_t INTERFACE_NAME_INDEX = 3;
static const std::regex reInfDesc("[a-zA-Z_][a-zA-Z0-9_]*(?:\\.[a-zA-Z_][a-zA-Z0-9_]*)*\\."
                                  "[V|v]([0-9]+)_([0-9]+)\\."
                                  "([a-zA-Z_][a-zA-Z0-9_]*)");
} // namespace

static int32_t ParseInterface(const std::string &desc, std::string &interface, uint32_t &versionMajor,
    uint32_t &versionMinor)
{
    std::smatch result;
    if (!std::regex_match(desc, result, reInfDesc)) {
        return HDF_FAILURE;
    }

    if (result.size() < INTERFACE_MATCH_RESIZE) {
        return HDF_FAILURE;
    }

    versionMajor = std::stoul(result[INTERFACE_VERSION_MAJOR_INDEX]);
    versionMinor = std::stoul(result[INTERFACE_VERSION_MINOR_INDEX]);
    std::string interfaceName = result[INTERFACE_NAME_INDEX];

    interface = interfaceName[0] == 'I' ? interfaceName.substr(1) : interfaceName;
    if (interface.empty()) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static std::string TransFileName(const std::string& interfaceName)
{
    if (interfaceName.empty()) {
        return interfaceName;
    }

    std::string result;
    for (size_t i = 0; i < interfaceName.size(); i++) {
        char c = interfaceName[i];
        if (std::isupper(c) != 0) {
            if (i > 1) {
                result += '_';
            }
            result += std::tolower(c);
        } else {
            result += c;
        }
    }
    return result;
}

/* service name: xxx_service
 * interface descriptor name: ohos.hdi.sample.v1_0.IFoo
 * interface: Foo
 * versionMajor: 1
 * versionMinor: 0
 * library name: libfoo_xxx_service_1.0.z.so
 * method name: FooImplGetInstance
 */
void *LoadHdiImpl(const char *desc, const char *serviceName)
{
    char path[PATH_MAX + 1] = {0};
    char resolvedPath[PATH_MAX + 1] = {0};
    // interface descriptor name like "ohos.hdi.sample.v1_0.IFoo", the last two are version and interface base name
    if (desc == nullptr || serviceName == nullptr) {
        HDF_LOGE("%{public}s interface descriptor or service name is nullptr", __func__);
        return nullptr;
    }

    if (strlen(desc) == 0 || strlen(serviceName) == 0) {
        HDF_LOGE("%{public}s invalid interface descriptor or service name", __func__);
        return nullptr;
    }

    std::string interfaceName;
    uint32_t versionMajor = 0;
    uint32_t versionMinor = 0;
    if (ParseInterface(desc, interfaceName, versionMajor, versionMinor) != HDF_SUCCESS) {
        HDF_LOGE("failed to parse hdi interface info from '%{public}s'", desc);
        return nullptr;
    }

    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "%s/lib%s_%s_%u.%u.z.so", HDI_SO_PATH,
            TransFileName(interfaceName).c_str(), serviceName, versionMajor, versionMinor) < 0) {
        HDF_LOGE("%{public}s snprintf_s failed", __func__);
        return nullptr;
    }
    if (realpath(path, resolvedPath) == nullptr || strncmp(resolvedPath, HDI_SO_PATH, strlen(HDI_SO_PATH)) != 0) {
        HDF_LOGE("%{public}s invalid hdi impl so name %{public}s", __func__, path);
        return nullptr;
    }

    HDF_LOGD("load interface impl lib: %{public}s", resolvedPath);
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
