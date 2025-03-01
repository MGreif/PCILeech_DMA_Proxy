#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"

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
		return (HANDLE)0x66;
	}

	BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
	{
		info = NULL;
		count_processes = 0;
		if (!VMMDLL_ProcessGetInformationAll(mem.vHandle, &info, &count_processes))
			return false;
		lppe->dwSize = sizeof(PROCESSENTRY32);
		lppe->th32ParentProcessID = info[current_process].dwPPID;
		lppe->th32ProcessID = info[current_process].dwPID;
		strcpy_s(lppe->szExeFile, info[current_process].szNameLong);
		current_process++;
		return true;
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
}
