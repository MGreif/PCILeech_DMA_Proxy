#include <iostream>
#include "libs/hooks.h"
#include "CreateRemoteThreadEx_LLA.h"
#include "log.hpp"
#include <thread>
#include <atomic>
#include "Communication.h"
#include "CommunicationPool.h"
#define OUTPUT_BUFFER_SIZE 1024
#define WAIT_TIME_SECONDS 2
#define NO_DATA_TIMEOUT_SECONDS 10
#define DISABLE_TIMEOUT true

#define PIPE_NAME "\\\\.\\pipe\\DMA_PROXY"

HANDLE g_hCommunicationPipe = INVALID_HANDLE_VALUE;
extern ProcessPool g_ProcessPool;
char* g_dllPath = nullptr;
extern std::vector<PrivateCommunicationChannel*> g_PrivateCommunicationChannels;

extern void startPrivateCommunicationThread();

struct ProcessInfo {
    RemoteProcessInfo ids;
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
        return { 0, 0, 0, NULL, NULL, NULL };
    }

    // Ensure the write handle is inheritable
    if (!SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, TRUE)) {
        error("Could not setHandleInformation for writePipe\n");
        return { 0, 0, 0, NULL, NULL, NULL };
    }

    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, FALSE)) {
        error("Could not setHandleInformation for readPipe\n");
        return { 0, 0, 0, NULL, NULL, NULL };
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
        return { pi.dwProcessId, pi.dwThreadId, 0, pi.hProcess, pi.hThread, hReadPipe };
    }
    else {
        std::cerr << "Failed to start process. Error: " << GetLastError() << std::endl;
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return { 0, 0, 0, NULL, NULL, NULL };
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
            warning("Could not peek named pipe. Process probably terminated\n");
            // Process probably terminated
            break;
        }
        if (bytesAvailable > 0) {
            timeoutCounter = 0;
            if (ReadFile(proc.hReadPipe, &buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\x00';
                    char* tempStart = buffer;
                    // For cleaner output
                    char* newLinePos = strchr(tempStart, '\n');
                    while (newLinePos != 0) {
                        if (newLinePos == 0) break;
                        *newLinePos = '\0';
                        std::cout << proc.ids.pid << " (stdout) >> " << tempStart << std::endl;
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
        else if (!DISABLE_TIMEOUT) {
            if (timeoutCounter == 0 || timeoutCounter == ((NO_DATA_TIMEOUT_SECONDS - NO_DATA_TIMEOUT_SECONDS % 2) / 2) || (NO_DATA_TIMEOUT_SECONDS - timeoutCounter) < 3) {
                warning("No bytes available. Maybe provide stdin? %d seconds until exit ...\n", NO_DATA_TIMEOUT_SECONDS - timeoutCounter);
            }
            timeoutCounter++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    warning("Finished outputDisplay thread.\n");
}


ProcessInfo proc;


BOOL WINAPI consoleHandler(DWORD signal) {

    if (signal == CTRL_C_EVENT) {
        error("CTRL+C detected ... Shutting down...\n");
        for (auto p : g_ProcessPool.processList) {
            TerminateProcess(p->hProcess, 0);
            CloseHandle(p->communicationPartner->getPipe());
            CloseHandle(p->hMainThread);
            CloseHandle(p->hProcess);
        }
        CloseHandle(g_hCommunicationPipe);
        VMMDLL_CloseAll();
    }

    return TRUE;
}


void cleanup() {
    if (proc.hProcess != INVALID_HANDLE_VALUE) TerminateProcess(proc.hProcess, 0);
    if (proc.hProcess != INVALID_HANDLE_VALUE) CloseHandle(proc.hProcess);
    if (proc.hReadPipe != INVALID_HANDLE_VALUE) CloseHandle(proc.hReadPipe);
    if (g_hCommunicationPipe != INVALID_HANDLE_VALUE) CloseHandle(g_hCommunicationPipe);
    VMMDLL_CloseAll();
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: ProxyLoader.exe <abs-path-to-proxy-dll> <abs-path-program> [args...]" << std::endl;
        return 1;
    }

    g_dllPath = argv[1];
    std::string exe = argv[2];
    std::string args;
    for (int i = 3; i < argc; ++i) {
        args += std::string(argv[i]) + " ";
    }

    if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
        error("Could not set control handler");
        return 1;
    }


    proc = CreateSuspendedProcess(exe, args);

    // Creating first process
    Process newProcess = Process();
    newProcess.setPid(proc.ids.pid);
    newProcess.setTid(proc.ids.tid);
    newProcess.hProcess = proc.hProcess;
    newProcess.hMainThread = proc.hThread;
    g_ProcessPool.add(&newProcess);

    // Creating first communication channel for initial process
    auto channel = PrivateCommunicationChannel();
    g_PrivateCommunicationChannels.push_back(&channel);
    newProcess.addCommunicationPartner(&channel);
    channel.pCarryingProcess = &newProcess;

    info("Created private communication pipe for initial process\n");

    std::thread communication(startPrivateCommunicationThread);
    std::thread outputDisplay(displayProcessOutput, proc);
    info("Started private communication thread and display process output thread\n");

    if (proc.ids.pid == 0) {
        error("Could not start process");
        cleanup();
        exit(1);
    }

    info("Injecting payload into %s.\n"
        "Make sure to have the DMA_Proxy DLL in an accessible location (e.g. Windows/System32 or in the process's dir).\n"
        "The DMA_Proxy DLL will load FTD3XX.dll, VMM.dll and leechcore.dll. Make sure they are accessible as well.\n",
        exe.c_str());
 
    if (!CreateRemoteThreadEx_LLAInjection(proc.hProcess, g_dllPath)) {
        error("Could not inject thread\n");
        cleanup();
        exit(1);
    }

    outputDisplay.join();
    info("output listener finished, cleaning up...\n");
    cleanup();
    return 0;
}
