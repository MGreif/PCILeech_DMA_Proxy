#include "CommunicationPool.h"
#include "Communication.h"
#include <thread>
#include <chrono>
#include <atomic>
extern inline int g_CommunicationPartnerCounter = 0;

extern HANDLE g_hCommunicationPipe;
extern Process *g_pFirstProcess;
ProcessPool g_ProcessPool = ProcessPool();
std::vector<PrivateCommunicationChannel*> g_PrivateCommunicationChannels = std::vector<PrivateCommunicationChannel*>();
std::atomic<bool> g_PrivateCommunicationThreadStarted(false);
std::thread hPrivateCommunicationThread;

void startPrivateCommunicationThread() {
    int counter = 0;
    // Iterate through all communicationPartners for non-blocking better performance
    while (g_PrivateCommunicationChannels.size() > 0) {

        fflush(stdout);
        if (counter >= g_PrivateCommunicationChannels.size()) counter = 0;
        PrivateCommunicationChannel* channel = g_PrivateCommunicationChannels.at(counter++);
        if (channel == nullptr) continue;

        if (!channel->isPipeConnected()) {
            info("Waiting for private pipe connection on pipe %p %s...\n", channel->getPipe(), channel->getPipeName());
            BOOL connected = ConnectNamedPipe(channel->getPipe(), NULL) ?
                TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (connected) {
                info("Connection received on pipe %p %s\n", channel->getPipe(), channel->getPipeName());
                channel->setPipeConnected(true);
            }
            else {
                DWORD err = GetLastError();
                warning("ConnectNamedPipe failed. Error: %lu on pipe %p %s\n", err, channel->getPipe(), channel->getPipeName());
                std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                continue;
            }

        }

        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(channel->getPipe(), NULL, 0, NULL, &bytesAvailable, NULL)) {
            warning("Could not peek private named pipe. Pipe probably terminated\n");
            channel->setPipeConnected(false);
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



                switch (cmd->type) {
                // case NEW_PROCESS // This creates the new process in the ProxyLoader TODO
                                                      // Check if channel has associated process

                case CONNECTED: {
                    // We presume the Process has already been created
                    ConnectedCommand* connectedCommand = (ConnectedCommand*)cmd;
                    info("Connected new process: %u thread: %u\n", connectedCommand->pid, connectedCommand->tid);

                    Process* associatedProcess = g_ProcessPool.getByPid(connectedCommand->pid);
                    if (associatedProcess == nullptr) {
                        warning("Connect command from unknown process received pid: %u\n", connectedCommand->pid);
                        continue;
                    }
                    if (!associatedProcess->addCommunicationPartner(channel)) {
                        warning("Not setting new communication channel!\n");
                        continue;
                    }

                    // Assign process pointer backlink
                    channel->pCarryingProcess = associatedProcess;
                    associatedProcess->setProxyTid(connectedCommand->tid);
                    info("Correlated dangling channel %p with process %u\n", channel, connectedCommand->pid);

                    // The following 2 lines are for testing
                    //char* noMemoryHookCommand = NoHookingCommand().setSpecifier("mem")->build().serialized;
                    //if (!WriteFile(process->communicationPartner.getPipe(), noMemoryHookCommand, strlen(noMemoryHookCommand), NULL, NULL)) break;

                    // Sending FinishSetup command so the DLL continues execution and does not wait for more commands
                    if (!WriteFile(channel->getPipe(), FinishSetupCommand().build().serialized, strlen(FinishSetupCommand().build().serialized), NULL, NULL)) break;
                    break;
                }
                              case READY_FOR_RESUME:
                                  break;
                                  // Check if channel has associated process
                }

                commandStart = commandEnd + 1;
                commandEnd = strchr(commandStart, COMMAND_END);
            }

        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    warning("Private communication thread stopped\n");
}


void startCommunicationThread() {
    info("Starting communication thread... id: %d\n", GetCurrentThreadId());
    // Wait for connection on the main pipe
    // These connections are then used just for one command to each communication channel
    // This commands issue a transfer to a freshly created private communication channel
    while (g_hCommunicationPipe != INVALID_HANDLE_VALUE && ConnectNamedPipe(g_hCommunicationPipe, NULL)) {
        info("Client connected to pipe\n");

        // CommunicationPartner constructor takes care of creating and removing the pipe
        PrivateCommunicationChannel* newPartner = new PrivateCommunicationChannel();
        g_PrivateCommunicationChannels.push_back(newPartner);

        debug("Initializing transfer to private pipe\n");

        if (!newPartner->getPipe()) {
            error("New channels pipe is invalid\n");
            continue;
        }

        TransferCommand command = TransferCommand(newPartner->getPipeName());

        debug("Added dangling communication channel %p\n", newPartner);

        if (g_PrivateCommunicationThreadStarted.load() == false) {
            debug("Starting private communication thread\n");
            hPrivateCommunicationThread = std::thread(startPrivateCommunicationThread);
            g_PrivateCommunicationThreadStarted.store(true);
        }

        // First stage: client send 9999:T:<private-named-pipe>
        // This issues a transfer to the new private pipe
        if (!WriteFile(g_hCommunicationPipe, command.build().serialized, COMMUNICATION_BUFFER, NULL, NULL)) {
            error("Could not write to file. Broken communication protocol\n");
        }
        debug("Sent transfer command\n");
    }
}