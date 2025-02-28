#include <iostream>
#include <Windows.h>

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

	HANDLE hProcess = OpenProcess(PROCESS_VM_READ, 0, pid);
	DWORD out;
	if (!ReadProcessMemory(hProcess, vaddr, &out, sizeof(DWORD), NULL)) {
		printf("[!] Failed reading memory ...\n");
		exit(1);
	}

	printf("Read memory...\n\nPID: %u\nVADDR: 0x%p\nDWORD:0x%x\n", pid, vaddr, out);
}
