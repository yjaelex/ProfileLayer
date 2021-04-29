#include <stdio.h>
#include <stdlib.h>
#include "Util.h"

#if _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <strsafe.h>

int64 GetPerfFrequency()
{
    LARGE_INTEGER frequencyQuery = { };
    QueryPerformanceFrequency(&frequencyQuery);
    return frequencyQuery.QuadPart;
}

int64 GetPerfCpuTime()
{
    LARGE_INTEGER cpuTimeQuery = { };
    QueryPerformanceCounter(&cpuTimeQuery);
    return cpuTimeQuery.QuadPart;
}

Result GetExecutableName(
    char*  pBuffer,
    char** ppFilename,
    size_t bufferLength)
{
    Result result = PL_Success;

    const DWORD count = GetModuleFileNameA(nullptr, pBuffer, static_cast<DWORD>(bufferLength));
    if (count > 0)
    {
        if (count == static_cast<DWORD>(bufferLength))
        {
            // Per MSDN documentation: GetModuleFileName() returns the length of the string copied to the output buffer,
            // not including the null terminator. However, if the buffer was insufficiently large, then the string is
            // truncated and the null terminator is appended, *and* the return value includes the null terminator.
            pBuffer[0] = '\0';

            result = PL_Error;
        }

        char*const pLastSlash = strrchr(pBuffer, '\\');
        (*ppFilename) = (pLastSlash == nullptr) ? pBuffer : (pLastSlash + 1);
    }

    return result;
}

uint32 GetIdOfCurrentProcess()
{
    return GetCurrentProcessId();
}

#else

#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cwchar>
#include <errno.h>
#include <linux/limits.h>

Result GetExecutableName(
    char*  pBuffer,
    char** ppFilename,
    size_t bufferLength)
{
    Result result = PL_Success;

    assert((pBuffer != nullptr) && (ppFilename != nullptr));

    const ssize_t count = readlink("/proc/self/exe", pBuffer, bufferLength - sizeof(char));
    if (count >= 0)
    {
        // readlink() doesn't append a null terminator, so we must do this ourselves!
        pBuffer[count] = '\0';
    }
    else
    {
        // The buffer was insufficiently large, just return an empty string.
        pBuffer[0] = '\0';
        result = PL_Error;
    }

    char*const pLastSlash = strrchr(pBuffer, '/');
    (*ppFilename) = (pLastSlash == nullptr) ? pBuffer : (pLastSlash + 1);

    return result;
}

int64 GetPerfFrequency()
{
    constexpr uint32 NanosecsPerSec = (1000 * 1000 * 1000);

    return NanosecsPerSec;
}

int64 GetPerfCpuTime()
{
    int64 time = 0LL;

    // clock_gettime() returns the monotonic time since EPOCH
    timespec ts = { };
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    {
        // the tick takes 1 ns since we actually can hardly get timer with res less than 1ns.
        constexpr int64 NanosecsPerSec = 1000000000LL;

        time = ((ts.tv_sec * NanosecsPerSec) + ts.tv_nsec);
    }

    return time;
}

uint32 GetIdOfCurrentProcess()
{
    return getpid();
}

#endif
