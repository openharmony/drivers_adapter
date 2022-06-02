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
#include <gtest/gtest.h>
#include <hdf_log.h>
#include <idevmgr_hdi.h>
#include <iproxy_broker.h>
#include <iremote_object.h>
#include <map>
#include <osal_mem.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "sample_proxy.h"

using namespace testing::ext;
using namespace OHOS::HDI::Sample::V1_0;
using OHOS::IRemoteObject;
using OHOS::sptr;
using OHOS::wptr;
using OHOS::HDI::DeviceManager::V1_0::IDeviceManager;

#define HDF_LOG_TAG sample_client_cpp_test

constexpr const char *TEST_SERVICE_NAME = "sample_driver_service";

class SampleHdiCppTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        auto devmgr = IDeviceManager::Get();
        if (devmgr != nullptr) {
            devmgr->LoadDevice(TEST_SERVICE_NAME);
        }
    }
    static void TearDownTestCase()
    {
        auto devmgr = IDeviceManager::Get();
        if (devmgr != nullptr) {
            devmgr->UnloadDevice(TEST_SERVICE_NAME);
        }
    }
    void SetUp() {}
    void TearDown() {}
};

// IPC mode get interface object
HWTEST_F(SampleHdiCppTest, HdiCppTest001, TestSize.Level1)
{
    OHOS::sptr<ISample> sampleService = ISample::Get(TEST_SERVICE_NAME, false);
    ASSERT_TRUE(sampleService != nullptr);
    OHOS::sptr<IFoo> fooInterface = nullptr;
    int ret = sampleService->GetInterface(fooInterface);
    ASSERT_EQ(ret, 0);

    bool value = false;
    ret = fooInterface->PingTest(true, value);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(value, true);
}

// passthrough mode get interface object
HWTEST_F(SampleHdiCppTest, HdiCppTest002, TestSize.Level1)
{
    OHOS::sptr<ISample> sampleService = ISample::Get(true);
    ASSERT_TRUE(sampleService != nullptr);
    OHOS::sptr<IFoo> fooInterface = nullptr;
    int ret = sampleService->GetInterface(fooInterface);
    ASSERT_EQ(ret, 0);

    bool value = false;
    ret = fooInterface->PingTest(true, value);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(value, true);
}

class SampleDeathRecipient : public IRemoteObject::DeathRecipient {
public:
    void OnRemoteDied(const wptr<IRemoteObject> &object) override
    {
        HDF_LOGI("sample service dead");
    }
};

// IPC mode add DeathRecipient
HWTEST_F(SampleHdiCppTest, HdiCppTest003, TestSize.Level1)
{
    sptr<ISample> sampleService = ISample::Get(TEST_SERVICE_NAME, false);
    ASSERT_TRUE(sampleService != nullptr);

    const sptr<IRemoteObject::DeathRecipient> recipient = new SampleDeathRecipient();
    sptr<IRemoteObject> remote = OHOS::HDI::hdi_objcast<ISample>(sampleService);
    bool ret = remote->AddDeathRecipient(recipient);
    ASSERT_EQ(ret, true);
}
