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

#include "devmgr_service_stub.h"
#include "devhost_service_clnt.h"
#include "devhost_service_proxy.h"
#include "device_token_proxy.h"
#include "devmgr_query_device.h"
#include "devmgr_pnp_service.h"
#include "devmgr_virtual_service.h"
#include "devsvc_manager.h"
#include "hdf_device_info_full.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "hdf_sbuf.h"
#include "osal_mem.h"
#include "osal_sysevent.h"
#include "securec.h"
#include "hdf_attribute_manager.h"
#include "device_resource_if.h"
#include "hcs_tree_if.h"
#include <linux/usb/ch9.h>

#define HDF_LOG_TAG devmgr_service_stub

#define USB_PNP_NOTIFY_SERVICE_NAME "hdf_usb_pnp_notify_service"

struct UsbPnpMatchIdTable {
    const char *moduleName;
    const char *serviceName;

    int32_t interfaceLength;
    
    uint8_t length;
    
    uint16_t matchFlag;

    uint16_t vendorId;
    uint16_t productId;

    uint16_t bcdDeviceLow;
    uint16_t bcdDeviceHigh;

    uint8_t deviceClass;
    uint8_t deviceSubClass;
    uint8_t deviceProtocol;

    uint8_t interfaceClass;
    uint8_t interfaceSubClass;
    uint8_t interfaceProtocol;

    uint8_t interfaceNumber[USB_PNP_INFO_MAX_INTERFACES];
};

struct UsbPnpDeviceListTable {
    struct DListHead list;
    const char *moduleName;
    const char *serviceName;
    uint32_t usbDevAddr;
    int32_t devNum;
    int32_t busNum;
    int32_t interfaceLength;
    uint8_t interfaceNumber[USB_PNP_INFO_MAX_INTERFACES];
};

struct UsbPnpRemoveInfo {
    uint8_t removeType;
    uint32_t usbDevAddr;
    int32_t devNum;
    int32_t busNum;
    uint8_t interfaceNum;
};

struct UsbPnpDeviceListTable *g_usbPnpDeviceTableList = NULL;

