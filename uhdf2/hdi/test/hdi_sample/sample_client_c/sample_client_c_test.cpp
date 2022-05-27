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

#include <devmgr_hdi.h>
#include <gtest/gtest.h>
#include <hdf_log.h>
#include <hdf_remote_adapter_if.h>
#include <osal_mem.h>
#include <osal_time.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "ifoo.h"
#include "isample.h"
#include "isample_callback.h"

using namespace OHOS;
using namespace testing::ext;

#define HDF_LOG_TAG sample_client_c_test

constexpr const char *TEST_SERVICE_NAME = "sample_driver_service";

class SampleHdiCTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        auto devmgr = HDIDeviceManagerGet();
        if (devmgr != nullptr) {
            devmgr->LoadDevice(devmgr, TEST_SERVICE_NAME);
            HDIDeviceManagerRelease(devmgr);
        }
    }
    static void TearDownTestCase()
    {
        auto devmgr = HDIDeviceManagerGet();
        if (devmgr != nullptr) {
            devmgr->UnloadDevice(devmgr, TEST_SERVICE_NAME);
            HDIDeviceManagerRelease(devmgr);
        }
    }
    void SetUp() {}
    void TearDown() {}
};

// IPC mode get interface object
HWTEST_F(SampleHdiCTest, HdiCTest001, TestSize.Level1)
{
    struct ISample *sampleService = ISampleGetInstance(TEST_SERVICE_NAME, false);
    ASSERT_TRUE(sampleService != nullptr);

    struct IFoo *fooInterface = nullptr;
    int ret = sampleService->GetInterface(sampleService, &fooInterface);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(fooInterface, nullptr);

    ret = fooInterface->FooEvent(fooInterface, 1);
    ASSERT_EQ(ret, 0);

    bool value = false;
    ret = sampleService->PingTest(sampleService, true, &value);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(value, true);

    IFooRelease(true, fooInterface);
    ISampleRelease(false, sampleService);
}

// passthrough mode get interface object
HWTEST_F(SampleHdiCTest, HdiCTest002, TestSize.Level1)
{
    struct ISample *sampleService = ISampleGet(true);
    ASSERT_TRUE(sampleService != nullptr);

    struct IFoo *fooInterface = nullptr;
    int ret = sampleService->GetInterface(sampleService, &fooInterface);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(fooInterface, nullptr);

    ret = fooInterface->FooEvent(fooInterface, 1);
    ASSERT_EQ(ret, 0);

    bool value = false;
    ret = sampleService->PingTest(sampleService, true, &value);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(value, true);

    IFooRelease(true, fooInterface);
    ISampleRelease(true, sampleService);
}

struct SampleCallbackImpl {
    struct ISampleCallback interface;
    bool called;
};

static int32_t SampleCallbackPingTest(struct ISampleCallback *self, int event)
{
    if (self == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    struct SampleCallbackImpl *callback = CONTAINER_OF(self, struct SampleCallbackImpl, interface);
    callback->called = true;
    HDF_LOGI("%{public}s called, event = %{public}d", __func__, event);
    return HDF_SUCCESS;
}

// ipc mode set callback
HWTEST_F(SampleHdiCTest, HdiCTest003, TestSize.Level1)
{
    struct ISample *sampleService = ISampleGetInstance(TEST_SERVICE_NAME, false);
    ASSERT_TRUE(sampleService != nullptr);

    bool value = false;
    int ret = sampleService->PingTest(sampleService, true, &value);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(value, true);

    struct SampleCallbackImpl callback;
    callback.interface.PingTest = SampleCallbackPingTest;
    ret = sampleService->SetCallback(sampleService, &callback.interface);

    uint32_t retry = 0;
    constexpr int WAIT_COUNT = 200;
    while (!callback.called && retry < WAIT_COUNT) {
        retry++;
        OsalMSleep(1);
    }

    ASSERT_TRUE(callback.called);
    ISampleRelease(false, sampleService);
}

// passthrougt mode set callback
HWTEST_F(SampleHdiCTest, HdiCTest004, TestSize.Level1)
{
    struct ISample *sampleService = ISampleGet(true);
    ASSERT_TRUE(sampleService != nullptr);

    bool value = false;
    int ret = sampleService->PingTest(sampleService, true, &value);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(value, true);
    struct SampleCallbackImpl callback;
    callback.interface.PingTest = SampleCallbackPingTest;
    ret = sampleService->SetCallback(sampleService, &callback.interface);
    uint32_t retry = 0;

    constexpr int WAIT_COUNT = 20;
    while (!callback.called && retry < WAIT_COUNT) {
        retry++;
        OsalMSleep(1);
    }

    ASSERT_TRUE(callback.called);
    ISampleRelease(true, sampleService);
}

struct SampleDeathRecipient {
    struct HdfDeathRecipient recipient;
    bool called;
};

static void SampleOnRemoteDied(struct HdfDeathRecipient *deathRecipient, struct HdfRemoteService *remote)
{
    HDF_LOGI("sample service dead");
    if (deathRecipient == nullptr || remote == nullptr) {
        return;
    }
    struct SampleDeathRecipient *sampleRecipient = CONTAINER_OF(deathRecipient, struct SampleDeathRecipient, recipient);
    sampleRecipient->called = true;
}

// IPC mode add DeathRecipient
HWTEST_F(SampleHdiCTest, HdiCTest005, TestSize.Level1)
{
    struct ISample *sampleService = ISampleGetInstance(TEST_SERVICE_NAME, false);
    ASSERT_TRUE(sampleService != nullptr);

    struct SampleDeathRecipient deathRecipient = {
        .recipient = {
            .OnRemoteDied = SampleOnRemoteDied,
        },
        .called = false,
    };

    ASSERT_NE(sampleService->AsObject, nullptr);
    struct HdfRemoteService *remote = sampleService->AsObject(sampleService);
    ASSERT_NE(remote, nullptr);

    HdfRemoteAdapterAddDeathRecipient(remote, &deathRecipient.recipient);
    TearDownTestCase();

    int retry = 0;
    constexpr int WAIT_COUNT = 200;
    while (!deathRecipient.called && retry < WAIT_COUNT) {
        retry++;
        OsalMSleep(1);
    }
    ASSERT_TRUE(deathRecipient.called);

    HdfRemoteAdapterRemoveDeathRecipient(remote, &deathRecipient.recipient);
    ISampleRelease(true, sampleService);
}