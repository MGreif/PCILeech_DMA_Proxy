#include "CommunicationPool.h"
#include "Communication.h"
#include <thread>
#include <chrono>
#include <atomic>
#include "CreateRemoteThreadEx_LLA.h"

extern inline int g_CommunicationPartnerCounter = 0;
extern char* g_dllPath;
ProcessPool g_ProcessPool = ProcessPool();
std::vector<PrivateCommunicationChannel*> g_PrivateCommunicationChannels = std::vector<PrivateCommunicationChannel*>();
std::atomic<bool> g_PrivateCommunicationThreadStarted(false);
std::atomic<bool> g_bDMAInitialized(false);
std::thread hPrivateCommunicationThread;

HANDLE g_VMMHandle = INVALID_HANDLE_VALUE;

void startPrivateCommunicationThread() {
    int counter = 0;
    // Iterate through all communicationPartners for better performance
    while (g_PrivateCommunicationChannels.size() > 0) {

        fflush(stdout);
        if (counter >= g_PrivateCommunicationChannels.size()) counter = 0;
        PrivateCommunicationChannel* channel = g_PrivateCommunicationChannels.at(counter++);
        if (channel == nullptr) continue;

        if (!channel->isPipeConnected()) {
            info("Waiting for private pipe connection on pipe %p %s on process %u...\n", channel->getPipe(), channel->getPipeName(), channel->pCarryingProcess->getPid());
            BOOL connected = ConnectNamedPipe(channel->getPipe(), NULL) ?
                TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (connected) {
                info("Connection received on pipe %p %s\n", channel->getPipe(), channel->getPipeName());
                channel->setPipeConnected(true);
            }
            else {
                DWORD err = GetLastError();
                warning("ConnectNamedPipe failed. Error: %lu on pipe %p %s on process %u\n", err, channel->getPipe(), channel->getPipeName(), channel->pCarryingProcess->getPid());
                continue;
            }

        }

        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(channel->getPipe(), NULL, 0, NULL, &bytesAvailable, NULL)) {
            warning("Could not peek private named pipe of process %u. Pipe probably terminated\n", channel->pCarryingProcess->getPid());
            channel->setPipeConnected(false);
            
            g_PrivateCommunicationChannels.erase(std::remove(g_PrivateCommunicationChannels.begin(), g_PrivateCommunicationChannels.end(), channel), g_PrivateCommunicationChannels.end());
            warning("Removed channel from channel pool\n");
            continue;
        }


        if (bytesAvailable > 0) {
            char buffer[COMMUNICATION_BUFFER] = { 0 };
            if (!ReadFile(channel->getPipe(), buffer, sizeof(buffer), NULL, NULL)) error("Could not read private communication pipe\n");
            info("Read from private pipe: %s\n", buffer);
            fflush(stdout);

            char* commandEnd = strchr(buffer, COMMAND_END);
            char* commandStart = buffer;
            while (commandEnd != nullptr) {
                *commandEnd = '\00';
                Command* cmd = nullptr;

                parseCommand(commandStart, &cmd);

                if (cmd->type != CONNECTED) {
                    if (channel->pCarryingProcess->getPid() != cmd->pid) {
                        error("Private Communication Channel sent a command with a PID (%u) that is not the PID (%u) of the associated process. Aborting....\n", cmd->pid, channel->pCarryingProcess->getPid());
                        continue;
                    }
                }

                switch (cmd->type) {                
                case CONNECTED: {
                    // We presume the Process has already been created
                    ConnectedCommand* connectedCommand = (ConnectedCommand*)cmd;
                    info("Connected new process: %u thread: %u\n", connectedCommand->pid, connectedCommand->tid);

                    Process* associatedProcess = g_ProcessPool.getByPid(connectedCommand->pid);
                    if (associatedProcess == nullptr) {
                        warning("Connect command from unknown process received pid: %u\n", connectedCommand->pid);
                        break;
                    }

                    // Assign process pointer backlink
                    associatedProcess->setProxyTid(connectedCommand->tid);

                    if (!g_bDMAInitialized.load()) {
                        g_bDMAInitialized.store(true);
                    }
                    else {
                        //char* noMemoryHookCommand = NoHookingCommand().setSpecifier("dma")->build().serialized;
                        //if (!WriteFile(channel->getPipe(), noMemoryHookCommand, strlen(noMemoryHookCommand), NULL, NULL)) break;
                        if (g_VMMHandle != INVALID_HANDLE_VALUE) {
                            //char* sendHandleCommand = SendHandleCommand(g_VMMHandle).build().serialized;
                            //if (!WriteFile(channel->getPipe(), sendHandleCommand, strlen(sendHandleCommand), NULL, NULL)) break;
                            info("Send VMMHandle to process pid %u\n", channel->pCarryingProcess->getPid());
                        }

                    }

                    // Sending FinishSetup command so the DLL continues execution and does not wait for more commands
                    if (!WriteFile(channel->getPipe(), FinishSetupCommand().build().serialized, strlen(FinishSetupCommand().build().serialized), NULL, NULL)) break;
                    break;
                }
                case NEW_PROCESS: // This creates the new process in the ProxyLoader TODO
                {
                    NewProcessCommand* newProcessCommand = (NewProcessCommand*)cmd;
                    info("New process seems to have started with pid: %u and tid: %u\n", newProcessCommand->newProcessPid, newProcessCommand->newProcessTid);

                    // Adding process to process pool
                    Process* newProcess = new Process();
                    newProcess->setPid(newProcessCommand->newProcessPid);
                    newProcess->setTid(newProcessCommand->newProcessTid);
                    g_ProcessPool.add(newProcess);

                    HANDLE hNewProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION, false, newProcess->getPid());
                    HANDLE hNewProcessMainThread = OpenThread(PROCESS_VM_WRITE | PROCESS_SUSPEND_RESUME | PROCESS_VM_OPERATION | THREAD_RESUME, false, newProcess->getTid());

                    if (hNewProcess == INVALID_HANDLE_VALUE || hNewProcessMainThread == INVALID_HANDLE_VALUE) {
                        error("Could not open process or thread handle\n");
                        break;
                    }

                    newProcess->hProcess = hNewProcess;
                    newProcess->hMainThread = hNewProcessMainThread;
                    
                    auto channel = new PrivateCommunicationChannel();
                    newProcess->addCommunicationPartner(channel);
                    channel->pCarryingProcess = newProcess;
                    g_PrivateCommunicationChannels.push_back(channel);
                    // Injecting DLL
                    if (!CreateRemoteThreadEx_LLAInjection(hNewProcess, g_dllPath)) {
                        error("Could not inject thread\n");
                        exit(1);
                    }
                    info("Successfully injected dll into process pid %u\n", newProcess->getPid());
                    break;

                }
                case READY_FOR_RESUME:
                {
                    DWORD resumed = ResumeThread(channel->pCarryingProcess->hMainThread);
                    if (resumed == -1) {
                        error("Failed resuming pid %u tid %u\n", channel->pCarryingProcess->getPid(), channel->pCarryingProcess->getTid());
                        break;
                    }
                    else {
                        info("Resumed %d threads in process %u thread %u\n", resumed, channel->pCarryingProcess->getPid(), channel->pCarryingProcess->getTid());

                    }
                    break;
                }
                case PASS_HANDLE:{
                    PassHandleCommand* passHandleCommand = (PassHandleCommand*)cmd;
                    info("Received VMM handle %u\n", passHandleCommand->handle);
                    if (g_VMMHandle != INVALID_HANDLE_VALUE) {
                        warning("Handle already set. Overwriting it!\n");
                    }
                    g_VMMHandle = passHandleCommand->handle;
                    break;
                }
                case INVALID:
                    warning("Could not parse command %s\n", commandStart);
                }

                commandStart = commandEnd + 1;
                commandEnd = strchr(commandStart, COMMAND_END);
            }

        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    warning("Private communication thread stopped\n");
}