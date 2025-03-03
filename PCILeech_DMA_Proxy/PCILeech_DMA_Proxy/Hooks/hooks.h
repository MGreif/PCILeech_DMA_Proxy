#pragma once
#ifndef MG_HOOKS
#define MG_HOOKS


#include <cstdint>
#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include "DMALibrary/Memory/Memory.h"

namespace Hooks
{
	enum KPROCESSOR_MODE
	{
		KernelMode,
		UserMode,
	};

	inline void* detour_function(void* pSource, void* pDestination)
	{
		printf("Detouring function at %p to %p\n", pSource, pDestination);
		BYTE jmp_bytes[14] = {
			0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp [RIP+0x00000000]
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // RIP value
		};
		*reinterpret_cast<uint64_t*>(jmp_bytes + 6) = reinterpret_cast<uint64_t>(pDestination);

		void* stub = VirtualAlloc(0, sizeof(jmp_bytes), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!stub)
		{
			printf("VirtualAlloc failed!\n");
			return nullptr;
		}

		memcpy(stub, pSource, sizeof(jmp_bytes));

		DWORD old_protect;
		if (VirtualProtect(pSource, sizeof(jmp_bytes), PAGE_EXECUTE_READWRITE, &old_protect))
		{
			memcpy(pSource, jmp_bytes, sizeof(jmp_bytes));
			VirtualProtect(pSource, sizeof(jmp_bytes), old_protect, &old_protect);
		}
		else
		{
			printf("VirtualProtect failed!\n");
			VirtualFree(stub, 0, MEM_RELEASE);
			return nullptr;
		}

		return stub;
	}

	inline BOOL isDMAProcessHandle(HANDLE handle) {
		return mem.initialized_processes.find(handle) != mem.initialized_processes.end();
	}

	//Mem.cpp
	extern SIZE_T hk_virtual_query_ex(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
	extern bool hk_write(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
	extern BOOL hk_read(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
	extern HANDLE hk_open_process(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
	extern BOOL hk_close_handle(HANDLE handle);
	extern BOOL hk_virtual_protect_ex(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);

	//Process.cpp
	extern HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID);
	extern BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
	extern BOOL hk_process_32_firstW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
	extern BOOL hk_process_32_next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
	extern BOOL hk_process_32_nextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
	extern BOOL WINAPI hk_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);

	//Modules.cpp
	extern BOOL hk_module_32_next(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
	extern BOOL hk_module_32_nextW(HANDLE hSnapshot, LPMODULEENTRY32W lpme);
	extern BOOL hk_module_32_first(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
	extern BOOL hk_module_32_firstW(HANDLE hSnapshot, LPMODULEENTRY32W lpme);

	//Threads.cpp
	extern BOOL hk_thread_32_next(HANDLE hSnapshot, LPTHREADENTRY32 lpte);
	extern BOOL hk_thread_32_first(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

	//Something still relies on OpenThread making this not working properly... gotto figure out what it is and implement it.
	extern HANDLE hk_open_thread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
	extern BOOL hk_get_thread_context(HANDLE hThread, PCONTEXT pContext);
	extern DWORD hk_resume_thread(HANDLE hThread);
	extern DWORD hk_suspend_thread(HANDLE hThread);
	extern BOOL hk_set_thread_context(HANDLE hThread, PCONTEXT pContext);


	// Original functions:

	// Define function pointer typedefs
	typedef HANDLE(WINAPI* tOpenProcess)(DWORD, BOOL, DWORD);
	typedef BOOL(WINAPI* tCloseHandle)(HANDLE);
	typedef BOOL(WINAPI* tReadProcessMemory)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
	typedef BOOL(WINAPI* tWriteProcessMemory)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
	typedef SIZE_T(WINAPI* tVirtualQueryEx)(HANDLE, LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
	typedef BOOL(WINAPI* tVirtualProtectEx)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
	typedef HANDLE(WINAPI* tCreateToolhelp32Snapshot)(DWORD, DWORD);
	typedef BOOL(WINAPI* tProcess32First)(HANDLE, LPPROCESSENTRY32);
	typedef BOOL(WINAPI* tProcess32Next)(HANDLE, LPPROCESSENTRY32);
	typedef BOOL(WINAPI* tModule32First)(HANDLE, LPMODULEENTRY32);
	typedef BOOL(WINAPI* tModule32Next)(HANDLE, LPMODULEENTRY32);
	typedef BOOL(WINAPI* tThread32First)(HANDLE, LPTHREADENTRY32);
	typedef BOOL(WINAPI* tThread32Next)(HANDLE, LPTHREADENTRY32);

	// Declare function pointers
	inline tOpenProcess open_process;
	inline tCloseHandle close_handle;
	inline tReadProcessMemory read_process_memory;
	inline tWriteProcessMemory write_process_memory;
	inline tVirtualQueryEx virtual_query;
	inline tVirtualProtectEx virtual_protect_ex;
	inline tCreateToolhelp32Snapshot create_tool_help32;
	inline tProcess32First process_32_first;
	inline tProcess32Next process_32_next;
	inline tModule32First module_32_first;
	inline tModule32Next module_32_next;
	inline tThread32First thread_32_first;
	inline tThread32Next thread_32_next;
}
#endif // !MG_HOOKS
