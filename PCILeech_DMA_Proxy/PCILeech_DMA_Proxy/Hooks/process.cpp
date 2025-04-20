#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"
#include <string>
#include <iostream>
#include <ks.h>
#include "Communication/Communication.h"
#define LOG(fmt, ...) {printf("[Proxy] ");std::printf(fmt, ##__VA_ARGS__); fflush(stdout);};
#define LOGW(fmt, ...) {wprintf(L"[Proxy] ");std::wprintf(fmt, ##__VA_ARGS__); fflush(stdout);};

extern Memory mem;
extern HANDLE g_hPrivateCommunicationPipe;

typedef VOID (*t_RtlInitUnicodeString)(
	PUNICODE_STRING DestinationString,
	PCWSTR SourceString
);



UNICODE_STRING CharToUnicodeString(const char* ansiStr) {
	UNICODE_STRING unicodeStr;

	static t_RtlInitUnicodeString RtlInitUnicodeString;

	if (!RtlInitUnicodeString) {
		HMODULE ntdll = GetModuleHandle("ntdll.dll");
		t_RtlInitUnicodeString RtlInitUnicodeString = (t_RtlInitUnicodeString)GetProcAddress(ntdll, "RtlInitUnicodeString");
	}

	RtlInitUnicodeString(&unicodeStr, nullptr);

	// Determine required buffer size
	int wLen = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, NULL, 0);
	if (wLen == 0) {
		std::cerr << "Conversion failed!" << std::endl;
		return unicodeStr;
	}

	// Allocate buffer
	wchar_t* wideStr = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wLen * sizeof(wchar_t));
	if (!wideStr) {
		std::cerr << "Memory allocation failed!" << std::endl;
		return unicodeStr;
	}

	// Convert to wide string
	MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, wideStr, wLen);

	// Initialize UNICODE_STRING
	unicodeStr.Buffer = wideStr;
	unicodeStr.Length = (wLen - 1) * sizeof(WCHAR); // Exclude null-terminator
	unicodeStr.MaximumLength = wLen * sizeof(WCHAR); // Include null-terminator

	return unicodeStr;
}

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

	// Not finished yet
	NTSTATUS WINAPI hk_nt_query_system_information(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength)
	{
		if (SystemInformationClass != SystemProcessInformation) return 0;
		
		PVMMDLL_PROCESS_INFORMATION info = NULL;
		count_processes = 0;
		if (!VMMDLL_ProcessGetInformationAll(mem.vHandle, &info, &count_processes)) {
			LOG("[Process32FirstHook] Could not get VMDLL_ProcessGetInformationAll\n");
			LOG("Error: %u\n", GetLastError());
			return false;
		}

		SYSTEM_PROCESS_INFORMATION* processes = (SYSTEM_PROCESS_INFORMATION*)calloc(count_processes, sizeof(SYSTEM_PROCESS_INFORMATION));

		for (int i = 0; i < count_processes; i++) {
			processes[i].BasePriority = { KSPRIORITY_NORMAL };
			processes[i].HandleCount = 2;
			processes[i].UniqueProcessId = (HANDLE)info[i].dwPID;
			if (i < count_processes - 1)processes[i].NextEntryOffset = (ULONG)&processes[i + 1];
			processes[i].ImageName = CharToUnicodeString(info[i].szNameLong);
			processes[i].PeakVirtualSize = info[i].wSize;
			processes[i].NumberOfThreads = 2;
			processes[i].PagefileUsage = 10;
			processes[i].PeakPagefileUsage = 20;
			processes[i].VirtualSize = info[i].wSize;
			processes[i].PeakWorkingSetSize = 20;
			processes[i].WorkingSetSize = 10;
			processes[i].PrivatePageCount = 0;
			processes[i].QuotaNonPagedPoolUsage = 0;
			processes[i].QuotaPagedPoolUsage = 0;
		}

		fflush(stdout);

		if (SystemInformation != nullptr) {
			*(SYSTEM_PROCESS_INFORMATION**)SystemInformation = processes;
		}

		return 0;
	}

	// This hooks just takes care of ANSI to WIDE conversion and then calls the WIDE function
	BOOL hk_create_process_a(
		_In_opt_ LPCSTR lpApplicationName,
		_Inout_opt_ LPSTR lpCommandLine,
		_In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
		_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
		_In_ BOOL bInheritHandles,
		_In_ DWORD dwCreationFlags,
		_In_opt_ LPVOID lpEnvironment,
		_In_opt_ LPCWSTR lpCurrentDirectory,
		_In_ LPSTARTUPINFOA lpStartupInfo,
		_Out_ LPPROCESS_INFORMATION lpProcessInformation
	) {
		// ANSI to WIDE (UTF-16 LE) conversion
		WCHAR wApplicationName[MAX_PATH] = L"";
		if (lpApplicationName != nullptr) {
			MultiByteToWideChar(CP_ACP, 0, (LPCCH)lpApplicationName, -1, wApplicationName, MAX_PATH);
		}

		WCHAR wCommandLine[MAX_PATH] = L"";
		if (lpCommandLine != nullptr) {
			MultiByteToWideChar(CP_ACP, 0, (LPCCH)lpCommandLine, -1, wCommandLine, MAX_PATH);
		}

		STARTUPINFOW wsa = { 0 };
		memcpy(&wsa, lpStartupInfo, sizeof(wsa));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)lpStartupInfo->lpTitle, -1, wsa.lpTitle, MAX_PATH);

		BOOL result = hk_create_process_w(lpApplicationName == nullptr ? nullptr : wApplicationName, lpCommandLine ==  nullptr ? nullptr : wCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, &wsa, lpProcessInformation);
		return result;
	}

	BOOL hk_create_process_w(
		_In_opt_ LPCWSTR lpApplicationName,
		_Inout_opt_ LPWSTR lpCommandLine,
		_In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
		_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
		_In_ BOOL bInheritHandles,
		_In_ DWORD dwCreationFlags,
		_In_opt_ LPVOID lpEnvironment,
		_In_opt_ LPCWSTR lpCurrentDirectory,
		_In_ LPSTARTUPINFOW lpStartupInfo,
		_Out_ LPPROCESS_INFORMATION lpProcessInformation
	) {
		fflush(stdout);

		DWORD suspendedCreationFlags = dwCreationFlags | CREATE_SUSPENDED; // Add suspended flag so the ProxyLoader can inject the DLL as usual
		lpStartupInfo->hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		lpStartupInfo->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		lpStartupInfo->dwFlags |= STARTF_USESTDHANDLES;

		BOOL result = create_process_w(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, true, suspendedCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
		LOGW(L"NEW PROCESS STARTED SUSPENDED %s %s with PID: %u and main TID: %u\n", lpApplicationName, lpCommandLine, lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId);
		NewProcessCommand newProcessCommand = NewProcessCommand(lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId);
		if (!WriteFile(g_hPrivateCommunicationPipe, newProcessCommand.build().serialized, strlen(newProcessCommand.build().serialized), NULL, NULL)) {
			LOG("Could not write to private communication pipe\n");
		}
		LOG("Sent NEW_PROCESS command to ProxyLoader\n");
		return result;
	}

	HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID)
	{
		for (const auto [hProcess, config] : Memory::initialized_processes) {
			if (config.PID == th32ProcessID) {
				return hProcess;
			}
		}
		
		if (dwFlags & TH32CS_SNAPMODULE) {
			// snapmodule was called on a process that has not been opened yet
			return mem.InitProcess(th32ProcessID, true);
		}
		
		fflush(stdout);

		return (HANDLE)0x66; // If th32ProcessID is not given, e.g. SNAPPROCESS is set
	}

	BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
	{
		info = NULL;
		LOG("[Process32FirstHook] Called with snapshot: %u\n", hSnapshot);
		count_processes = 0;
		if (!VMMDLL_ProcessGetInformationAll(mem.vHandle, &info, &count_processes)) {
			LOG("[Process32FirstHook] Could not get VMDLL_ProcessGetInformationAll\n");
			LOG("Error: %u\n", GetLastError());
			return false;
		}
		lppe->dwSize = sizeof(PROCESSENTRY32);
		lppe->th32ParentProcessID = info[current_process].dwPPID;
		lppe->th32ProcessID = info[current_process].dwPID;
		strcpy_s(lppe->szExeFile, info[current_process].szNameLong);
		current_process++;
		fflush(stdout);

		return true;
	}

	BOOL hk_process_32_firstW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
	{
		PROCESSENTRY32 interm = { 0 };
		BOOL result = hk_process_32_first(hSnapshot, (LPPROCESSENTRY32)&interm);
		memcpy_s(lppe, sizeof(PROCESSENTRY32W), &interm, sizeof(PROCESSENTRY32));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szExeFile, -1, lppe->szExeFile, MAX_PATH);
		fflush(stdout);
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
		fflush(stdout);
		return true;
	}
	BOOL hk_process_32_nextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
	{
		PROCESSENTRY32 interm = { 0 };
		BOOL result = hk_process_32_next(hSnapshot, (LPPROCESSENTRY32)&interm);
		memcpy_s(lppe, sizeof(PROCESSENTRY32W), &interm, sizeof(PROCESSENTRY32));
		MultiByteToWideChar(CP_ACP, 0, (LPCCH)&interm.szExeFile, -1, lppe->szExeFile, MAX_PATH);
		fflush(stdout);
		return result;
	}
}
