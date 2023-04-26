#pragma once

#include "../../System.h"

#include <windows.h>



struct SYSTEM_API FPlatformMisc
{
    static void PlatformInit();
    FORCEINLINE static void MemoryBarrier() { _mm_sfence(); }
};


