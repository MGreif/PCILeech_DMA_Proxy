// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include <TlHelp32.h>
#include "MinHook.h"
#include <stdio.h>
#include "DMALibrary/Memory/Memory.h"
#include "Hooks/hooks.h"
#include <iostream>

DWORD WINAPI start_thread();
extern Memory mem;
#define LOG(fmt, ...) {printf("[Proxy] ");std::printf(fmt, ##__VA_ARGS__); fflush(stdout);};


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        //DisableThreadLibraryCalls(hModule); // Prevent thread attach/detach calls for efficiency
        HANDLE hThread = CreateThread(NULL, 4096, (LPTHREAD_START_ROUTINE)start_thread, NULL, 0, NULL);
        LOG("Started thread %u\n", GetThreadId(hThread));
        return TRUE;
    }
    case DLL_PROCESS_DETACH: {
        break;
    }
    }
    return TRUE;
}


// Define function pointer typedefs
typedef HANDLE(WINAPI* tOpenProcess)(DWORD, BOOL, DWORD);
typedef BOOL(WINAPI* tReadProcessMemory)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
typedef BOOL(WINAPI* tWriteProcessMemory)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
typedef SIZE_T(WINAPI* tVirtualQueryEx)(HANDLE, LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
typedef HANDLE(WINAPI* tCreateToolhelp32Snapshot)(DWORD, DWORD);
typedef BOOL(WINAPI* tProcess32First)(HANDLE, LPPROCESSENTRY32);
typedef BOOL(WINAPI* tProcess32Next)(HANDLE, LPPROCESSENTRY32);
typedef BOOL(WINAPI* tModule32First)(HANDLE, LPMODULEENTRY32);
typedef BOOL(WINAPI* tModule32Next)(HANDLE, LPMODULEENTRY32);
typedef BOOL(WINAPI* tThread32First)(HANDLE, LPTHREADENTRY32);
typedef BOOL(WINAPI* tThread32Next)(HANDLE, LPTHREADENTRY32);

// Declare function pointers
tOpenProcess open_process = nullptr;
tReadProcessMemory read_process_memory = nullptr;
tWriteProcessMemory write_process_memory = nullptr;
tVirtualQueryEx virtual_query = nullptr;
tCreateToolhelp32Snapshot create_tool_help32 = nullptr;
tProcess32First process_32_first = nullptr;
tProcess32Next process_32_next = nullptr;
tModule32First module_32_first = nullptr;
tModule32Next module_32_next = nullptr;
tThread32First thread_32_first = nullptr;
tThread32Next thread_32_next = nullptr;

void resolve_function_addresses() {
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        LOG("Failed to get handle for kernel32.dll\n");
        return;
    }

    open_process = (tOpenProcess)GetProcAddress(hKernel32, "OpenProcess");
    read_process_memory = (tReadProcessMemory)GetProcAddress(hKernel32, "ReadProcessMemory");
    write_process_memory = (tWriteProcessMemory)GetProcAddress(hKernel32, "WriteProcessMemory");
    virtual_query = (tVirtualQueryEx)GetProcAddress(hKernel32, "VirtualQueryEx");
    create_tool_help32 = (tCreateToolhelp32Snapshot)GetProcAddress(hKernel32, "CreateToolhelp32Snapshot");
    process_32_first = (tProcess32First)GetProcAddress(hKernel32, "Process32First");
    process_32_next = (tProcess32Next)GetProcAddress(hKernel32, "Process32Next");
    module_32_first = (tModule32First)GetProcAddress(hKernel32, "Module32First");
    module_32_next = (tModule32Next)GetProcAddress(hKernel32, "Module32Next");
    thread_32_first = (tThread32First)GetProcAddress(hKernel32, "Thread32First");
    thread_32_next = (tThread32Next)GetProcAddress(hKernel32, "Thread32Next");

    // Print results
    LOG("Original functions:\n");
    LOG("OpenProcess: 0x%p\n", open_process);
    LOG("ReadProcessMemory: 0x%p\n", read_process_memory);
    LOG("WriteProcessMemory: 0x%p\n", write_process_memory);
    LOG("VirtualQueryEx: 0x%p\n", virtual_query);
    LOG("CreateToolhelp32Snapshot: 0x%p\n", create_tool_help32);
    LOG("Process32First: 0x%p\n", process_32_first);
    LOG("Process32Next: 0x%p\n", process_32_next);
    LOG("Module32First: 0x%p\n", module_32_first);
    LOG("Module32Next: 0x%p\n", module_32_next);
    LOG("Thread32First: 0x%p\n", thread_32_first);
    LOG("Thread32Next: 0x%p\n", thread_32_next);
}

DWORD WINAPI start_thread() {
    //open console
    
    if (false) {
        AllocConsole();
        freopen_s((FILE**)stdin, "conin$", "r", stdin);
        freopen_s((FILE**)stdout, "conout$", "w", stdout);
        freopen_s((FILE**)stdout, "conout$", "w", stderr);
    }

    
    resolve_function_addresses();

    if (MH_Initialize() != MH_OK) {
        LOG("[!] Could not initialize hooks");
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "OpenProcess", &Hooks::hk_open_process, NULL) != MH_OK) {
        LOG("[!] Could not initialize OpenProcess");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "ReadProcessMemory", &Hooks::hk_read, NULL) != MH_OK) {
        LOG("[!] Could not initialize ReadProcessMemory");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "WriteProcessMemory", &Hooks::hk_write, NULL) != MH_OK) {
        LOG("[!] Could not initialize WriteProcessMemory");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "VirtualQueryEx", &Hooks::hk_virtual_query_ex, NULL) != MH_OK) {
        LOG("[!] Could not initialize VirtualQuery");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    if (MH_CreateHookApi(L"kernel32.dll", "CreateToolhelp32Snapshot", &Hooks::hk_create_tool_help_32_snapshot, NULL) != MH_OK) {
        LOG("[!] Could not initialize CreateToolhelp32Snapshot");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Process32FirstW", &Hooks::hk_process_32_firstW, NULL) != MH_OK) {
        LOG("[!] Could not initialize Process32FirstW");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Process32NextW", &Hooks::hk_process_32_nextW, NULL) != MH_OK) {
        LOG("[!] Could not initialize Process32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Module32FirstW", &Hooks::hk_module_32_firstW, NULL) != MH_OK) {
        LOG("[!] Could not initialize Module32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Module32NextW", &Hooks::hk_module_32_nextW, NULL) != MH_OK) {
        LOG("[!] Could not initialize Module32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Thread32First", &Hooks::hk_thread_32_first, NULL) != MH_OK) {
        LOG("[!] Could not initialize Thread32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Thread32Next", &Hooks::hk_thread_32_next, NULL) != MH_OK) {
        LOG("[!] Could not initialize Thread32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    if (!mem.Init(false, false)) {
        LOG("[!] Could not initialize DMA");
        exit(1);
    };


    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG("[!] Could not enable hooks");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    LOG("Hooks initialized\n");
    return 0;
}