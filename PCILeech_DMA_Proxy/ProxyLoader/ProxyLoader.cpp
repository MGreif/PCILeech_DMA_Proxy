#include <iostream>
#include "libs/hooks.h"
#include "CreateRemoteThreadEx_LLA.h"
#include "log.hpp"
#include <thread>
#include <atomic>
#define OUTPUT_BUFFER_SIZE 1024
#define WAIT_TIME_SECONDS 3
#define NO_DATA_TIMEOUT_SECONDS 10
std::atomic<bool> gbProcessSuspended(true);

struct ProcessInfo {
    DWORD pid;
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hReadPipe;
};

ProcessInfo CreateSuspendedProcess(const std::string& exe, const std::string& args) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), 0 };
    sa.bInheritHandle = TRUE; // Allow handle inheritance
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        error("Failed to create pipe\n");
        return { 0, NULL, NULL, NULL};
    }

    // Ensure the write handle is inheritable
    if (!SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, TRUE)) {
        error("Could not setHandleInformation for writePipe\n");
        return { 0, NULL, NULL, NULL };
    }

    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, FALSE)) {
        error("Could not setHandleInformation for readPipe\n");
        return { 0, NULL, NULL, NULL };
    }

    STARTUPINFOA si = { sizeof(si), 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe; // Redirect stderr too
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES; 

    // Create a writable command line (CreateProcess modifies it)
    std::string commandLine = exe + " " + args;
    char* cmd = &commandLine[0];

    info("Starting process ...\n");
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        printf("[%u] Process started!\n", pi.dwProcessId);
        CloseHandle(hWritePipe);
        return { pi.dwProcessId, pi.hProcess, pi.hThread, hReadPipe };
    }
    else {
        std::cerr << "Failed to start process. Error: " << GetLastError() << std::endl;
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return { 0, NULL, NULL, NULL };
    }
}

void displayProcessOutput(ProcessInfo proc) {
    char buffer[OUTPUT_BUFFER_SIZE] = { 0 };
    DWORD bytesRead = 0;
    DWORD bytesAvailable;
    int timeoutCounter = 0;
    fflush(stdout);
    info("Starting outputDisplay thread\n");
    while (timeoutCounter < NO_DATA_TIMEOUT_SECONDS) {
        if (!PeekNamedPipe(proc.hReadPipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
            // Process probably terminated
            break;
        }
        if (bytesAvailable > 0) {
            timeoutCounter = 0;
            if (ReadFile(proc.hReadPipe, &buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\x00';
                    char* tempStart = buffer;
                    char* newLinePos = strchr(tempStart, '\n');
                    while (newLinePos != 0) {
                        if (newLinePos == 0) break;
                        *newLinePos = '\0';
                        std::cout << proc.pid << " >> " << tempStart << std::endl;
                        tempStart = newLinePos + 1;
                        newLinePos = strchr(tempStart, '\n');

                    };
                }
                else {
                    break;
                }
            }
        }
        else if (GetLastError() == ERROR_BROKEN_PIPE) {
            warning("Pipe broke. Process probably terminated\n");
            break;
        }
        else if (!gbProcessSuspended.load()) {
            if (timeoutCounter == 0 || timeoutCounter == ((NO_DATA_TIMEOUT_SECONDS - NO_DATA_TIMEOUT_SECONDS % 2) / 2) || (NO_DATA_TIMEOUT_SECONDS - timeoutCounter) < 3) {
                warning("No bytes available. Maybe provide stdin? %d seconds until exit ...\n", NO_DATA_TIMEOUT_SECONDS - timeoutCounter);
            }
            timeoutCounter++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

ProcessInfo proc;


BOOL WINAPI consoleHandler(DWORD signal) {

    if (signal == CTRL_C_EVENT) {
        error("CTRL+C detected ... Shutting down...\n");
        TerminateProcess(proc.hProcess, 0);
        VMMDLL_CloseAll();
    }

    return TRUE;
}


int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: ProxyLoader.exe <abs-path-to-proxy-dll> <abs-path-program> [args...]" << std::endl;
        return 1;
    }

    char* dllPath = argv[1];
    std::string exe = argv[2];
    std::string args;
    for (int i = 3; i < argc; ++i) {
        args += std::string(argv[i]) + " ";
    }

    if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
        printf("\nERROR: Could not set control handler");
        return 1;
    }


    proc = CreateSuspendedProcess(exe, args);

    std::thread outputDisplay(displayProcessOutput, proc);

    if (proc.pid == 0) {
        error("Could not start process");
        exit(1);
    }

    HANDLE hThread;
    info("Injecting payload into %s.\n"
        "Make sure to have the DMA_Proxy DLL in an accessible location (e.g. Windows/System32 or in the process's dir).\n"
        "The DMA_Proxy DLL will load FTD3XX.dll, VMM.dll and leechcore.dll. Make sure they are accessible as well.\n",
        exe.c_str());
 
    if (!CreateRemoteThreadEx_LLAInjection(proc.hProcess, dllPath)) {
        error("Could not inject thread\n");
        CloseHandle(proc.hProcess);
        CloseHandle(proc.hReadPipe);
        exit(1);
    }


    info("Resuming process in %u seconds so DMA/VMM can initialize...\n", WAIT_TIME_SECONDS);
    std::this_thread::sleep_for(std::chrono::seconds(WAIT_TIME_SECONDS)); // Small wait so injected DLL can initialize DMA
    if (ResumeThread(proc.hThread) < 0) {
        error("Could not resume thread\n");
        printf("Error: %u\n", GetLastError());
        exit(1);
    }
    info("Resumed main thread of target process\n");
    gbProcessSuspended.store(false);

    outputDisplay.join();
    info("output listener finished, cleaning up...\n");
    TerminateProcess(proc.hProcess, 0);
    CloseHandle(proc.hProcess);
    CloseHandle(proc.hReadPipe);
    VMMDLL_CloseAll();
    return 0;
}
