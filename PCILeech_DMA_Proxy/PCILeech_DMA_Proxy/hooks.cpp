#include <Windows.h>
#include "libs/vmm.h"
#include <TlHelp32.h>
#include <stdio.h>
#include <iostream>
#include "hooks.h"

unsigned char abort2[4] = { 0x10, 0x00, 0x10, 0x00 };


namespace ProcessHooks
{
	//IsWow64Process
	typedef BOOL(WINAPI* IsWow64Process_t)(HANDLE, PBOOL);

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
		if (!VMMDLL_ProcessGetInformationAll(Setup::vHandle, &info, &count_processes))
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




namespace Setup {

	bool SetFPGA()
	{
		ULONG64 qwID = 0, qwVersionMajor = 0, qwVersionMinor = 0;
		if (!VMMDLL_ConfigGet(vHandle, LC_OPT_FPGA_FPGA_ID, &qwID) && VMMDLL_ConfigGet(vHandle, LC_OPT_FPGA_VERSION_MAJOR, &qwVersionMajor) && VMMDLL_ConfigGet(vHandle, LC_OPT_FPGA_VERSION_MINOR, &qwVersionMinor))
		{
			printf("[!] Failed to lookup FPGA device, Attempting to proceed\n\n");
			return false;
		}

		printf("[+] VMMDLL_ConfigGet");
		printf(" ID = %lli", qwID);
		printf(" VERSION = %lli.%lli\n", qwVersionMajor, qwVersionMinor);

		if ((qwVersionMajor >= 4) && ((qwVersionMajor >= 5) || (qwVersionMinor >= 7)))
		{
			HANDLE handle;
			LC_CONFIG config = { .dwVersion = LC_CONFIG_VERSION, .szDevice = "existing" };
			handle = LcCreate(&config);
			if (!handle)
			{
				printf("[!] Failed to create FPGA device\n");
				return false;
			}

			LcCommand(handle, LC_CMD_FPGA_CFGREGPCIE_MARKWR | 0x002, 4, reinterpret_cast<PBYTE>(&abort2), NULL, NULL);
			printf("[-] Register auto cleared\n");
			LcClose(handle);
		}

		return true;
	}

	BOOL Init()
	{
		if (!DMA_INITIALIZED)
		{
			printf("inizializing...\n");
		reinit:
			LPCSTR args[] = { const_cast<LPCSTR>(""), const_cast<LPCSTR>("-device"), const_cast<LPCSTR>("fpga://algo=0"), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>("") };
			DWORD argc = 3;
#ifdef  _DEBUG
			args[argc++] = const_cast<LPCSTR>("-v");
			args[argc++] = const_cast<LPCSTR>("-vv");
			args[argc++] = const_cast<LPCSTR>("-vvv");
			args[argc++] = const_cast<LPCSTR>("-printf");

#endif //  _DEBUG
		//	args[argc++] = const_cast<LPCSTR>("-waitinitialize");



			std::string path = "";
			

			vHandle = VMMDLL_Initialize(argc, args);
			if (!vHandle)
			{
				printf("[!] Initialization failed! Is the DMA in use or disconnected?\n");
				return false;
			}

			ULONG64 FPGA_ID = 0, DEVICE_ID = 0;

			VMMDLL_ConfigGet(vHandle, LC_OPT_FPGA_FPGA_ID, &FPGA_ID);
			VMMDLL_ConfigGet(vHandle, LC_OPT_FPGA_DEVICE_ID, &DEVICE_ID);

			printf("FPGA ID: %llu\n", FPGA_ID);
			printf("DEVICE ID: %llu\n", DEVICE_ID);
			printf("success!\n");

			if (!SetFPGA())
			{
				printf("[!] Could not set FPGA!\n");
				VMMDLL_Close(vHandle);
				return false;
			}

			DMA_INITIALIZED = TRUE;
		}
		else
			printf("DMA already initialized!\n");


		return true;
	}
}

BOOL __declspec(dllexport) initDMA() {
	return Setup::Init();
}