static int32_t DevmgrServiceStubDispatchAttachDevice(
    struct IDevmgrService *devmgrSvc, struct HdfSBuf *data)
{
    uint32_t hostId;
    uint32_t deviceId;
    if (!HdfSbufReadUint32(data, &hostId) || !HdfSbufReadUint32(data, &deviceId)) {
        HDF_LOGE("%s:failed to get host id and device id", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct HdfDevTokenProxy *tokenClnt = HdfDevTokenProxyObtain(NULL);
    if (tokenClnt == NULL) {
        return HDF_FAILURE;
    }
    struct HdfDeviceInfo deviceInfo;
    deviceInfo.deviceId = deviceId;
    deviceInfo.hostId = hostId;
    return devmgrSvc->AttachDevice(devmgrSvc, &deviceInfo, &tokenClnt->super);
}

static int32_t DevmgrServiceStubDispatchPnpDevice(
    struct IDevmgrService *devmgrSvc, struct HdfSBuf *data, bool isReg)
{
    int32_t ret = HDF_FAILURE;
    uint32_t infoSize = 0;
    struct UsbPnpNotifyServiceInfo *privateData = NULL;
    
    const char *moduleName = HdfSbufReadString(data);
    if (moduleName == NULL) {
        return ret;
    }
    const char *serviceName = HdfSbufReadString(data);
    if (serviceName == NULL) {
        return ret;
    }

    if (!HdfSbufReadBuffer(data, (const void **)(&privateData), &infoSize)) {
        HDF_LOGW("%{public}s: HdfSbufReadBuffer privateData error!", __func__);
        privateData = NULL;
    }
    if (privateData == NULL) {
        HDF_LOGW("%{public}s: privateData is NULL!", __func__);
    } else {
        HDF_LOGD("%{public}s: length=%{public}d, infoSize=%{public}d, moduleName=%{public}s, serviceName=%{public}s", 
            __func__, privateData->length, infoSize, moduleName, serviceName);
    }
    
    if (isReg) {
        ret = DevmgrServiceRegPnpDevice(devmgrSvc, moduleName, serviceName, (const void *)privateData);
    } else {
        ret = DevmgrServiceUnRegPnpDevice(devmgrSvc, moduleName, serviceName);
    }
    return ret;
}

int32_t DevmgrServiceStubDispatch(
    struct HdfRemoteService* stub, int code, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    int32_t ret = HDF_FAILURE;
    struct DevmgrServiceStub *serviceStub = (struct DevmgrServiceStub *)stub;
    if (serviceStub == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    struct IDevmgrService *super = (struct IDevmgrService *)&serviceStub->super;
    HDF_LOGE("%s devmgr service stub dispatch cmd %d", __func__, code);
    switch (code) {
        case DEVMGR_SERVICE_ATTACH_DEVICE_HOST: {
            uint32_t hostId = 0;
            if (!HdfSbufReadUint32(data, &hostId)) {
                HDF_LOGE("invalid host id");
                return HDF_FAILURE;
            }
            struct HdfRemoteService *service = HdfSBufReadRemoteService(data);
            struct IDevHostService *hostIf = DevHostServiceProxyObtain(hostId, service);
            ret = super->AttachDeviceHost(super, hostId, hostIf);
            break;
        }
        case DEVMGR_SERVICE_ATTACH_DEVICE: {
            ret = DevmgrServiceStubDispatchAttachDevice(super, data);
            break;
        }
        case DEVMGR_SERVICE_REGIST_PNP_DEVICE: {
            ret = DevmgrServiceStubDispatchPnpDevice(super, data, true);
            break;
        }
        case DEVMGR_SERVICE_UNREGIST_PNP_DEVICE: {
            ret = DevmgrServiceStubDispatchPnpDevice(super, data, false);
            break;
        }
        case DEVMGR_SERVICE_QUERY_DEVICE: {
            ret = DevFillQueryDeviceInfo(super, data, reply);
            break;
        }
        case DEVMGR_SERVICE_REGISTER_VIRTUAL_DEVICE: {
            ret = DevmgrServiceVirtualDevice(super, data, reply, true);
            break;
        }
        case DEVMGR_SERVICE_UNREGISTER_VIRTUAL_DEVICE: {
            ret = DevmgrServiceVirtualDevice(super, data, reply, false);
            break;
        }
        default:
            break;
    }
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s devmgr service stub dispach failed, cmd id is %d", __func__, code);
        HdfSbufWriteInt32(reply, ret);
    }

    return ret;
}

static struct HdfRemoteDispatcher g_devmgrDispatcher = {
    .Dispatch = DevmgrServiceStubDispatch
};

static struct HdfSBuf *DevmgrServiceStubBufCreate(const char *moduleName, 
    const char *serviceName, struct UsbPnpNotifyServiceInfo serviceInfo)
{
    struct HdfSBuf *pnpData = NULL;
    char modName[128] = {0};

    pnpData = HdfSBufObtainDefaultSize();
    if (pnpData == NULL) {
        HDF_LOGE("%{public}s: HdfSBufTypedObtain pnpData fail", __func__);
        return NULL;
    }

    if (sprintf_s(modName, sizeof(modName) - 1, "lib%s.z.so", moduleName) < 0) {
        HDF_LOGE("%{public}s: sprintf_s modName fail", __func__);
        goto out;
    }
    if (!HdfSbufWriteString(pnpData, modName)) {
        HDF_LOGE("%{public}s: write modName name failed!", __func__);
        goto out;
    }
    
    if (!HdfSbufWriteString(pnpData, serviceName)) {
        HDF_LOGE("%{public}s: write service name failed!", __func__);
        goto out;
    }

    if (!HdfSbufWriteBuffer(pnpData, (const void *)(&serviceInfo), sizeof(struct UsbPnpNotifyServiceInfo))) {
        HDF_LOGE("%{public}s: write privateData failed!", __func__);
        goto out;
    }
    
    HDF_LOGD("%{public}s: create: devNum=%{public}d, busNum=%{public}d, moduleName=%{public}s, \
        serviceName=%{public}s", __func__, serviceInfo.devNum, serviceInfo.busNum, modName, serviceName);

    return pnpData;
    
out:
    HdfSBufRecycle(pnpData);

    return NULL;
}

static bool DevmgrServiceStubUsbMatchDevice(struct UsbPnpNotifyMatchInfoTable *dev, 
    const struct UsbPnpMatchIdTable *id)
{
    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_VENDOR) && 
        (id->vendorId != dev->deviceInfo.vendorId)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_PRODUCT) && 
        (id->productId != dev->deviceInfo.productId)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_DEV_LOW) && 
        (id->bcdDeviceLow > dev->deviceInfo.bcdDeviceLow)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_DEV_HIGH) && 
        (id->bcdDeviceHigh < dev->deviceInfo.bcdDeviceHigh)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_DEV_CLASS) && 
        (id->deviceClass != dev->deviceInfo.deviceClass)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_DEV_SUBCLASS) && 
        (id->deviceSubClass != dev->deviceInfo.deviceSubClass)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_DEV_PROTOCOL) && 
        (id->deviceProtocol != dev->deviceInfo.deviceProtocol)) {
        return false;
    }

    return true;
}

