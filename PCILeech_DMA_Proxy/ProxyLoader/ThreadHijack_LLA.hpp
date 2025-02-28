#pragma once
#ifndef MG_THREADHIJACK
#define MG_THREADHIJACK

#ifndef _WINDOWS_
#include <Windows.h>
#include <TlHelp32.h>
#endif

bool ThreadHijack_LLAInjection(HANDLE hProcess, int pid, char dll_path[], HANDLE *hThread);


#endif // !MG_THREADHIJACK