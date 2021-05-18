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

#include "dev_attribute_parcel.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "osal_mem.h"

#define HDF_LOG_TAG dev_attribute_parcel

#define ATTRIBUTE_PRIVATE_DATA_LENGTH_NULL  0

static bool DeviceAttributePrivateWrite(const struct HdfDeviceInfoFull *attribute, struct HdfSBuf *sbuf)
{
    bool flag;
    uint8_t byteData;
    int32_t wordData;
    struct UsbPnpNotifyServiceInfo *info = (struct UsbPnpNotifyServiceInfo *)(attribute->super.private);
    int32_t i;

    if (info == NULL) {
        HDF_LOGE("Device attribute writePrivate private is NULL");
        return false;
    }

    wordData = info->length;
    flag = HdfSbufWriteInt32(sbuf, wordData);
    if (flag == false) {
        HDF_LOGE("Device attribute writePrivate length failed, flag=%{public}d", flag);
        return false;
    }
    wordData = info->devNum;
    flag = HdfSbufWriteInt32(sbuf, wordData);
    if (flag == false) {
        HDF_LOGE("Device attribute writePrivate devNum failed, flag=%{public}d", flag);
        return false;
    }
    wordData = info->busNum;
    flag = HdfSbufWriteInt32(sbuf, wordData);
    if (flag == false) {
        HDF_LOGE("Device attribute writePrivate busNum failed, flag=%{public}d", flag);
        return false;
    }
    wordData = info->interfaceLength;
    flag = HdfSbufWriteInt32(sbuf, wordData);
    if (flag == false) {
        HDF_LOGE("Device attribute writePrivate interfaceLength failed, flag=%{public}d", flag);
        return false;
    }
    for (i = 0; i < info->interfaceLength; i++) {
        byteData = info->interfaceNumber[i];
        flag = HdfSbufWriteUint8(sbuf, byteData);
        if (flag == false) {
            HDF_LOGE("Device attribute writePrivate interfaceNumber[%{public}d] failed, flag=%{public}d", flag, i);
            return false;
        }
    }

    return true;
}

static const void *DeviceAttributePrivateRead(struct HdfSBuf *sbuf)
{
    bool flag;
    int32_t wordData;
    uint8_t byteData;
    int32_t i;

    struct UsbPnpNotifyServiceInfo *info = (struct UsbPnpNotifyServiceInfo *)OsalMemCalloc(sizeof(struct UsbPnpNotifyServiceInfo));
    if (info == NULL) {
        HDF_LOGE("%{public}s: OsalMemCalloc failed, info is null", __func__);
        return NULL;
    }

    flag = HdfSbufReadInt32(sbuf, &wordData);
    if (flag == false) {
        HDF_LOGE("Device attribute readPrivate length failed, flag=%{public}d", flag);
        return NULL;
    }
    info->length = wordData;

    if (info->length == ATTRIBUTE_PRIVATE_DATA_LENGTH_NULL) {
        HDF_LOGW("%{public}s: readPrivate is NULL and info->length=%{public}d", __func__, info->length);
    } else {
        flag = HdfSbufReadInt32(sbuf, &wordData);
        if (flag == false) {
            HDF_LOGE("Device attribute readPrivate devNum failed, flag=%{public}d", flag);
            return NULL;
        }
        info->devNum = wordData;

        flag = HdfSbufReadInt32(sbuf, &wordData);
        if (flag == false) {
            HDF_LOGE("Device attribute readPrivate busNum failed, flag=%{public}d", flag);
            return NULL;
        }
        info->busNum = wordData;

        flag = HdfSbufReadInt32(sbuf, &wordData);
        if (flag == false) {
            HDF_LOGE("Device attribute readPrivate interfaceLength failed, flag=%{public}d", flag);
            return NULL;
        }
        info->interfaceLength = wordData;

        for (i = 0; i < info->interfaceLength; i++) {
            flag = HdfSbufReadUint8(sbuf, &byteData);
            if (flag == false) {
                HDF_LOGE("Device attribute readPrivate interfaceNumber[%{public}d] failed, flag=%{public}d", flag, i);
                return NULL;
            }
            info->interfaceNumber[i] = byteData;
        }
    }

    return (const void *)info;
}

