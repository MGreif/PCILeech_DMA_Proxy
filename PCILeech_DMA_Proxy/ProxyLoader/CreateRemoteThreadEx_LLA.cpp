#include <Windows.h>
#include "log.hpp"



typedef bool (*_LoadLibraryA)(char*);


bool CreateRemoteThreadEx_LLAInjection(HANDLE hProcess, char dll_path[], HANDLE* hThread) {
    debug("Starting LoadLibraryA Injection ...\n");

    void* pDllPath = VirtualAllocEx(hProcess, NULL, strlen(dll_path), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pDllPath) {
        error("Could not allocate memory in remote process\n");
        return FALSE;
    }

    if (!WriteProcessMemory(hProcess, pDllPath, dll_path, strlen(dll_path), NULL)) {
        error("Failed to write dll path (%s) to allocated memory (0x%p)\n", dll_path, pDllPath);
        return FALSE;
    }

    debug("Dll path written to 0x%p\n", pDllPath);


    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel) {
        error("Could not get kernel handle\n");
        return FALSE;
    }

    debug("Got module handle: %d\n", hKernel);

    _LoadLibraryA* pLLA = (_LoadLibraryA*)GetProcAddress(hKernel, "LoadLibraryA");
    debug("LoadLibraryA at 0x%p\n", pLLA);

    HANDLE pThread = CreateRemoteThreadEx(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLLA, pDllPath, NULL, NULL, NULL);

    *hThread = pThread;

    if (!pThread) {
        error("Could not create thread");
        return FALSE;
    }

    debug("Thread started: %d at address 0x%p with arguments at 0x%p\n", pThread, pLLA, pDllPath);

    int thread_id = GetThreadId(pThread);

    debug("Thread Id: %d\n", thread_id);

    return TRUE;

}
