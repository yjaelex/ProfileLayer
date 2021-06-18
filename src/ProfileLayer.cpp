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

#include "ProfileLayer.h"
#include <sstream>
#include <algorithm>
#include <queue>

using namespace std;

#ifdef _DEBUG
#if _WIN32
#pragma comment(lib, "VkLayer_utils_dbg.lib")
#else
#pragma comment(lib, "libVkLayer_utils.a")
#endif
#else
#if _WIN32
#pragma comment(lib, "VkLayer_utils.lib")
#else
#pragma comment(lib, "libVkLayer_utils.a")
#endif
#endif // DEBUG


static uint32_t display_rate = 60;

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

thread_local int64 Profiler::m_timeAPI = 0;

void Profiler::DumpLog(const char *format, ...)
{
    va_list ap;
    char buffer[256];

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    //m_logMutex.lock();
    m_ssLog << buffer;
    if ((m_ssLog.gcount() >= 1 * 1024 * 1024) ||
        ((m_nFrame % display_rate) == 0))
    {
        m_logFile << m_ssLog.str();
        m_logFile.flush();
        m_ssLog.str(std::string());
    }
    //m_logMutex.unlock();
    printf("[VkLayer_PROFILE_LAYER] - %s", buffer);
}

// ms
int64_t Profiler::BeginCpuTime(void)
{
    return GetPerfCpuTime();
}

float Profiler::EndCpuTime(int64 beginTime, const char * pDumpStr)
{
    static const float MsPerSec = (float)(1000 * 1000);
    int64 endTime = GetPerfCpuTime();
    float time = (endTime - beginTime) / MsPerSec;

    if (pDumpStr)
    {
        DumpLog("%s : Time = %.6f ms\n", pDumpStr, time);
    }

    return time;
}

float Profiler::GetFramesPerSecond(void)
{
    // FPS is 1 divided by the average time for a single frame.
    return (m_cpuTimeSum > 0) ? (float)(m_cpuTimeSamples) / m_cpuTimeSum : 0.0f;
}

void Profiler::UpdateFps(void)
{
    // Set the last time query.
    m_performanceCounters[LastQuery] = m_performanceCounters[CurrentQuery];

    // Find the current time.
    m_performanceCounters[CurrentQuery] = GetPerfCpuTime();

    if (m_performanceCounters[LastQuery] != 0)
    {
        // Time since last frame is the difference between the queries divided by the frequency of the performance counter.
        float time = (m_performanceCounters[CurrentQuery] - m_performanceCounters[LastQuery]) / m_frequency;

        // Simple Moving Average: Subtract the oldest time on the list, add in the newest time, and update the list of
        // times.
        m_cpuTimeSum -= m_cpuTimeList[m_cpuTimeIndex];
        m_cpuTimeSum += time;
        m_cpuTimeList[m_cpuTimeIndex] = time;

        if (m_optionFlag & PL_OPTION_PRINT_FPS)
        {
            DumpLog("\nFrame Num = %d\n", m_nFrame);
            DumpLog("TotalFrame : Time = %.4f ms\n", time * 1000);
            DumpLog("Avg FPS: %.2f\n", GetFramesPerSecond());
        }

        // Calculating value for the time graph
        // scaledTime = time * NumberOfPixelsToScale / (1/60)fps
        //double scaledCpuTimePerFrame = time * NumberOfPixelsToScale * 60.0;
        //m_scaledCpuTimeList[m_cpuTimeIndex] = static_cast<uint32>(scaledCpuTimePerFrame);

        // Loop the list.
        if (++m_cpuTimeIndex == TimeCount)
        {
            m_cpuTimeIndex = 0;
        }

        // Increase m_cpuTimeSamples put don't go above TimeCount
        m_cpuTimeSamples = min(m_cpuTimeSamples + 1, TimeCount);
    }
    m_nFrame++;
}

void  Profiler::ProcessCmdFifo()
{
    int8 buffer[16];
    buffer[0] = 0;

    int32 ret = ReadFromFifo(buffer, 16);
    if (ret > 0)
    {
        switch (buffer[0])
        {
        case 'S':
            m_optionFlag |= PL_OPTION_PRINT_FPS;
            break;
        case 'E':
            m_optionFlag = m_optionFlag & (~PL_OPTION_PRINT_FPS);
            break;
        case 'A':
            m_optionFlag |= PL_OPTION_PRINT_API_NAME;
            break;
        case 'B':
            m_optionFlag = m_optionFlag & (~PL_OPTION_PRINT_API_NAME);
            break;
        case 'C':
            m_optionFlag |= PL_OPTION_PRINT_DEBUG_INFO;
            break;
        case 'D':
            m_optionFlag = m_optionFlag & (~PL_OPTION_PRINT_DEBUG_INFO);
            break;
        case 'F':
            m_optionFlag |= PL_OPTION_PRINT_PROFILE_INFO;
            break;
        case 'G':
            m_optionFlag = m_optionFlag & (~PL_OPTION_PRINT_PROFILE_INFO);
            break;
        case 'H':
            m_optionFlag |= PL_OPTION_PRINT_PROFILE_INFO_ALL;
            break;
        case 'I':
            m_optionFlag = m_optionFlag & (~PL_OPTION_PRINT_PROFILE_INFO_ALL);
            break;
        default:
            break;
        }
    }
}

