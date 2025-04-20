// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include <TlHelp32.h>
#include "MinHook.h"
#include <stdio.h>
#include "Hooks/hooks.h"
#include "DMALibrary/Memory/Memory.h"
#include <iostream>
#include "Communication/Communication.h"

DWORD WINAPI start_thread();
extern Memory mem;
#define LOG(fmt, ...) {printf("[Proxy] ");std::printf(fmt, ##__VA_ARGS__); fflush(stdout);};
#define LOGW(fmt, ...) {wprintf(L"[Proxy] ");std::wprintf(fmt, ##__VA_ARGS__); fflush(stdout);};
HANDLE g_hPrivateCommunicationPipe = INVALID_HANDLE_VALUE;



struct Configuration {
    boolean hookProcess;
    boolean hookMemory;
    boolean hookThreads;
    boolean hookModules;
    boolean hookAllocConsole;
    boolean initDMA;
};

// All true by default
static Configuration g_Configuration = {
    true,
    true,
    true,
    true,
    true,
    true,
};

void handleNoHookingCommand(NoHookingCommand* cmd) {
    if (strncmp(cmd->getSpecifier(), "mem", sizeof("mem")) == 0) {
        g_Configuration.hookMemory = false;
    } else if (strncmp(cmd->getSpecifier(), "process", sizeof("process")) == 0) {
        g_Configuration.hookProcess = false;
    }
    else if (strncmp(cmd->getSpecifier(), "threads", sizeof("threads")) == 0) {
        g_Configuration.hookThreads = false;
    }
    else if(strncmp(cmd->getSpecifier(), "modules", sizeof("modules")) == 0) {
        g_Configuration.hookModules = false;
    }
    else if (strncmp(cmd->getSpecifier(), "console", sizeof("console")) == 0) {
        g_Configuration.hookAllocConsole = false;
    }
    else if (strncmp(cmd->getSpecifier(), "dma", sizeof("dma")) == 0) {
        g_Configuration.initDMA = false;
    }
}

bool g_bFinishedSetup = false;

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
    if (g_Configuration.hookMemory) {
        LOG("OpenProcess: 0x%p\n", Hooks::open_process);
        LOG("CloseHandle: 0x%p\n", Hooks::close_handle);
        LOG("ReadProcessMemory: 0x%p\n", Hooks::read_process_memory);
        LOG("WriteProcessMemory: 0x%p\n", Hooks::write_process_memory);
        LOG("VirtualQueryEx: 0x%p\n", Hooks::virtual_query);
        LOG("VirtualProtectEx: 0x%p\n", Hooks::virtual_protect_ex);
    }
    if (g_Configuration.hookProcess) {
        LOG("CreateProcessW: 0x%p\n", Hooks::create_process_w);
        LOG("CreateProcessA: 0x%p\n", Hooks::create_process_a);
        LOG("CreateToolhelp32Snapshot: 0x%p\n", Hooks::create_tool_help32);
        LOG("Process32First: 0x%p\n", Hooks::process_32_first);
        LOG("Process32Next: 0x%p\n", Hooks::process_32_next);
    }

    if (g_Configuration.hookModules) {
        LOG("Module32First: 0x%p\n", Hooks::module_32_first);
        LOG("Module32Next: 0x%p\n", Hooks::module_32_next);
    }

    if (g_Configuration.hookThreads) {
        LOG("Thread32First: 0x%p\n", Hooks::thread_32_first);
        LOG("Thread32Next: 0x%p\n", Hooks::thread_32_next);
    }

}

BOOL hk_alloc_console() {
    return true;
}

