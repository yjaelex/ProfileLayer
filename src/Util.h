#pragma once

#include "stdint.h"

/*
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
*/

typedef int8_t   int8;    ///< 8-bit integer.
typedef int16_t  int16;   ///< 16-bit integer.
typedef int32_t  int32;   ///< 32-bit integer.
typedef int64_t  int64;   ///< 64-bit integer.
typedef uint8_t  uint8;   ///< Unsigned 8-bit integer.
typedef uint16_t uint16;  ///< Unsigned 16-bit integer.
typedef uint32_t uint32;  ///< Unsigned 32-bit integer.
typedef uint64_t uint64;  ///< Unsigned 64-bit integer.

typedef enum Result
{
    PL_Success = 0,
    PL_TimeOut = 1,
    PL_Error = 2
}Result;

int64 GetPerfFrequency();
int64 GetPerfCpuTime();
Result GetExecutableName(char*  pBuffer, char** ppFilename, size_t bufferLength);
uint32 GetIdOfCurrentProcess();
