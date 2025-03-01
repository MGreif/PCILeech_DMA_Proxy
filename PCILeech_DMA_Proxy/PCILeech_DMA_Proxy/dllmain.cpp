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

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        //DisableThreadLibraryCalls(hModule); // Prevent thread attach/detach calls for efficiency
        CreateThread(NULL, 4096, (LPTHREAD_START_ROUTINE)start_thread, NULL, 0, NULL);
        return TRUE;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
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
        std::cerr << "Failed to get handle for kernel32.dll\n";
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
    printf("Original functions:\n");
    std::cout << "OpenProcess: 0x" << open_process << std::endl;
    std::cout << "ReadProcessMemory: 0x" << read_process_memory << std::endl;
    std::cout << "WriteProcessMemory: 0x" << write_process_memory << std::endl;
    std::cout << "VirtualQueryEx: 0x" << virtual_query << std::endl;
    std::cout << "CreateToolhelp32Snapshot: 0x" << create_tool_help32 << std::endl;
    std::cout << "Process32First: 0x" << process_32_first << std::endl;
    std::cout << "Process32Next: 0x" << process_32_next << std::endl;
    std::cout << "Module32First: 0x" << module_32_first << std::endl;
    std::cout << "Module32Next: 0x" << module_32_next << std::endl;
    std::cout << "Thread32First: 0x" << thread_32_first << std::endl;
    std::cout << "Thread32Next: 0x" << thread_32_next << std::endl;
}

DWORD WINAPI start_thread() {
    //open console
    AllocConsole();

    freopen_s((FILE**)stdin, "conin$", "r", stdin);
    freopen_s((FILE**)stdout, "conout$", "w", stdout);
    freopen_s((FILE**)stdout, "conout$", "w", stderr);

    resolve_function_addresses();

    if (MH_Initialize() != MH_OK) {
        printf("[!] Could not initialize hooks");
        exit(1);
    }

    printf("Initialized DMA with handle %x\n", mem.vHandle);
    if (MH_CreateHookApi(L"kernel32.dll", "OpenProcess", &Hooks::hk_open_process, NULL) != MH_OK) {
        printf("[!] Could not initialize OpenProcess");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "ReadProcessMemory", &Hooks::hk_read, NULL) != MH_OK) {
        printf("[!] Could not initialize ReadProcessMemory");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "WriteProcessMemory", &Hooks::hk_write, NULL) != MH_OK) {
        printf("[!] Could not initialize WriteProcessMemory");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "VirtualQuery", &Hooks::hk_virtual_query, NULL) != MH_OK) {
        printf("[!] Could not initialize VirtualQuery");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    if (MH_CreateHookApi(L"kernel32.dll", "CreateToolhelp32Snapshot", &Hooks::hk_create_tool_help_32_snapshot, NULL) != MH_OK) {
        printf("[!] Could not initialize CreateToolhelp32Snapshot");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Process32First", &Hooks::hk_process_32_first, NULL) != MH_OK) {
        printf("[!] Could not initialize Process32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Process32Next", &Hooks::hk_process_32_next, NULL) != MH_OK) {
        printf("[!] Could not initialize Process32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Module32First", &Hooks::hk_module_32_first, NULL) != MH_OK) {
        printf("[!] Could not initialize Module32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Module32Next", &Hooks::hk_module_32_next, NULL) != MH_OK) {
        printf("[!] Could not initialize Module32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Thread32First", &Hooks::hk_thread_32_first, NULL) != MH_OK) {
        printf("[!] Could not initialize Thread32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Thread32Next", &Hooks::hk_thread_32_next, NULL) != MH_OK) {
        printf("[!] Could not initialize Thread32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    if (!mem.Init(false, false)) {
        printf("[!] Could not initialize DMA");
        exit(1);
    };


    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        printf("[!] Could not enable hooks");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    printf("Hooks initialized\n");

    return 0;
}