static bool DevmgrServiceStubUsbMatchOneIdIntf(struct UsbPnpNotifyMatchInfoTable *dev, 
    int8_t index, struct UsbPnpMatchIdTable *id)
{
    int32_t i;
    uint8_t j;
    
    if (dev->deviceInfo.deviceClass == USB_CLASS_VENDOR_SPEC && 
        !(id->matchFlag & USB_PNP_NOTIFY_MATCH_VENDOR) && 
        (id->matchFlag & (USB_PNP_NOTIFY_MATCH_INT_CLASS | USB_PNP_NOTIFY_MATCH_INT_SUBCLASS | 
        USB_PNP_NOTIFY_MATCH_INT_PROTOCOL | USB_PNP_NOTIFY_MATCH_INT_NUMBER))) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_INT_CLASS) && 
        (id->interfaceClass !=  dev->interfaceInfo[index].interfaceClass)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_INT_SUBCLASS) && 
        (id->interfaceSubClass != dev->interfaceInfo[index].interfaceSubClass)) {
        return false;
    }

    if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_INT_PROTOCOL) && 
        (id->interfaceProtocol != dev->interfaceInfo[index].interfaceProtocol)) {
        return false;
    }

    if (id->interfaceLength == 1) {
        if ((id->matchFlag & USB_PNP_NOTIFY_MATCH_INT_NUMBER) && 
            (id->interfaceNumber[0] != dev->interfaceInfo[index].interfaceNumber)) {
            return false;
        }

        id->interfaceNumber[0] = dev->interfaceInfo[index].interfaceNumber;
    } else {
        if (id->matchFlag & USB_PNP_NOTIFY_MATCH_INT_NUMBER) {
            for (i = 0; i < id->interfaceLength; i++) {
                for (j = 0; j < dev->numInfos; j++) {
                    if (id->interfaceNumber[i] == dev->interfaceInfo[j].interfaceNumber) {
                        break;
                    }
                }

                if (j >= dev->numInfos) {
                    return false;
                }
            }
        } else {
            id->interfaceLength = 1;
            id->interfaceNumber[0] = dev->interfaceInfo[index].interfaceNumber;
        }
    }

    return true;
}

static int32_t DevmgrServiceStubParseIdTable(const struct DeviceResourceNode *node, 
    struct DeviceResourceIface *devResIface, struct UsbPnpMatchIdTable *table)
{    
    if (node == NULL || table == NULL || devResIface == NULL) {
        HDF_LOGE("%{public}s: node or table or devResIface is NULL!", __func__);
        return HDF_FAILURE;
    }
    
    if (devResIface->GetString(node, "moduleName", &table->moduleName, "") != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read moduleName fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetString(node, "serviceName", &table->serviceName, "") != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read serviceName fail!", __func__);
        return HDF_FAILURE;
    }
    
