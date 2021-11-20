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

#include "devmgr_service_proxy.h"
#include "devhost_service_stub.h"
#include "devmgr_service_stub.h"
#include "device_token_stub.h"
#include "devmgr_service.h"
#include "devsvc_manager_clnt.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "hdf_sbuf.h"
#include "osal_mem.h"

#define HDF_LOG_TAG devmgr_service_proxy

int DevmgrServiceProxyAttachDeviceHost(
    struct IDevmgrService *inst, uint16_t hostId, struct IDevHostService *service)
{
    int status = HDF_FAILURE;
    struct HdfSBuf *data = HdfSBufTypedObtain(SBUF_IPC);
    struct HdfSBuf *reply = HdfSBufTypedObtain(SBUF_IPC);
    struct HdfRemoteDispatcher *dipatcher = NULL;
    struct HdfRemoteService *remoteService = NULL;
    struct DevmgrServiceProxy *serviceProxy = (struct DevmgrServiceProxy *)inst;
    struct DevHostServiceStub *hostStub = (struct DevHostServiceStub *)service;
    if ((serviceProxy->remote == NULL) || (data == NULL) || (reply == NULL)) {
        HDF_LOGE("DevmgrServiceProxyAttachDeviceHost failed, host id is %{public}u", hostId);
        goto finished;
    }
    remoteService = serviceProxy->remote;
    dipatcher = remoteService->dispatcher;
    HdfSbufWriteInt32(data, hostId);
    HdfSBufWriteRemoteService(data, hostStub->remote);
    status = dipatcher->Dispatch(remoteService, DEVMGR_SERVICE_ATTACH_DEVICE_HOST, data, reply);
    HDF_LOGI("Attach device host dispatch finish, status is %{public}d", status);
finished:
    if (reply != NULL) {
        HdfSBufRecycle(reply);
    }
    if (data != NULL) {
        HdfSBufRecycle(data);
    }
    return status;
}

int DevmgrServiceProxyAttachDevice(
    struct IDevmgrService *inst, struct IHdfDeviceToken *token)
{
    int status = HDF_FAILURE;
    struct HdfSBuf *data = HdfSBufTypedObtain(SBUF_IPC);
    struct HdfSBuf *reply = HdfSBufTypedObtain(SBUF_IPC);
    struct DevmgrServiceProxy *serviceProxy = (struct DevmgrServiceProxy *)inst;
    if (serviceProxy == NULL || serviceProxy->remote == NULL || data == NULL ||
        reply == NULL || token == NULL) {
        HDF_LOGE("DevmgrServiceProxyAttachDevice failed");
        goto finished;
    }
    struct HdfRemoteService *remoteService = serviceProxy->remote;
    HdfSbufWriteInt32(data, token->devid);
    status = remoteService->dispatcher->Dispatch(remoteService, DEVMGR_SERVICE_ATTACH_DEVICE, data, reply);
finished:
    if (reply != NULL) {
        HdfSBufRecycle(reply);
    }
    if (data != NULL) {
        HdfSBufRecycle(data);
    }
    return status;
}

int DevmgrServiceProxyDetachDevice(struct IDevmgrService *inst, devid_t devid)
{
    int status = HDF_FAILURE;
    struct HdfSBuf *data = HdfSBufTypedObtain(SBUF_IPC);
    struct HdfSBuf *reply = HdfSBufTypedObtain(SBUF_IPC);
    struct DevmgrServiceProxy *serviceProxy = (struct DevmgrServiceProxy *)inst;
    if (serviceProxy == NULL || serviceProxy->remote == NULL || data == NULL || reply == NULL) {
        HDF_LOGE("DevmgrServiceProxyDetachDevice failed");
        goto finished;
    }
    struct HdfRemoteService *remoteService = serviceProxy->remote;
    HdfSbufWriteInt32(data, devid);
    status = remoteService->dispatcher->Dispatch(remoteService, DEVMGR_SERVICE_DETACH_DEVICE, data, reply);
finished:
    if (reply != NULL) {
        HdfSBufRecycle(reply);
    }
    if (data != NULL) {
        HdfSBufRecycle(data);
    }
    return status;
}
int  DevmgrServiceProxyLoadDevice(struct IDevmgrService *inst, const char *svcName)
{
    int status = HDF_FAILURE;
    struct DevmgrServiceProxy *serviceProxy = (struct DevmgrServiceProxy *)inst;
    if (serviceProxy == NULL || serviceProxy->remote == NULL || svcName == NULL) {
        HDF_LOGE("DevmgrServiceProxyLoadDevice failed");
        return HDF_ERR_INVALID_PARAM;
    }

    struct HdfSBuf *data = HdfSBufTypedObtain(SBUF_IPC);
    struct HdfRemoteService *remoteService = serviceProxy->remote;
    HdfSbufWriteString(data, svcName);

    status = remoteService->dispatcher->Dispatch(remoteService, DEVMGR_SERVICE_LOAD_DEVICE, data, NULL);

    HdfSBufRecycle(data);
    return status;
}

