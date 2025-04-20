#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

#define PID_LEN 8

int main(int argc, char** argv)
{
	if (argc < 3) {
		printf("Usage: reader.exe <pid> <virtual-address> <iterations-for-testing>\n");
		return 1;
	}
	int pid = atoi(argv[1]);
	int iterations = atoi(argv[3]);

	printf("Starting SampleMemoryReader PID: %u TID: %u Iterations: %d\n", GetCurrentProcessId(), GetCurrentThreadId(), iterations);

	LPVOID vaddr = (LPVOID)strtoll(argv[2], &argv[2] + strlen(argv[2]), 16);

	printf("Reading memory of pid:%u at 0x%p\n", pid, vaddr);

	// This block is used to simulated child processes
	if (iterations > 0) {
		char newIterations[5] = { 0 };
		sprintf_s(newIterations, "%d", iterations - 1);

		char CurrentDir[1024] = { 0 };
		GetCurrentDirectoryA(sizeof(CurrentDir), CurrentDir);
		char command[1024] = { 0 };
		sprintf_s(command, "%s\\SampleMemoryReader.exe %s %s %s", CurrentDir, argv[1], argv[2], newIterations);
		printf("Starting new iteration: %s\n", command);
		STARTUPINFOA si = { 0 };
		PROCESS_INFORMATION pa = { 0 };
		si.cb = sizeof(si);
		if (!CreateProcessA(NULL, command, NULL, NULL, false, NULL, NULL, NULL, &si, &pa)) {
			printf("Could not create process. Error %u\n", GetLastError());
		}
		else {
			printf("Started new process %u\n", pa.dwProcessId);
		}
	}

	HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, false, pid);

	printf("Got process handle: 0x%p\n", hProcess);

	if (!hProcess) {
		printf("[!] Failed getting process. Error %u\n", GetLastError());
		exit(1);
	}


	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (!hSnapshot) {
		printf("[!] Could not get snapshot\n");
		exit(1);
	}
	PROCESSENTRY32W p = { 0 };
	p.dwSize = sizeof(PROCESSENTRY32);
	PROCESSENTRY32W *pFound = nullptr;
	if (!Process32First(hSnapshot, &p)) {
		printf("[!] Could not get Process32First\n");
		exit(1);
	}
	do {
		if (p.th32ProcessID == pid) {
			pFound = &p;
			break;
		}
	} while (Process32Next(hSnapshot, &p));
	if (pFound == nullptr) {
		printf("[!] Could not get processentry\n");
		exit(1);
	}
	
	wprintf(L"Found process: %s\n", pFound->szExeFile);

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (!hSnapshot) {
		printf("[!] Could not get snapshot\n");
		exit(1);
	}

	MODULEENTRY32W m = { 0 };
	m.dwSize = sizeof(MODULEENTRY32W);
	Module32First(hSnapshot, &m);
	do {
		wprintf(L"Found module: %s at %p with size %p\n", m.szModule, m.modBaseAddr, m.modBaseSize);
	} while (Module32Next(hSnapshot, &m));
	if (pFound == nullptr) {
		printf("[!] Could not get moduleentry\n");
		exit(1);
	}


	MEMORY_BASIC_INFORMATION mbi = { 0 };

	if (!VirtualQueryEx(hProcess, vaddr, &mbi, sizeof(mbi))) {
		printf("Could not query the vaddr\n");
		printf("Error: %u\n", GetLastError());
		exit(1);
	}

	if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) {
		printf("Page is not accessible %x\n", mbi.Protect);
		printf("Error: %u\n", GetLastError());
		exit(1);
	}

	DWORD oldProtect;
	if (!VirtualProtectEx(hProcess, vaddr, sizeof(DWORD), PAGE_READWRITE, &oldProtect)) {
		printf("Could not VirtualProtectEx\n");
		printf("Error: %u\n", GetLastError());
		exit(1);
	}

	DWORD out;
	if (!ReadProcessMemory(hProcess, vaddr, &out, sizeof(DWORD), NULL)) {

		printf("[!] Failed reading memory ...\n");
		exit(1);
	}

	printf("Read memory...\n\nPID: %u\nVADDR: 0x%p\nDWORD:0x%x\n", pid, vaddr, out);

	printf("Setting memory to 1337\n");

	DWORD newVal = 1337 + iterations;
	SIZE_T wrote = 0;
	if (!WriteProcessMemory(hProcess, vaddr, &newVal, sizeof(DWORD), &wrote)) {
		printf("[!] Could not write to %p\n", vaddr);
		printf("Error: %u\n", GetLastError());
		exit(1);
	}

	printf("Wrote 0x%x to %p\n", vaddr, newVal);

	if (!ReadProcessMemory(hProcess, vaddr, &out, sizeof(DWORD), NULL)) {

		printf("[!] Failed reading memory ...\n");
		exit(1);
	}

	printf("New value of %p is 0x%x\n", vaddr, out);
	Sleep(10000);
	fflush(stdout);
	CloseHandle(hProcess);


	return 0;
}
