// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include "libs/MinHook.h"
#include "hooks.h"
#include <stdio.h>

DWORD WINAPI startThread();

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        //DisableThreadLibraryCalls(hModule); // Prevent thread attach/detach calls for efficiency
        CreateThread(NULL, 4096, (LPTHREAD_START_ROUTINE)startThread, NULL, 0, NULL);
        return TRUE;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


DWORD WINAPI startThread() {
    //open console
    AllocConsole();

    freopen_s((FILE**)stdin, "conin$", "r", stdin);
    freopen_s((FILE**)stdout, "conout$", "w", stdout);
    freopen_s((FILE**)stdout, "conout$", "w", stderr);



    if (MH_Initialize() != MH_OK) {
        printf("[!] Could not initialize hooks\n");
        return 1;
    }
    printf("Hooks initialized\n");
    if (!MemoryHooks::Init()) {
        printf("Could not initialize DMA\n");
        return 1;
    }


    return 1;
}