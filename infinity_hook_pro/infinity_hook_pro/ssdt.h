#pragma once
#include "headers.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

__int64 GetIndexByName(const char* sdName);
__int64 GetSSDTEntry(ULONG index);
PVOID GetFunctionAddress(const char* apiname);

#ifdef __cplusplus
}
#endif