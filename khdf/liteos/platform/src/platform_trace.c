/*
 * Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "platform_trace.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "los_trace.h"

int32_t PlatformTraceStart(void)
{
#ifdef LOSCFG_KERNEL_TRACE
    uint32_t ret;
    ret = LOS_TraceStart();
    if (ret != LOS_OK) {
        HDF_LOGE("PlatformTraceStart error:%d", ret);
        return HDF_FAILURE;
    }
#endif
    return HDF_SUCCESS;
}

int32_t PlatformTraceStop(void)
{
#ifdef LOSCFG_KERNEL_TRACE
    LOS_TraceStop();
#endif
    return HDF_SUCCESS;
}

void PlatformTraceReset(void)
{
#ifdef LOSCFG_KERNEL_TRACE
    LOS_TraceReset();
#endif
}

#ifdef LOSCFG_KERNEL_TRACE
static void TraceMeanPrint(void)
{
    uint32_t i = 0;
    dprintf("********trace moudle and function explain:********\t");
    const struct PlatformTraceModuleExplain *explains = PlatformTraceModuleExplainGetAll();
    int32_t size = PlatformTraceModuleExplainCount();
    for (i = 0; i < size; i++) {
        if (explains[i].moduleInfo < PLATFORM_TRACE_MODULE_MAX) {
            dprintf("meaning of module num 0x%x is %s\t", explains[i].moduleInfo, explains[i].moduleMean);
        } else if (explains[i].moduleInfo > PLATFORM_TRACE_MODULE_MAX) {
            dprintf("meaning of function num 0x%x is %s\t", explains[i].moduleInfo, explains[i].moduleMean);
        }
    }
}
#endif

void PlatformTraceInfoDump(void)
{
#ifdef LOSCFG_KERNEL_TRACE
    TraceMeanPrint();
    LOS_TraceRecordDump(FALSE);
#endif
}

#define PLATFORM_TRACE_IDENTIFY 0x33
void PlatformTraceAddUintMsg(int module, int moduleFun, uint infos[], uint8_t size)
{
#ifdef LOSCFG_KERNEL_TRACE
    if ((size <= 0) || (size > PLATFORM_TRACE_UINT_PARAM_SIZE_MAX)) {
        HDF_LOGE("PlatformTraceAddUintMsg %hhu size illegal", size);
        return;
    }
    if ((module < PLATFORM_TRACE_MODULE_I2S) || (module >= PLATFORM_TRACE_MODULE_MAX)) {
        HDF_LOGE("PlatformTraceAddUintMsg %d module illegal", module);
        return;
    }
    if ((moduleFun < PLATFORM_TRACE_MODULE_I2S_FUN) || (moduleFun >= PLATFORM_TRACE_MODULE_MAX_FUN)) {
        HDF_LOGE("PlatformTraceAddUintMsg %d moduleFun illegal", moduleFun);
        return;
    }

    switch (size) {
        case PLATFORM_TRACE_UINT_PARAM_SIZE_1:
            LOS_TRACE_EASY(TRACE_SYS_FLAG, PLATFORM_TRACE_IDENTIFY, module, moduleFun,
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_1 - 1]);
            break;
        case PLATFORM_TRACE_UINT_PARAM_SIZE_2:
            LOS_TRACE_EASY(TRACE_SYS_FLAG, PLATFORM_TRACE_IDENTIFY, module, moduleFun,
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_1 - 1], infos[PLATFORM_TRACE_UINT_PARAM_SIZE_2 - 1]);
            break;
        case PLATFORM_TRACE_UINT_PARAM_SIZE_3:
            LOS_TRACE_EASY(TRACE_SYS_FLAG, PLATFORM_TRACE_IDENTIFY, module, moduleFun,
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_1 - 1], infos[PLATFORM_TRACE_UINT_PARAM_SIZE_2 - 1],
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_3 - 1]);
            break;
        case PLATFORM_TRACE_UINT_PARAM_SIZE_4:
            LOS_TRACE_EASY(TRACE_SYS_FLAG, PLATFORM_TRACE_IDENTIFY, module, moduleFun,
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_1 - 1], infos[PLATFORM_TRACE_UINT_PARAM_SIZE_2 - 1],
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_3 - 1], infos[PLATFORM_TRACE_UINT_PARAM_SIZE_4 - 1]);
            break;
        case PLATFORM_TRACE_UINT_PARAM_SIZE_MAX:
            LOS_TRACE_EASY(TRACE_SYS_FLAG, PLATFORM_TRACE_IDENTIFY, module, moduleFun,
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_1 - 1], infos[PLATFORM_TRACE_UINT_PARAM_SIZE_2 - 1],
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_3 - 1], infos[PLATFORM_TRACE_UINT_PARAM_SIZE_4 - 1],
                infos[PLATFORM_TRACE_UINT_PARAM_SIZE_MAX - 1]);
            break;
        default:
            HDF_LOGE("PlatformTraceAddUintMsg %hhu size illegal", size);
            break;
    }
#endif
}

void PlatformTraceAddMsg(const char *module, const char *moduleFun, const char *fmt, ...)
{
    // not support, return
    return;
}
