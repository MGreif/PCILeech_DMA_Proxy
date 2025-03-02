#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

#define PID_LEN 8

int main(int argc, char** argv)
{
	if (argc < 3) {
		printf("Usage: reader.exe <pid> <virtual-address>\n");
		return 1;
	}

	int pid = atoi(argv[1]);
	LPVOID vaddr = (LPVOID)strtoll(argv[2], &argv[2] + strlen(argv[2]), 16);

	printf("Reading memory of pid:%u at 0x%p\n", pid, vaddr);

	HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, 0, pid);

	printf("Got process handle: 0x%p\n", hProcess);

	if (!hProcess) {
		printf("[!] Failed getting process\n");
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

	DWORD out;
	if (!ReadProcessMemory(hProcess, vaddr, &out, sizeof(DWORD), NULL)) {

		printf("[!] Failed reading memory ...\n");
		exit(1);
	}

	printf("Read memory...\n\nPID: %u\nVADDR: 0x%p\nDWORD:0x%x\n", pid, vaddr, out);

	printf("Setting memory to 1337\n");

	DWORD newVal = 1337;
	SIZE_T wrote = 0;
	if (!WriteProcessMemory(hProcess, vaddr, &newVal, sizeof(DWORD), &wrote)) {
		printf("[!] Could not write to %p\n", vaddr);
		printf("Error: %u\n", GetLastError());
		exit(1);
	}

	printf("Wrote 0x%x to %p\n", vaddr, newVal);

	out;
	if (!ReadProcessMemory(hProcess, vaddr, &out, sizeof(DWORD), NULL)) {

		printf("[!] Failed reading memory ...\n");
		exit(1);
	}

	printf("New value of %p is 0x%x\n", vaddr, newVal);

	return 0;
}