bool DeviceAttributeFullWrite(const struct HdfDeviceInfoFull *attribute, struct HdfSBuf *sbuf)
{
    if (attribute == NULL || sbuf == NULL) {
        return false;
    }

    uint8_t ret = 1;
    ret &= HdfSbufWriteInt32(sbuf, attribute->super.deviceId);
    ret &= HdfSbufWriteInt32(sbuf, attribute->super.hostId);
    ret &= HdfSbufWriteInt32(sbuf, attribute->super.policy);
    ret &= HdfSbufWriteString(sbuf, attribute->super.svcName);
    ret &= HdfSbufWriteString(sbuf, attribute->super.moduleName);
    ret &= HdfSbufWriteString(sbuf, attribute->super.deviceMatchAttr);
    if (attribute->super.private != NULL) {
        ret &= DeviceAttributePrivateWrite(attribute, sbuf);
    } else {
        HDF_LOGW("%{public}s: Device attribute write private is NULL", __func__);
        uint8_t privateLength = ATTRIBUTE_PRIVATE_DATA_LENGTH_NULL;
        ret &= HdfSbufWriteUint8(sbuf, privateLength);
    }

    if (ret == 0) {
        HDF_LOGE("Device attribute write parcel failed");
        return false;
    }
    return true;
}

struct HdfDeviceInfoFull *DeviceAttributeFullRead(struct HdfSBuf *sbuf)
{
    if (sbuf == NULL) {
        return NULL;
    }

    struct HdfDeviceInfoFull *attribute = HdfDeviceInfoFullNewInstance();
    if (attribute == NULL) {
        HDF_LOGE("OsalMemCalloc failed, attribute is null");
        return NULL;
    }
    HdfSbufReadUint16(sbuf, &attribute->super.deviceId);
    HdfSbufReadUint16(sbuf, &attribute->super.hostId);
    HdfSbufReadUint16(sbuf, &attribute->super.policy);
    do {
        const char *svcName = HdfSbufReadString(sbuf);
        if (svcName == NULL) {
            HDF_LOGE("Read from parcel failed, svcName is null");
            break;
        }
        attribute->super.svcName = strdup(svcName);
        if (attribute->super.svcName == NULL) {
            HDF_LOGE("Read from parcel failed, strdup svcName fail");
            break;
        }
        const char *moduleName = HdfSbufReadString(sbuf);
        if (moduleName == NULL) {
            HDF_LOGE("Read from parcel failed, driverPath is null");
            break;
        }
        attribute->super.moduleName = strdup(moduleName);
        if (attribute->super.moduleName == NULL) {
            HDF_LOGE("Read from parcel failed, strdup moduleName fail");
            break;
        }

        const char *deviceMatchAttr = HdfSbufReadString(sbuf);
        if (deviceMatchAttr == NULL) {
            HDF_LOGE("%{public}s: Read from parcel failed, deviceMatchAttr is null", __func__);
            break;
        }
        attribute->super.deviceMatchAttr = strdup(deviceMatchAttr);

        attribute->super.private = DeviceAttributePrivateRead(sbuf);
        if (attribute->super.private == NULL) {
            HDF_LOGE("%{public}s: Read from parcel failed, private is null", __func__);
            break;
        }

        HDF_LOGD("%{public}s: AttributeRead moduleName=%{public}s, svcName=%{public}s, deviceMatchAttr=%{public}s, private=%{public}p, length=%{public}d", \
            __func__, moduleName, svcName, deviceMatchAttr, attribute->super.private, \
            ((struct UsbPnpNotifyServiceInfo *)(attribute->super.private))->length);

        return attribute;
    } while (0);

    HdfDeviceInfoFullFreeInstance(attribute);
    return NULL;
}
