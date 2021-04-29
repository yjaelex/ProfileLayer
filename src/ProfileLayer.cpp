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


void Profiler::DumpLog(const char *format, ...)
{
    va_list ap;
    char buffer[256];

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    //m_logMutex.lock();
    // m_ssLog << buffer;
    // if ((m_ssLog.gcount() >= 1 * 1024 * 1024) ||
    //     ((m_nFrame % display_rate) == 0))
    // {
    //     m_logFile << m_ssLog.str();
    //     m_logFile.flush();
    //     m_ssLog.str(std::string());
    // }
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

        DumpLog("\nFrame Num = %d\n", m_nFrame);
        DumpLog("TotalFrame : Time = %.4f ms\n", time * 1000);
        DumpLog("Avg FPS: %.2f\n", GetFramesPerSecond());

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

// This function will be called for every API call
void Profiler::PreCallApiFunction(const char *api_name)
{
    if (m_optionFlag & PL_OPTION_PRINT_API_NAME)
    {
        DumpLog("Calling %s\n", api_name);
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

    return VK_SUCCESS;
}

Profiler Profiler_Layer;

