#include <iostream>
#include "libs/hooks.h"
#include "ThreadHijack_LLA.hpp"
#include "CreateRemoteThreadEx_LLA.h"
#include "log.hpp"
struct ProcessInfo {
    DWORD pid;
    HANDLE hProcess;
    HANDLE hThread;
};

ProcessInfo CreateSuspendedProcess(const std::string& exe, const std::string& args) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa) };
    sa.bInheritHandle = TRUE; // Allow handle inheritance
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        std::cerr << "Failed to create pipe. Error: " << GetLastError() << std::endl;
        return { 0, NULL, NULL};
    }

    // Ensure the write handle is inheritable
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);



    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe; // Redirect stderr too
    si.hStdInput = hReadPipe;
    

    // Create a writable command line (CreateProcess modifies it)
    std::string commandLine = exe + " " + args;
    char* cmd = &commandLine[0];

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cout << "Process started! PID: " << pi.dwProcessId << std::endl;

        return { pi.dwProcessId, pi.hProcess, pi.hThread, };
    }
    else {
        std::cerr << "Failed to start process. Error: " << GetLastError() << std::endl;
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return { 0, NULL, NULL };
    }
}


int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: ProxyLoader.exe <path-to-proxy-dll> <program> [args...]" << std::endl;
        return 1;
    }

    char* dllPath = argv[1];
    std::string exe = argv[2];
    std::string args;
    for (int i = 3; i < argc; ++i) {
        args += std::string(argv[i]) + " ";
    }

    ProcessInfo proc = CreateSuspendedProcess(exe, args);



    if (proc.pid == 0) {
        return 1;
    }

    HANDLE hThread;
    info("Injecting payload into %s.\nMake sure to have the DMA_Proxy DLL in an accessible location (e.g. Windows/System32 or in the process's dir)\n", exe.c_str());
    
    /*
    if (!ThreadHijack_LLAInjection(proc.hProcess, proc.pid, dllPath, &hThread)) {
        error("Could not inject thread\n");
        exit(1);
    }
    */
    if (!CreateRemoteThreadEx_LLAInjection(proc.hProcess, dllPath)) {
        error("Could not inject thread\n");
        exit(1);
    }

    info("Resuming process in 5 seconds...\n");


    Sleep(5000); // Small wait for DMA setup
    MessageBoxA(NULL, "Click to continue", "Click to continue", MB_OK);
    if (!ResumeThread(proc.hThread)) {
        error("Could not resume thread\n");
        exit(1);
    }

    WaitForSingleObject(proc.hProcess, INFINITE);

    return 0;
}
