#pragma once
#ifndef MG_HOOKS
#define MG_HOOKS

#include <Windows.h>
#include "libs/vmm.h"
#include <TlHelp32.h>
#include <string>

namespace ProcessHooks
{
	inline DWORD count_processes = 0;
	inline DWORD current_process = 0;
	inline PVMMDLL_PROCESS_INFORMATION info = NULL;

	//IsWow64Process
	typedef BOOL(WINAPI* IsWow64Process_t)(HANDLE, PBOOL);

	BOOL WINAPI hk_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);

	HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID);

	BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);

	BOOL hk_process_32_next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
}

struct CurrentProcessInformation
{
	int PID = 0;
	size_t base_address = 0;
	size_t base_size = 0;
	std::string process_name = "";
};




namespace Setup {
	inline BOOL DMA_INITIALIZED = false;
	inline VMM_HANDLE vHandle;
	inline CurrentProcessInformation current_process = { 0 };

	bool SetFPGA();
	BOOL Init();
}


BOOL __declspec(dllexport) initDMA();

#endif // !MG_HOOKS