int  DevmgrServiceProxyUnLoadDevice(struct IDevmgrService *inst, const char *svcName)
{
    int status = HDF_FAILURE;
    struct DevmgrServiceProxy *serviceProxy = (struct DevmgrServiceProxy *)inst;
    if (serviceProxy == NULL || serviceProxy->remote == NULL || svcName == NULL) {
        HDF_LOGE("DevmgrServiceProxyLoadDevice failed");
        return HDF_ERR_INVALID_PARAM;
    }

    struct HdfSBuf *data = HdfSBufTypedObtain(SBUF_IPC);
    struct HdfRemoteService *remoteService = serviceProxy->remote;
    HdfSbufWriteString(data, svcName);

    status = remoteService->dispatcher->Dispatch(remoteService, DEVMGR_SERVICE_UNLOAD_DEVICE, data, NULL);

    HdfSBufRecycle(data);
    return status;
}

static void DevmgrServiceProxyConstruct(struct DevmgrServiceProxy *inst)
{
    struct IDevmgrService *pvtbl = (struct IDevmgrService *)inst;
    pvtbl->AttachDeviceHost = DevmgrServiceProxyAttachDeviceHost;
    pvtbl->AttachDevice = DevmgrServiceProxyAttachDevice;
    pvtbl->DetachDevice = DevmgrServiceProxyDetachDevice;
    pvtbl->LoadDevice = DevmgrServiceProxyLoadDevice;
    pvtbl->UnloadDevice = DevmgrServiceProxyUnLoadDevice;
    pvtbl->StartService = NULL;
}


static struct IDevmgrService *DevmgrServiceProxyObtain(struct HdfRemoteService *service)
{
    if (service != NULL) {
        struct DevmgrServiceProxy *demgrServicProxy =
            (struct DevmgrServiceProxy *)OsalMemCalloc(sizeof(struct DevmgrServiceProxy));
        if (demgrServicProxy != NULL) {
            demgrServicProxy->remote = service;
            DevmgrServiceProxyConstruct(demgrServicProxy);
            return &demgrServicProxy->super;
        }
    }
    HDF_LOGE("DevmgrServiceProxyObtain failed");
    return NULL;
}

struct HdfObject *DevmgrServiceProxyCreate(void)
{
    static struct IDevmgrService *instance = NULL;
    if (instance == NULL) {
        struct IDevSvcManager *serviceManagerIf = NULL;
        struct DevSvcManagerClnt *serviceManager = (struct DevSvcManagerClnt *)DevSvcManagerClntGetInstance();
        if ((serviceManager == NULL) || (serviceManager->devSvcMgrIf == NULL)) {
            HDF_LOGE("Fail to Create Service Manager Client");
            return NULL;
        }
        serviceManagerIf = serviceManager->devSvcMgrIf;
        if (serviceManagerIf->GetService == NULL) {
            HDF_LOGE("Get Service is not implement!!!");
            return NULL;
        } else {
            struct HdfRemoteService *remote = (struct HdfRemoteService *)
                serviceManagerIf->GetService(serviceManagerIf, DEVICE_MANAGER_SERVICE);
            if (remote != NULL) {
                instance = DevmgrServiceProxyObtain(remote);
            }
        }
    }
    return (struct HdfObject *)instance;
}

void DevmgrServiceProxyRelease(struct HdfObject *object)
{
    struct DevmgrServiceProxy *instance = (struct DevmgrServiceProxy *)object;
    if (instance != NULL) {
        if (instance->remote != NULL) {
            HdfRemoteServiceRecycle(instance->remote);
            instance->remote = NULL;
        }
        OsalMemFree(instance);
    }
}
