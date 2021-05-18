/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef DEVMGR_SERVICE_STUB_H
#define DEVMGR_SERVICE_STUB_H

#include "hdf_remote_service.h"
#include "devmgr_service_full.h"
#include "osal_mutex.h"

#define DEVICE_MANAGER_SERVICE "hdf_device_manager"
#define DEVICE_MANAGER_SERVICE_SA_ID 5002


struct DevmgrServiceStub {
    struct DevmgrServiceFull super;
    struct HdfRemoteService *remote;
    struct OsalMutex devmgrStubMutx;
};

enum {
    DEVMGR_SERVICE_ATTACH_DEVICE_HOST = 1,
    DEVMGR_SERVICE_ATTACH_DEVICE,
    DEVMGR_SERVICE_REGIST_PNP_DEVICE,
    DEVMGR_SERVICE_UNREGIST_PNP_DEVICE,
    DEVMGR_SERVICE_QUERY_DEVICE,
    DEVMGR_SERVICE_REGISTER_VIRTUAL_DEVICE,
    DEVMGR_SERVICE_UNREGISTER_VIRTUAL_DEVICE,
};

struct HdfObject *DevmgrServiceStubCreate(void);
void DevmgrServiceStubRelease(struct HdfObject *object);

#endif /* DEVMGR_SERVICE_STUB_H */
