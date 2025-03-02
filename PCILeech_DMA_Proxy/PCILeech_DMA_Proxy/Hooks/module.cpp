#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"

extern Memory mem;


namespace Hooks
{
	PVMMDLL_MAP_MODULE module_info = NULL;
	DWORD current_module = 0;

	// hSnapshot is the HANDLE to the DMA process (initialized_processes)
	BOOL hk_module_32_first(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
	{
		module_info = NULL;
		current_module = 0;
		if (!VMMDLL_Map_GetModuleU(mem.vHandle, mem.initialized_processes[hSnapshot].PID, &module_info, VMMDLL_MODULE_FLAG_NORMAL))
			return false;

		lpme->dwSize = sizeof(MODULEENTRY32);
		lpme->th32ProcessID = mem.initialized_processes[hSnapshot].PID;
		lpme->hModule = (HMODULE)module_info->pMap[current_module].vaBase;
		lpme->modBaseSize = module_info->pMap[current_module].cbImageSize;
		lpme->modBaseAddr = (BYTE*)module_info->pMap[current_module].vaBase;
		strcpy_s(lpme->szModule, module_info->pMap[current_module].uszFullName);
		strcpy_s(lpme->szExePath, module_info->pMap[current_module].uszText);
		return true;
	}

	BOOL hk_module_32_firstW(HANDLE hSnapshot, LPMODULEENTRY32W lppe)
	{
		MODULEENTRY32 interm = { 0 };
		BOOL result = hk_module_32_first(hSnapshot, &interm);
		memcpy_s(lppe, sizeof(MODULEENTRY32W), &interm, sizeof(MODULEENTRY32));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szModule, -1, lppe->szModule, MAX_PATH);
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szExePath, -1, lppe->szExePath, MAX_PATH);
		return result;
	}

	BOOL hk_module_32_next(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
	{
		if (current_module >= module_info->cMap)
		{
			current_module = 0;
			return false;
		}

		lpme->dwSize = sizeof(MODULEENTRY32);
		lpme->th32ProcessID = mem.initialized_processes[hSnapshot].PID;
		lpme->hModule = (HMODULE)module_info->pMap[current_module].vaBase;
		lpme->modBaseSize = module_info->pMap[current_module].cbImageSize;
		lpme->modBaseAddr = (BYTE*)module_info->pMap[current_module].vaBase;
		strcpy_s(lpme->szModule, module_info->pMap[current_module].uszText);
		strcpy_s(lpme->szExePath, module_info->pMap[current_module].uszText);
		current_module++;
		return true;
	}

	BOOL hk_module_32_nextW(HANDLE hSnapshot, LPMODULEENTRY32W lppe)
	{
		MODULEENTRY32 interm = { 0 };
		BOOL result = hk_module_32_next(hSnapshot, &interm);
		memcpy_s(lppe, sizeof(MODULEENTRY32W), &interm, sizeof(MODULEENTRY32));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szModule, -1, lppe->szModule, MAX_PATH);
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szExePath, -1, lppe->szExePath, MAX_PATH);
		return result;
	}
}