    if (devResIface->GetUint8(node, "length", &table->length, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read length fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint16(node, "matchFlag", &table->matchFlag, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read matchFlag fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint16(node, "vendorId", &table->vendorId, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read vendorId fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint16(node, "productId", &table->productId, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read productId fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint16(node, "bcdDeviceLow", &table->bcdDeviceLow, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read bcdDeviceLow fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint16(node, "bcdDeviceHigh", &table->bcdDeviceHigh, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read bcdDeviceHigh fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint8(node, "deviceClass", &table->deviceClass, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read deviceClass fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint8(node, "deviceSubClass", &table->deviceSubClass, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read deviceSubClass fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint8(node, "deviceProtocol", &table->deviceProtocol, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read deviceProtocol fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint8(node, "interfaceClass", &table->interfaceClass, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read interfaceClass fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint8(node, "interfaceSubClass", &table->interfaceSubClass, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read interfaceSubClass fail!", __func__);
        return HDF_FAILURE;
    }

    if (devResIface->GetUint8(node, "interfaceProtocol", &table->interfaceProtocol, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read interfaceProtocol fail!", __func__);
        return HDF_FAILURE;
    }

    table->interfaceLength = devResIface->GetElemNum(node, "interfaceNumber");
    if (table->interfaceLength <= 0) {
        HDF_LOGE("%{public}s: read interfaceLength=%{public}d fail!", __func__, table->interfaceLength);
        return HDF_FAILURE;
    }
    if (devResIface->GetUint8Array(node, "interfaceNumber", table->interfaceNumber, \
        table->interfaceLength, 0) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: read interfaceNumber fail!", __func__);
        return HDF_FAILURE;
    }

    HDF_LOGD("%{public}s:%{public}s parseIdTable success, length=%{public}d", \
        __func__, USB_PNP_DEBUG_STRING, table->length);
    
    return HDF_SUCCESS;
}

static struct UsbPnpMatchIdTable **DevmgrServiceStubParseTableList(const struct DeviceResourceNode *node)
{
    struct DeviceResourceIface *devResIface = NULL;
    int32_t idTabCount;
    int32_t count;
    int32_t ret;
    const char *idTableName = NULL;
    const struct DeviceResourceNode *tableNode = NULL;
    struct UsbPnpMatchIdTable **idTable = NULL;

    if (node == NULL) {
        HDF_LOGE("%s: node is NULL!", __func__);
        return NULL;
    }
    devResIface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (devResIface == NULL) {
        HDF_LOGE("%{public}s: devResIface is NULL!", __func__);
        return NULL;
    }
    idTabCount = devResIface->GetElemNum(node, "idTableList");
    if (idTabCount <= 0) {
        HDF_LOGE("%{public}s: idTableList not found!", __func__);
        return NULL;
    }
    HDF_LOGD("%{public}s:%{public}s idTabCount=%{public}d!", \
        __func__, USB_PNP_DEBUG_STRING, idTabCount);
    idTable = (struct UsbPnpMatchIdTable **)OsalMemCalloc((idTabCount + 1) * sizeof(struct UsbPnpMatchIdTable *));
    if (idTable == NULL) {
        HDF_LOGE("%{public}s: OsalMemCalloc failure!", __func__);
        return NULL;
    }
    idTable[idTabCount] = NULL;
    for (count = 0; count < idTabCount; count++) {
        idTable[count] = (struct UsbPnpMatchIdTable *)OsalMemCalloc(sizeof(struct UsbPnpMatchIdTable));
        if (idTable[count] == NULL) {
            HDF_LOGE("%{public}s: OsalMemCalloc failure!", __func__);
            goto out;        
        }
        ret = devResIface->GetStringArrayElem(node, "idTableList", count, &idTableName, NULL);
        if (ret != HDF_SUCCESS) {
            goto out;
        }
        tableNode = devResIface->GetChildNode(node, idTableName);
        if (tableNode == NULL) {
            HDF_LOGE("%{public}s: tableNode is NULL!", __func__);
            goto out;
        }
        if (DevmgrServiceStubParseIdTable(tableNode, devResIface, idTable[count]) != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: DevmgrServiceStubParseIdTable failure!", __func__);
            goto out;
        }
    }

    HDF_LOGD("%{public}s: %{public}s parse usb pnp id table success, idTabCount=%{public}d!", \
        __func__, USB_PNP_DEBUG_STRING, idTabCount);

    return idTable;

out:
    while ((--count) >= 0) {
        OsalMemFree(idTable[count]);
    }
    OsalMemFree(idTable);
    
    return NULL;
}

static struct UsbPnpMatchIdTable **DevmgrServiceStubParseDeviceId(const struct DeviceResourceNode *node)
{
    const char *deviceIdName = NULL;
    struct DeviceResourceIface *devResIface = NULL;
    const struct DeviceResourceNode *deviceIdNode = NULL;
    
    if (node == NULL) {
        HDF_LOGE("%{public}s: node is NULL!", __func__);
        return NULL;
    }

    devResIface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (devResIface == NULL) {
        HDF_LOGE("%{public}s: devResIface is NULL!", __func__);
        return NULL;
    }

    if (devResIface->GetString(node, "usb_pnp_device_id", &deviceIdName, NULL) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: get usb_pnp_device_id name failure!", __func__);
        return NULL;
    }
    
    deviceIdNode = devResIface->GetChildNode(node, deviceIdName);
    if (deviceIdNode == NULL) {
        HDF_LOGE("%{public}s: deviceIdNode is NULL!", __func__);
        return NULL;
    }
    
    return DevmgrServiceStubParseTableList(deviceIdNode);
}

static struct UsbPnpMatchIdTable **DevmgrServiceStubPnpMatch(void)
{
    struct DeviceResourceIface *devResInstance = NULL;
    const struct DeviceResourceNode *rootNode = NULL;
    const struct DeviceResourceNode *usbPnpNode = NULL;

    devResInstance = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (devResInstance == NULL) {
        HDF_LOGE("%{public}s: devResInstance is NULL!", __func__);
        return NULL;
    }

    rootNode = devResInstance->GetRootNode();
    if (rootNode == NULL) {
        HDF_LOGE("%{public}s: devResNode is NULL!", __func__);
        return NULL;
    }

    usbPnpNode = devResInstance->GetNodeByMatchAttr(rootNode, "usb_pnp_match");
    if (usbPnpNode == NULL) {
        HDF_LOGE("%{public}s: usbPnpNode is NULL!", __func__);
        return NULL;
    }

    return DevmgrServiceStubParseDeviceId(usbPnpNode);
}

static const struct UsbPnpMatchIdTable *DevmgrServiceStubPnpAddInterface(uint8_t index, 
    struct UsbPnpNotifyMatchInfoTable *infoTable, struct UsbPnpMatchIdTable **matchIdTable)
{
    struct UsbPnpMatchIdTable *idTable = NULL;
    int32_t tableCount;
    
    for (tableCount = 0, idTable = matchIdTable[0]; idTable != NULL; idTable = matchIdTable[++tableCount]) {
        HDF_LOGD("%{public}s:%{public}s matchDevice doing, idTable=%{public}p-%{public}d-0x%{public}x-0x%{public}x", \
            __func__, USB_PNP_DEBUG_STRING, idTable, idTable->matchFlag, idTable->vendorId, idTable->productId);
        
        if (!DevmgrServiceStubUsbMatchDevice(infoTable, idTable)) {
            continue;
        }
        
        if (!DevmgrServiceStubUsbMatchOneIdIntf(infoTable, index, idTable)) {
            continue;
        }

        HDF_LOGD("%{public}s: matchDevice end, index=%{public}d tableCount=%{public}d is match idTable=%{public}p, \
            moduleName=%{public}s, serviceName=%{public}s", __func__, index, tableCount, idTable, \
            idTable->moduleName, idTable->serviceName);

        return idTable;
    }

    HDF_LOGD("%{public}s:%{public}s matchDevice end, idTable=%{public}p", \
        __func__, USB_PNP_DEBUG_STRING, idTable);
    
    return NULL;
}

static int DevmgrServiceStubPnpRemoveInterface(struct IDevmgrService *devmgrSvc, 
    struct UsbPnpRemoveInfo removeInfo)
{
    int ret = HDF_SUCCESS;
    struct HdfSBuf *pnpData = NULL;
    struct UsbPnpDeviceListTable *deviceListTablePos = NULL;
    struct UsbPnpDeviceListTable *deviceListTableTemp = NULL;
    bool findFlag = false;
    struct UsbPnpNotifyServiceInfo serviceInfo;
    int32_t i;
    
    HDF_LOGD("%{public}s:%{public}s removeType=%{public}d Enter", \
        __func__, USB_PNP_DEBUG_STRING, removeInfo.removeType);
    
    if (DListIsEmpty(&g_usbPnpDeviceTableList->list)) {
        HDF_LOGE("%{public}s:%{public}d g_usbPnpDeviceTableList is empty. ", __func__, __LINE__);
        return HDF_ERR_INVALID_OBJECT;
    }
    
    DLIST_FOR_EACH_ENTRY_SAFE(deviceListTablePos, deviceListTableTemp, &g_usbPnpDeviceTableList->list, \
        struct UsbPnpDeviceListTable, list) {
        if ((deviceListTablePos->usbDevAddr == removeInfo.usbDevAddr) && \
            (deviceListTablePos->busNum == removeInfo.busNum)) {
            if (removeInfo.removeType == USB_PNP_NOTIFY_REMOVE_INTERFACE_NUM) {
                for (i = 0; i < deviceListTablePos->interfaceLength; i++) {
                    if (deviceListTablePos->interfaceNumber[i] == removeInfo.interfaceNum) {
                        break;
                    }
                }

                if (i >= deviceListTablePos->interfaceLength) {
                    continue;
                }
            }
            findFlag = true;

            serviceInfo.length = sizeof(struct UsbPnpNotifyServiceInfo) - (USB_PNP_INFO_MAX_INTERFACES \
                - deviceListTablePos->interfaceLength);
            serviceInfo.devNum = deviceListTablePos->devNum;
            serviceInfo.busNum = deviceListTablePos->busNum;
            serviceInfo.interfaceLength = deviceListTablePos->interfaceLength;
            memcpy_s(serviceInfo.interfaceNumber, USB_PNP_INFO_MAX_INTERFACES, \
                deviceListTablePos->interfaceNumber, USB_PNP_INFO_MAX_INTERFACES);
            pnpData = DevmgrServiceStubBufCreate(deviceListTablePos->moduleName, deviceListTablePos->serviceName, \
                serviceInfo);
            if (pnpData == NULL) {
                ret = HDF_FAILURE;
                HDF_LOGE("%{public}s: DevmgrServiceStubBufCreate faile", __func__);
                break;
            }
            ret = DevmgrServiceStubDispatchPnpDevice(devmgrSvc, pnpData, false);
            if (ret != HDF_SUCCESS) {
                HDF_LOGE("%{public}s: DevmgrServiceStubDispatchPnpDevice faile", __func__);
                break;
            }

            DListRemove(&deviceListTablePos->list);
            OsalMemFree(deviceListTablePos);

            HDF_LOGD("%{public}s:%{public}s usbDevAddr=0x%{public}x, device=%{public}d-%{public}d-%{public}d \
                to be remove success. ", __func__, USB_PNP_DEBUG_STRING, removeInfo.usbDevAddr, \
                removeInfo.devNum, removeInfo.busNum, removeInfo.interfaceNum);
        }
    }

    if (findFlag == false) {
        HDF_LOGE("%{public}s:%{public}s removeType=%{public}d, usbDevAddr=0x%{public}x, \
            device=%{public}d-%{public}d-%{public}d to be remove but not exist. ", \
            __func__, USB_PNP_DEBUG_STRING, removeInfo.removeType, removeInfo.usbDevAddr, \
            removeInfo.devNum, removeInfo.busNum, removeInfo.interfaceNum);
        ret = HDF_FAILURE;
    }

    return ret;
}

static int DevmgrServiceStubDeviceListAdd(struct UsbPnpNotifyMatchInfoTable *info,
    const struct UsbPnpMatchIdTable *idTable)
{
    int ret;
    unsigned char *ptr = NULL;
    struct UsbPnpDeviceListTable *deviceTableListTemp = NULL;
    
    ptr = OsalMemAlloc(sizeof(struct UsbPnpDeviceListTable));
    if (ptr == NULL) {
        ret = HDF_ERR_MALLOC_FAIL;
        HDF_LOGE("%{public}s:%{public}d OsalMemAlloc faile, ret=%{public}d ", __func__, __LINE__, ret);
    } else {
        deviceTableListTemp = (struct UsbPnpDeviceListTable *)ptr;

        DListHeadInit(&deviceTableListTemp->list);
        deviceTableListTemp->moduleName = idTable->moduleName;
        deviceTableListTemp->serviceName = idTable->serviceName;
        deviceTableListTemp->usbDevAddr = info->usbDevAddr;
        deviceTableListTemp->devNum = info->devNum;
        deviceTableListTemp->busNum = info->busNum;
        deviceTableListTemp->interfaceLength = idTable->interfaceLength;
        memcpy_s(deviceTableListTemp->interfaceNumber, USB_PNP_INFO_MAX_INTERFACES, \
            idTable->interfaceNumber, USB_PNP_INFO_MAX_INTERFACES);

        DListInsertTail(&deviceTableListTemp->list, &g_usbPnpDeviceTableList->list);
        HDF_LOGD("%{public}s:%{public}s add %{public}s-%{public}s-0x%{public}x-%{public}d-%{public}d-\
            %{public}d-%{public}d device to g_usbPnpDeviceTableList", __func__, USB_PNP_DEBUG_STRING, \
            deviceTableListTemp->moduleName, deviceTableListTemp->serviceName, deviceTableListTemp->usbDevAddr, \
            deviceTableListTemp->devNum, deviceTableListTemp->busNum, deviceTableListTemp->interfaceLength, \
            deviceTableListTemp->interfaceNumber[0]);

        ret = HDF_SUCCESS;
    }

    return ret;
}

static int DevmgrServiceStubEventReceived(void *priv, uint32_t id, struct HdfSBuf *data)
{
    int ret;
    bool flag;
    uint32_t infoSize;
    struct UsbPnpNotifyMatchInfoTable *infoTable = NULL;
    struct HdfSBuf *pnpData = NULL;
    struct IDevmgrService *super = (struct IDevmgrService *)priv;
    struct UsbPnpMatchIdTable **matchIdTable = NULL;
    const struct UsbPnpMatchIdTable *idTable = NULL;
    int8_t i;
    int32_t tableCount;
    struct UsbPnpRemoveInfo removeInfo;
    struct UsbPnpNotifyServiceInfo serviceInfo;

    HDF_LOGD("%{public}s:%{public}s enter", __func__, USB_PNP_DEBUG_STRING);
    
    flag = HdfSbufReadBuffer(data, (const void **)(&infoTable), &infoSize);
    if ((flag == false) || (infoTable == NULL)) {
        ret = HDF_ERR_INVALID_PARAM;
        HDF_LOGE("%{public}s: fail to read infoTable in event data, flag=%{public}d, infoTable=%{public}p", \
            __func__, flag, infoTable);
        return ret;
    }
    HDF_LOGD("%{public}s: %{public}s infoSize=%{public}d, usbDevAddr=0x%{public}x, devNum=%{public}d, \
        busNum=%{public}d, infoTable=0x%{public}x-0x%{public}x success", __func__, USB_PNP_DEBUG_STRING, \
        infoSize, infoTable->usbDevAddr, infoTable->devNum, infoTable->busNum, infoTable->deviceInfo.vendorId, \
        infoTable->deviceInfo.productId);

    HDF_LOGD("%{public}s: priv=%{public}p received:id=%{public}u", __func__, priv, id);
    switch (id) {
        case USB_PNP_NOTIFY_ADD_INTERFACE:
            matchIdTable = DevmgrServiceStubPnpMatch();
            if ((matchIdTable == NULL) || (matchIdTable[0] == NULL)) {
                ret = HDF_ERR_INVALID_PARAM;
                HDF_LOGE("%{public}s: matchIdTable or matchIdTable[0] is NULL!", __func__);
                return ret;
            }
            
            for (i = 0; i < infoTable->numInfos; i++) {
                idTable = DevmgrServiceStubPnpAddInterface(i, infoTable, matchIdTable);
                if (idTable == NULL) {
                    HDF_LOGD("%{public}s: i=%{public}d is not match", __func__, i);
                    continue;
                }

                serviceInfo.length = sizeof(struct UsbPnpNotifyServiceInfo) - (USB_PNP_INFO_MAX_INTERFACES \
                    - idTable->interfaceLength);
                serviceInfo.devNum = infoTable->devNum;
                serviceInfo.busNum = infoTable->busNum;
                serviceInfo.interfaceLength = idTable->interfaceLength;
                memcpy_s(serviceInfo.interfaceNumber, USB_PNP_INFO_MAX_INTERFACES, \
                    idTable->interfaceNumber, USB_PNP_INFO_MAX_INTERFACES);
                pnpData = DevmgrServiceStubBufCreate(idTable->moduleName, idTable->serviceName, serviceInfo);
                if (pnpData == NULL) {
                    ret = HDF_FAILURE;
                    HDF_LOGE("%{public}s: DevmgrServiceStubBufCreate faile", __func__);
                    break;
                }

                ret = DevmgrServiceStubDispatchPnpDevice(super, pnpData, true);
                if (ret != HDF_SUCCESS) {
                    HDF_LOGE("%{public}s:%{public}s handle failed, id is %{public}d, ret=%{public}d", \
                        __func__, USB_PNP_DEBUG_STRING, id, ret);
                } else {
                    ret = DevmgrServiceStubDeviceListAdd(infoTable, idTable);
                    if (ret != HDF_SUCCESS) {
                        HDF_LOGE("%{public}s:%{public}s DevmgrServiceStubDeviceListAdd faile", \
                            __func__, USB_PNP_DEBUG_STRING);
                        break;
                    }
                }
            }
            
            HDF_LOGD("%{public}s:%{public}s OsalMemFree matchIdTable, ret=%{public}d", \
                __func__, USB_PNP_DEBUG_STRING, ret);

            tableCount = 0;
            for (idTable = matchIdTable[0]; idTable != NULL;) {
                tableCount++;
                idTable = matchIdTable[tableCount];
            }
            while ((--tableCount) >= 0) {
                OsalMemFree(matchIdTable[tableCount]);
            }
            OsalMemFree(matchIdTable);
            break;
        case USB_PNP_NOTIFY_REMOVE_INTERFACE:
            removeInfo.removeType = infoTable->removeType;
            removeInfo.usbDevAddr = infoTable->usbDevAddr;
            removeInfo.devNum = infoTable->devNum;
            removeInfo.busNum = infoTable->busNum;
            removeInfo.interfaceNum = infoTable->interfaceInfo[0].interfaceNumber;
            ret = DevmgrServiceStubPnpRemoveInterface(super, removeInfo);
            HDF_LOGD("%{public}s: %{public}s remove interface done", __func__, USB_PNP_DEBUG_STRING);
            break;
        case USB_PNP_NOTIFY_REPORT_INTERFACE:
            ret = HDF_ERR_NOT_SUPPORT;
            break;
        default:
            ret = HDF_ERR_INVALID_PARAM;
            break;
    }

    HDF_LOGD("%{public}s:%{public}s ret=%{public}d DONE", __func__, USB_PNP_DEBUG_STRING, ret);
    
    return ret;
}

static int DevmgrServiceStubEventSend(struct HdfIoService *serv, char *eventData)
{
    int ret;
    int replyData = 0;
    struct HdfSBuf *data = NULL;
    
    data = HdfSBufObtainDefaultSize();
    if (data == NULL) {
        ret = HDF_DEV_ERR_NO_MEMORY;
        HDF_LOGE("%{public}s: fail to obtain sbuf data", __func__);
        return ret;
    }

    struct HdfSBuf *reply = HdfSBufObtainDefaultSize();
    if (reply == NULL) {
        ret = HDF_DEV_ERR_NO_MEMORY;
        HDF_LOGE("%{public}s: fail to obtain sbuf reply", __func__);
        goto out;
    }

    if (!HdfSbufWriteString(data, eventData)) {
        ret = HDF_FAILURE;
        HDF_LOGE("%{public}s: fail to write sbuf", __func__);
        goto out;
    }

    ret = serv->dispatcher->Dispatch(&serv->object, USB_PNP_NOTIFY_REPORT_INTERFACE, data, reply);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: fail to send serivice call, ret=%{public}d", __func__, ret);
        goto out;
    }

    if (!HdfSbufReadInt32(reply, &replyData)) {
        ret = HDF_ERR_INVALID_OBJECT;
        HDF_LOGE("%{public}s: fail to get service call reply", __func__);
        goto out;
    }

    HDF_LOGE("%{public}s: get reply is 0x%{public}x", __func__, replyData);

out:
    HdfSBufRecycle(data);
    HdfSBufRecycle(reply);
    
    return ret;
}

static int DevmgrServiceStubEventHandle(struct IDevmgrService *inst)
{
    int status;
    struct HdfIoService *usbPnpServ = HdfIoServiceBind(USB_PNP_NOTIFY_SERVICE_NAME);
    static struct HdfDevEventlistener usbPnpListener = {
        .callBack = DevmgrServiceStubEventReceived,
    };
    usbPnpListener.priv = (void *)(inst);

    if (g_usbPnpDeviceTableList == NULL) {
        g_usbPnpDeviceTableList = (struct UsbPnpDeviceListTable *)OsalMemCalloc(sizeof(struct UsbPnpDeviceListTable));
        if (g_usbPnpDeviceTableList == NULL) {
            status = HDF_ERR_MALLOC_FAIL;
            HDF_LOGE("%{public}s: OsalMemCalloc g_usbPnpDeviceTableList faile status=%{public}d", \
                __func__, status);
            return status;
        }
        DListHeadInit(&g_usbPnpDeviceTableList->list);
        g_usbPnpDeviceTableList->moduleName = "";
        g_usbPnpDeviceTableList->serviceName = "";
        g_usbPnpDeviceTableList->usbDevAddr = 0;
        g_usbPnpDeviceTableList->devNum = 0;
        g_usbPnpDeviceTableList->busNum = 0;
        g_usbPnpDeviceTableList->interfaceLength = 0;
        memset_s(g_usbPnpDeviceTableList->interfaceNumber, USB_PNP_INFO_MAX_INTERFACES, 0, USB_PNP_INFO_MAX_INTERFACES);
        HDF_LOGI("%{public}s OsalMemCalloc success,g_usbPnpDeviceTableList=%{public}p", \
            USB_PNP_DEBUG_STRING, g_usbPnpDeviceTableList);
    } else {
        HDF_LOGD("%{public}s g_usbPnpDeviceTableList=%{public}p is not NULL", \
            USB_PNP_DEBUG_STRING, g_usbPnpDeviceTableList);
    }
    
    status = HdfDeviceRegisterEventListener(usbPnpServ, &usbPnpListener);
    if (status != HDF_SUCCESS) {
        HDF_LOGE("HdfDeviceRegisterEventListener faile status=%{public}d", status);
        goto out;
    }
    HDF_LOGI("%{public}s HdfDeviceRegisterEventListener success,inst=%{public}p", USB_PNP_DEBUG_STRING, inst);

    status = DevmgrServiceStubEventSend(usbPnpServ, "USB PNP Handle Info");
    if (status != HDF_SUCCESS) {
        HDF_LOGE("DevmgrServiceStubEventSend faile status=%{public}d", status);
        goto out;
    }
    HDF_LOGI("DevmgrServiceStubEventSend success");

out:
    OsalMemFree(g_usbPnpDeviceTableList);
    
    return status;
}

int DevmgrServiceStubStartService(struct IDevmgrService *inst)
{
    int status = HDF_FAILURE;
    struct HdfRemoteService *remoteService = NULL;
    struct DevmgrServiceStub *fullService = (struct DevmgrServiceStub *)inst;
    struct IDevSvcManager *serviceManager = DevSvcManagerGetInstance();
    if (fullService == NULL) {
        HDF_LOGI("Start service failed, fullService is null");
        return HDF_FAILURE;
    }
    remoteService = HdfRemoteServiceObtain((struct HdfObject *)inst, &g_devmgrDispatcher);
    if ((remoteService == NULL) || (serviceManager == NULL)) {
        HDF_LOGI("Start service failed, remoteService or serviceManager is null");
        return HDF_FAILURE;
    }
    struct HdfDeviceObject *deviceObject = OsalMemCalloc(sizeof(struct HdfDeviceObject));
    if (deviceObject == NULL) {
        return HDF_FAILURE;
    }
    deviceObject->service = (struct IDeviceIoService *)remoteService;
    if (serviceManager->AddService != NULL) {
        status = DevSvcManagerAddService(
            serviceManager, DEVICE_MANAGER_SERVICE, deviceObject);
    }
    if (status != HDF_SUCCESS) {
        OsalMemFree(deviceObject);
        return status;
    }
    fullService->remote = remoteService;

    status = DevmgrServiceStartService((struct IDevmgrService *)&fullService->super);

    HDF_LOGI("%{public}s:%{public}s Start service status is %{public}d", __func__, USB_PNP_DEBUG_STRING, status);

    HDF_LOGI("%{public}s:%{public}s", __func__, USB_PNP_DEBUG_STRING);
    if (status == HDF_SUCCESS) {
        if (DevmgrServiceStubEventHandle(inst) != HDF_SUCCESS) {
            HDF_LOGE("%{public}s:%{public}s error", __func__, USB_PNP_DEBUG_STRING);
        }
    }

    return status;
}

static void DevmgrServiceStubConstruct(struct DevmgrServiceStub *inst)
{
    struct IDevmgrService *pvtbl = (struct IDevmgrService *)inst;

    DevmgrServiceFullConstruct(&inst->super);
    pvtbl->StartService = DevmgrServiceStubStartService;
    inst->remote = NULL;
    OsalMutexInit(&inst->devmgrStubMutx);
}

struct HdfObject *DevmgrServiceStubCreate()
{
    static struct DevmgrServiceStub *instance = NULL;
    if (instance == NULL) {
        instance = (struct DevmgrServiceStub *)OsalMemCalloc(sizeof(struct DevmgrServiceStub));
        if (instance == NULL) {
            HDF_LOGE("Creating devmgr service stub failed, alloc mem error");
            return NULL;
        }
        DevmgrServiceStubConstruct(instance);
    }
    return (struct HdfObject *)instance;
}

void DevmgrServiceStubRelease(struct HdfObject *object)
{
    struct DevmgrServiceStub *instance = (struct DevmgrServiceStub *)object;
    if (instance != NULL) {
        if (instance->remote != NULL) {
            HdfRemoteServiceRecycle(instance->remote);
            instance->remote = NULL;
        }
        OsalMutexDestroy(&instance->devmgrStubMutx);
        OsalMemFree(instance);
    }
}

