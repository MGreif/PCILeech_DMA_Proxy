#pragma once
#ifndef MG_DLL_INJ_CreateRemoteThreadEx_LLA
#define MG_DLL_INJ_CreateRemoteThreadEx_LLA

#ifndef _WINDOWS_
#include <Windows.h>
#endif

bool CreateRemoteThreadEx_LLAInjection(HANDLE hProcess, char dll_path[]);

#endif