static bool CompFunc(const std::pair<std::string, CallData>& i, const std::pair<std::string, CallData>& j)
{
    return (i.second.time > j.second.time);
}

void Profiler::UpdateProfileInfo()
{
    if (m_optionFlag & PL_OPTION_PRINT_PROFILE_INFO)
    {
        DumpLog("\nProfiling Data, Frame %d\n", m_nFrame);
        
        std::vector<std::pair<std::string, CallData>> vec;
        //std::priority_queue<CallData, std::vector<CallData>, std::function<bool(CallData&, CallData&)>> pq(CompFunc);

        float totalAPITime = 0.000001f;
        for (auto it=m_apiCallMap.begin(); it!=m_apiCallMap.end(); ++it)
        {
            totalAPITime += it->second.time;
            vec.push_back(*it);
            //pq.push(*first);
        }

        std::sort(vec.begin(), vec.end(), CompFunc);

        DumpLog("\n--------------------------------------------------------------\n");
        DumpLog("\nHot API Calls: Frame %d, Total APICall Time %.4f\n", m_nFrame, totalAPITime);
        DumpLog("Name,Time,Percentage,CallCount\n", m_nFrame);
        uint32 c = 0;
        for (auto it=vec.begin(); it!=vec.end(); ++it)
        {
            DumpLog("%s,%.4f,%.2f%%,%d\n", it->first.c_str(), it->second.time,
                    it->second.time*100/totalAPITime, it->second.callCount);
            c++;
            if (!(m_optionFlag & PL_OPTION_PRINT_PROFILE_INFO_ALL) && c>=10)
            {
                break;
            }
        }
        DumpLog("\n");
        m_apiCallMap.clear();
    }
}

// This function will be called for every API call
void Profiler::PreCallApiFunction(const char *api_name)
{
    if (m_optionFlag & PL_OPTION_PRINT_API_NAME)
    {
        DumpLog("Calling %s\n", api_name);
    }

    if (m_optionFlag & PL_OPTION_PRINT_PROFILE_INFO)
    {
        PreTime(api_name);
    }
}

void Profiler::PostCallApiFunction(const char *api_name, VkResult result)
{
    if (m_optionFlag & PL_OPTION_PRINT_API_NAME)
    {
        DumpLog("Called %s, result = %d\n", api_name, result);
    }

    if (m_optionFlag & PL_OPTION_PRINT_PROFILE_INFO)
    {
        float time = PostTime(api_name);

        std::string name = std::string(api_name);
        CallData data = { 0.0f, 0 };
        auto it = m_apiCallMap.find(name);
        if (it != m_apiCallMap.end())
        {
            data = it->second;
        }

        data.time += time;
        data.callCount++;

        m_apiCallMap[name] = data;
    }
}

// Intercept the memory allocation calls and increment the counter
VkResult Profiler::PostCallAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                         const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory, VkResult result) {
    number_mem_objects_++;
    total_memory_ += pAllocateInfo->allocationSize;
    mem_size_map_[*pMemory] = pAllocateInfo->allocationSize;
    return VK_SUCCESS;
}

// Intercept the free memory calls and update totals
void Profiler::PreCallFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator) {
    if (memory != VK_NULL_HANDLE) {
        number_mem_objects_--;
        VkDeviceSize this_alloc = mem_size_map_[memory];
        total_memory_ -= this_alloc;
    }
}

VkResult Profiler::PreCallQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo) {
    present_count_++;
    ProcessCmdFifo();
    if (present_count_ >= display_rate) {
        present_count_ = 0;

        std::stringstream message;
        message << "Memory Allocation Count: " << number_mem_objects_ << "\n";
        message << "Total Memory Allocation Size: " << total_memory_ << "\n\n";

        // Various text output options:
        // Call through simplified interface
        Profiler::Information(message.str());

#ifdef _WIN32
        // On Windows, call OutputDebugString to send output to the MSVC output window or debug out
        std::string str = message.str();
        LPCSTR cstr = str.c_str();
        OutputDebugStringA(cstr);
#endif

        // Option 3, use printf to stdout
        DumpLog("Demo layer: %s\n", message.str().c_str());
    }

    UpdateFps();

    UpdateProfileInfo();

    return VK_SUCCESS;
}

Profiler Profiler_Layer;

