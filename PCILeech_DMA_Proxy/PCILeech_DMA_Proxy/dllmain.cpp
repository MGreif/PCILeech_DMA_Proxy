// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include <TlHelp32.h>
#include "MinHook.h"
#include <stdio.h>
#include "Hooks/hooks.h"
#include "DMALibrary/Memory/Memory.h"
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



void printFunctionPointers() {
    // Print results
    LOG("OpenProcess: 0x%p\n", Hooks::open_process);
    LOG("CloseHandle: 0x%p\n", Hooks::close_handle);
    LOG("ReadProcessMemory: 0x%p\n", Hooks::read_process_memory);
    LOG("WriteProcessMemory: 0x%p\n", Hooks::write_process_memory);
    LOG("VirtualQueryEx: 0x%p\n", Hooks::virtual_query);
    LOG("VirtualProtectEx: 0x%p\n", Hooks::virtual_protect_ex);
    LOG("CreateToolhelp32Snapshot: 0x%p\n", Hooks::create_tool_help32);
    LOG("Process32First: 0x%p\n", Hooks::process_32_first);
    LOG("Process32Next: 0x%p\n", Hooks::process_32_next);
    LOG("Module32First: 0x%p\n", Hooks::module_32_first);
    LOG("Module32Next: 0x%p\n", Hooks::module_32_next);
    LOG("Thread32First: 0x%p\n", Hooks::thread_32_first);
    LOG("Thread32Next: 0x%p\n", Hooks::thread_32_next);
}

DWORD WINAPI start_thread() {
    //open console
    
    if (false) {
        AllocConsole();
        freopen_s((FILE**)stdin, "conin$", "r", stdin);
        freopen_s((FILE**)stdout, "conout$", "w", stdout);
        freopen_s((FILE**)stdout, "conout$", "w", stderr);
    }

    

    if (MH_Initialize() != MH_OK) {
        LOG("[!] Could not initialize hooks");
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "OpenProcess", &Hooks::hk_open_process, (LPVOID*)&Hooks::open_process) != MH_OK) {
        LOG("[!] Could not initialize OpenProcess");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "CloseHandle", &Hooks::hk_close_handle, (LPVOID*)&Hooks::close_handle) != MH_OK) {
        LOG("[!] Could not initialize CloseHandle");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "ReadProcessMemory", &Hooks::hk_read, (LPVOID*)&Hooks::read_process_memory) != MH_OK) {
        LOG("[!] Could not initialize ReadProcessMemory");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "WriteProcessMemory", &Hooks::hk_write, (LPVOID*)&Hooks::write_process_memory) != MH_OK) {
        LOG("[!] Could not initialize WriteProcessMemory");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "VirtualQueryEx", &Hooks::hk_virtual_query_ex, (LPVOID*)&Hooks::virtual_query) != MH_OK) {
        LOG("[!] Could not initialize VirtualQueryEx");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "VirtualProtectEx", &Hooks::hk_virtual_protect_ex, (LPVOID*)&Hooks::virtual_protect_ex) != MH_OK) {
        LOG("[!] Could not initialize VirtualProtectEx");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "CreateToolhelp32Snapshot", &Hooks::hk_create_tool_help_32_snapshot, (LPVOID*)&Hooks::create_tool_help32) != MH_OK) {
        LOG("[!] Could not initialize CreateToolhelp32Snapshot");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Process32FirstW", &Hooks::hk_process_32_firstW, (LPVOID*)&Hooks::process_32_first) != MH_OK) {
        LOG("[!] Could not initialize Process32FirstW");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Process32NextW", &Hooks::hk_process_32_nextW, (LPVOID*)&Hooks::process_32_next) != MH_OK) {
        LOG("[!] Could not initialize Process32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Module32FirstW", &Hooks::hk_module_32_firstW, (LPVOID*)&Hooks::module_32_first) != MH_OK) {
        LOG("[!] Could not initialize Module32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Module32NextW", &Hooks::hk_module_32_nextW, (LPVOID*)&Hooks::module_32_next) != MH_OK) {
        LOG("[!] Could not initialize Module32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Thread32First", &Hooks::hk_thread_32_first, (LPVOID*)&Hooks::thread_32_first) != MH_OK) {
        LOG("[!] Could not initialize Thread32First");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }
    if (MH_CreateHookApi(L"kernel32.dll", "Thread32Next", &Hooks::hk_thread_32_next, (LPVOID*)&Hooks::thread_32_next) != MH_OK) {
        LOG("[!] Could not initialize Thread32Next");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }

    LOG("Original Functions:\n");
    printFunctionPointers();


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