/*
 * Copyright (c) 2015-2021 Valve Corporation
 * Copyright (c) 2015-2021 LunarG, Inc.
 *
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
 *
 * Author: Mark Lobodzinski <mark@lunarg.com>
 */

#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <unordered_map>
#include <map>
#include <fstream>
#include <mutex>
#include "vulkan/vulkan.h"
#include "vk_layer_logging.h"
#include "layer_factory.h"
#include "Util.h"

#define TimeCount 40

#define PL_OPTION_PRINT_API_NAME    0x1
#define PL_OPTION_PRINT_FPS         0x2
#define PL_OPTION_PRINT_DEBUG_INFO  0x4
#define PL_OPTION_PRINT_PROFILE_INFO  0x8
#define PL_OPTION_PRINT_PROFILE_INFO_ALL 0x16

#define FIFO_NAME   "/tmp/VKProfileLayerCmd.fifo"

typedef struct CallData
{
    float   time;
    uint32  callCount;
}CallData;

class Profiler : public layer_factory {
   public:
    // Constructor for state_tracker
    Profiler() : number_mem_objects_(0), total_memory_(0), present_count_(0)
    {
        m_performanceCounters[NumQuery] = { 0 };
        m_cpuTimeSamples = 0;                        // Number of valid entried in m_cpuTimeList
        m_cpuTimeIndex = 0;                          // Current index into list of times
        m_cpuTimeSum = 0.0;
        m_nFrame = 0;
        m_optionFlag = 0;
        m_optionFlag |= PL_OPTION_PRINT_FPS;

        memset(&m_cpuTimeList[0], 0, sizeof(m_cpuTimeList));
        m_frequency = (float)(GetPerfFrequency());

#ifdef _WIN32
        m_logFile.open("DumpLogFile.txt", std::fstream::out | std::fstream::trunc);
#else
        m_logFile.open("/tmp/DumpLogFile.txt", std::fstream::out | std::fstream::trunc);
#endif

        if (!m_logFile.is_open())
        {
            Warning(std::string("Fail to open Dump file!"));
        }

        InitCmdFifo();
    }

    ~Profiler()
    {
        if (m_fifoFd != -1)
        {
            close(m_fifoFd);
        }
        m_logFile.close();
    }

    void PreCallApiFunction(const char *api_name);
    void PostCallApiFunction(const char *api_name, VkResult result);

    VkResult PostCallAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                    const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory, VkResult result);

    void PreCallFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator);

    VkResult PreCallQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo);

   private:
    void  DumpLog(const char *format, ...);
    int64 BeginCpuTime(void);
    float EndCpuTime(int64 beginTime, const char * pDumpStr);
    float GetFramesPerSecond(void);
    void  UpdateFps(void);
    void  UpdateProfileInfo(void);
    void  ProcessCmdFifo();
    void  OutDebugInfo(const char* str)
    {
        if (m_optionFlag & PL_OPTION_PRINT_DEBUG_INFO)
        {
            DumpLog("[DEBUG_INFO] - %s\n", str);
        }
    }

    void  OutProfilerInfo(const char* str)
    {
        if (m_optionFlag & PL_OPTION_PRINT_PROFILE_INFO)
        {
            DumpLog("[PROFILE_INFO] - %s\n", str);
        }
    }

    int32 InitCmdFifo()
    {
        m_fifoFd = -1;
        mkfifo(FIFO_NAME, 0666);

        m_fifoFd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK);
        if (m_fifoFd == -1)
        {
            DumpLog("\n[ERROR] - open %s error!\n", FIFO_NAME);
        }
        else
        {
            DumpLog("\n[INFO] - open %s successfully!\n", FIFO_NAME);
        }
        return m_fifoFd;
    }

    int32 ReadFromFifo(int8* pBuf, size_t size)
    {
        int32 ret = -1;
        if (m_fifoFd != -1)
        {
            ret = read(m_fifoFd, pBuf, size);
        }
        return ret;
    }

    void  PreTime(const char * name)
    {
        m_timeAPI = BeginCpuTime();
    }

    float  PostTime(const char * name)
    {
        return EndCpuTime(m_timeAPI, NULL);
    }

    static thread_local int64 m_timeAPI;

    std::map<std::string, CallData> m_apiCallMap;
    int32       m_fifoFd;
    uint32_t number_mem_objects_;
    VkDeviceSize total_memory_;
    uint32_t present_count_;
    std::unordered_map<VkDeviceMemory, VkDeviceSize> mem_size_map_;

    enum QueryTime
    {
        LastQuery = 0,       // Last performance query index
        CurrentQuery,        // Current performance query index
        NumQuery             // Total number of query indices
    };

    uint32_t            m_nFrame;
    int64_t             m_performanceCounters[NumQuery];
    float               m_frequency;                                 // Frequency of performance counters
    float               m_cpuTimeList[TimeCount];                    // List of times between frames
    uint32_t            m_cpuTimeSamples;                            // Number of valid entried in m_cpuTimeList
    uint32_t            m_cpuTimeIndex;                              // Current index into list of times
    float               m_cpuTimeSum;                                // Current sum of all times
    uint64              m_optionFlag;

    std::stringstream   m_ssLog;
    std::fstream        m_logFile;
    std::mutex          m_logMutex;
};
