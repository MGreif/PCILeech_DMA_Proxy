#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"
#include <string>

namespace Hooks
{
	DWORD count_processes = 0;
	DWORD current_process = 0;
	PVMMDLL_PROCESS_INFORMATION info = NULL;

	//IsWow64Process
	typedef BOOL (WINAPI*IsWow64Process_t)(HANDLE, PBOOL);

	BOOL WINAPI hk_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process)
	{
		return true;
	}

	HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID)
	{
		for (const auto [hProcess, config] : Memory::initialized_processes) {
			if (config.PID == th32ProcessID) {
				return hProcess;
			}
		}
		return (HANDLE)0x66; // If th32ProcessID is not given, e.g. SNAPPROCESS is set
	}

	BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
	{
		info = NULL;
		printf("[Process32FirstHook] Called with snapshot: %u\n", hSnapshot);
		count_processes = 0;
		if (!VMMDLL_ProcessGetInformationAll(mem.vHandle, &info, &count_processes)) {
			printf("[Process32FirstHook] Could not get VMDLL_ProcessGetInformationAll\n");
			printf("Error: %u\n", GetLastError());
			return false;
		}
		lppe->dwSize = sizeof(PROCESSENTRY32);
		lppe->th32ParentProcessID = info[current_process].dwPPID;
		lppe->th32ProcessID = info[current_process].dwPID;
		strcpy_s(lppe->szExeFile, info[current_process].szNameLong);
		current_process++;
		return true;
	}

	BOOL hk_process_32_firstW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
	{
		PROCESSENTRY32 interm = { 0 };
		BOOL result = hk_process_32_first(hSnapshot, (LPPROCESSENTRY32)&interm);
		memcpy_s(lppe, sizeof(PROCESSENTRY32W), &interm, sizeof(PROCESSENTRY32));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szExeFile, -1, lppe->szExeFile, MAX_PATH);
		return result;
	}

	BOOL hk_process_32_next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
	{
		if (current_process >= count_processes)
		{
			current_process = 0;
			return false;
		}

		lppe->dwSize = sizeof(PROCESSENTRY32);
		lppe->th32ParentProcessID = info[current_process].dwPPID;
		lppe->th32ProcessID = info[current_process].dwPID;
		strcpy_s(lppe->szExeFile, info[current_process].szNameLong);
		current_process++;
		return true;
	}
	BOOL hk_process_32_nextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
	{
		PROCESSENTRY32 interm = { 0 };
		BOOL result = hk_process_32_next(hSnapshot, (LPPROCESSENTRY32)&interm);
		memcpy_s(lppe, sizeof(PROCESSENTRY32W), &interm, sizeof(PROCESSENTRY32));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szExeFile, -1, lppe->szExeFile, MAX_PATH);
		return result;
	}
}