DWORD WINAPI start_thread() {
    
    // Allocating a new console messed with STDOUT, so its commented out for now...
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
    fflush(stdout);

    // Connecting to ProxyLoader via named pipe
    HANDLE hCommunicationPipe = CreateFileA("\\\\.\\pipe\\DMA_PROXY", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, NULL, NULL);
    if (hCommunicationPipe) {

        // When first connected to the main pipe, the ProxyLoader instantly issues a TransferCommand to switch to a private communication pipe
        char bufferOut[1024] = { 0 };
        if (!ReadFile(hCommunicationPipe, bufferOut, sizeof(bufferOut), NULL, NULL)) LOG("Could not read communication pipe\n");
        char* commandEnd = strchr(bufferOut, COMMAND_END);
        *commandEnd = '\0';
        char* commandStart = bufferOut;
        fflush(stdout);

        TransferCommand* transferCommand = nullptr;
        if (!parseCommand(commandStart, (Command**)&transferCommand)) {
            LOG("Could not parse transfer command\n");
            goto HOOKING;
        }
        LOG("Got transfer command with named pipe: %s\n", transferCommand->namedPipeName);

        g_hPrivateCommunicationPipe = CreateFileA(transferCommand->namedPipeName, GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, NULL, NULL);
        if (!g_hPrivateCommunicationPipe) 
        {
            LOG("Could not open private communication pipe\n");
            goto HOOKING;
        }
        delete transferCommand;

        // Sending a ConnectedCommand with the current processId
        ConnectedCommand connectedCommand = ConnectedCommand(GetCurrentProcessId(), GetCurrentThreadId());
        BuiltCommand built = connectedCommand.build();

        if (!WriteFile(g_hPrivateCommunicationPipe, built.serialized, strlen(built.serialized), NULL, NULL)) LOG("Could not write to private communication pipe\n");

        // Currently there is no acknowledgement whatsoever, we just hope windows does a good job :shrug:

        LOG("Now accepting configuration commands\n");
        ZeroMemory(bufferOut, COMMUNICATION_BUFFER);
        while (!g_bFinishedSetup && ReadFile(g_hPrivateCommunicationPipe, bufferOut, sizeof(bufferOut), NULL, NULL) ) {
            char* commandEnd = strchr(bufferOut, COMMAND_END);
            char* commandStart = bufferOut;
            LOG("Received command buffer: %s\n", bufferOut);
            while (commandEnd != nullptr) {
                *commandEnd = '\00';
                LOG("Processing command: %s\n", commandStart);
                Command* cmd = nullptr;
                if (!parseCommand(commandStart, &cmd)) {
                    LOG("Could not parse command: %s\n", commandStart);
                    goto END_COMMAND_PROCESS;
                }

                switch (cmd->type) {
                case FINISH_SETUP: {
                    LOG("Finishing setup ...\n");
                    g_bFinishedSetup = true;
                    goto HOOKING;
                }
                case NO_HOOKING: {
                    handleNoHookingCommand((NoHookingCommand*)&cmd);
                }
                }
            END_COMMAND_PROCESS:
                commandStart = commandEnd + 1;
                commandEnd = strchr(commandStart, COMMAND_END);
            }
            ZeroMemory(bufferOut, COMMUNICATION_BUFFER);
        }
    }
    else {
        LOG("Named pipe not found. Either something crashed or this DLL was manually injected\n");
    }
HOOKING:
    LOG("Starting hooking phase\n");
    fflush(stdout);
    if (g_Configuration.hookAllocConsole) {
        LOG("Hooking console\n");
        // Hooking Alloc Console so host process does not malform output
        if (MH_CreateHookApi(L"kernel32.dll", "AllocConsole", &hk_alloc_console, NULL) != MH_OK) {
            LOG("[!] Could not initialize AllocConsole");
            VMMDLL_Close(mem.vHandle);
            exit(1);
        }
    }


    if (g_Configuration.hookMemory) {
        LOG("Hooking mem\n");
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
    }

    if (g_Configuration.hookProcess) {
        LOG("Hooking process\n");

        if (MH_CreateHookApi(L"kernel32.dll", "CreateProcessW", &Hooks::hk_create_process_w, (LPVOID*)&Hooks::create_process_w) != MH_OK) {
            LOG("[!] Could not initialize CreateProcessW");
            VMMDLL_Close(mem.vHandle);
            exit(1);
        }

        if (MH_CreateHookApi(L"kernel32.dll", "CreateProcessA", &Hooks::hk_create_process_a, (LPVOID*)&Hooks::create_process_a) != MH_OK) {
            LOG("[!] Could not initialize CreateProcessA");
            VMMDLL_Close(mem.vHandle);
            exit(1);
        }

        if (MH_CreateHookApi(L"kernel32.dll", "CreateToolhelp32Snapshot", &Hooks::hk_create_tool_help_32_snapshot, (LPVOID*)&Hooks::create_tool_help32) != MH_OK) {
            LOG("[!] Could not initialize CreateToolhelp32Snapshot");
            VMMDLL_Close(mem.vHandle);
            exit(1);
        }

        //if (MH_CreateHookApi(L"kernel32.dll", "NtQuerySystemInformation", &Hooks::hk_nt_query_system_information, (LPVOID*)&Hooks::nt_query_system_information) != MH_OK) {
        //    LOG("[!] Could not initialize NtQuerySystemInformation");
        //    VMMDLL_Close(mem.vHandle);
        //    exit(1);
        //} // commented out for now

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
    }

    if (g_Configuration.hookModules) {
        LOG("Hooking modules\n");

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
    }

    if (g_Configuration.hookThreads) {
        LOG("Hooking threads\n");

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
    }


    LOG("Original Functions:\n");
    printFunctionPointers();

    if (g_Configuration.initDMA) {
        if (!mem.Init(false, false)) {
            LOG("[!] Could not initialize DMA. Terminating Process\n");
            exit(1);
        };
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG("[!] Could not enable hooks");
        VMMDLL_Close(mem.vHandle);
        exit(1);
    }



    LOG("Hooks initialized\n");
    return 0;
}