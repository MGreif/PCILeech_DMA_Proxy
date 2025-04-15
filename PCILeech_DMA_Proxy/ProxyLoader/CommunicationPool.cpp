#include "CommunicationPool.h"
#include "Communication.h"
#include <thread>
#include <chrono>
#include <atomic>
extern inline int g_CommunicationPartnerCounter = 0;

extern HANDLE g_hCommunicationPipe;
CommunicationPool g_CommunicationPool = CommunicationPool();
std::atomic<bool> g_PrivateCommunicationThreadStarted(false);
std::thread hPrivateCommunicationThread;

void startPrivateCommunicationThread() {
    int counter = 0;
    while (g_CommunicationPool.count() > 0) {
        if (counter >= g_CommunicationPool.count()) counter = 0;
        CommunicationPartner* partner = g_CommunicationPool.get(counter++);
        if (partner == nullptr) continue;

        if (!partner->isPipeConnected()) {
            info("Waiting for private pipe connection on pipe %p %s...\n", partner->getPipe(), partner->getPipeName());
            BOOL connected = ConnectNamedPipe(partner->getPipe(), NULL) ?
                TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (connected) {
                info("Connection received on pipe %p %s\n", partner->getPipe(), partner->getPipeName());
                partner->setPipeConnected(true);
            }
            else {
                DWORD err = GetLastError();
                warning("ConnectNamedPipe failed. Error: %lu on pipe %p %s\n", err, partner->getPipe(), partner->getPipeName());
                std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                continue;
            }

        }

        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(partner->getPipe(), NULL, 0, NULL, &bytesAvailable, NULL)) {
            warning("Could not peek private named pipe. Pipe probably terminated\n");
            partner->setPipeConnected(false);
            continue;
        }

        if (bytesAvailable > 0) {
            char buffer[COMMUNICATION_BUFFER] = { 0 };
            if (!ReadFile(partner->getPipe(), buffer, sizeof(buffer), NULL, NULL)) error("Could not read private communication pipe\n");
            info("Read from private pipe: %s\n", buffer);
            Command* cmd = nullptr;
            parseCommand(buffer, &cmd);

            switch (cmd->type) {
            case CONNECTED: {
                info("Connected new process: %u\n", (ConnectedCommand*)cmd->pid);
                partner->setPid(((ConnectedCommand*)cmd)->pid);
                //Testing
                char testBuffer[] = "ABC:ABC:123";
                if (!WriteFile(partner->getPipe(), testBuffer, sizeof(testBuffer), NULL, NULL)) break;
                testBuffer[2] = '1';
                if (!WriteFile(partner->getPipe(), FinishSetupCommand().build().serialized, sizeof(FinishSetupCommand().build().serialized), NULL, NULL)) break;
                testBuffer[2] = '2';
                if (!WriteFile(partner->getPipe(), testBuffer, sizeof(testBuffer), NULL, NULL)) break;
                testBuffer[2] = '3';
                if (!WriteFile(partner->getPipe(), testBuffer, sizeof(testBuffer), NULL, NULL)) break;
                break;
            }
            }

        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    warning("Private communication thread stopped\n");
}

void startCommunicationThread() {
    info("Starting communication thread... id: %d\n", GetCurrentThreadId());
        while (g_hCommunicationPipe != INVALID_HANDLE_VALUE && ConnectNamedPipe(g_hCommunicationPipe, NULL)) {
            info("Client connected to pipe\n");
            CommunicationPartner* newPartner = new CommunicationPartner();
            info("Initializing transfer to private pipe\n");

            if (!newPartner->getPipe()) {
                error("New partners pipe is invalid\n");
                continue;
            }

            TransferCommand command = TransferCommand(newPartner->getPipeName());


            g_CommunicationPool.add(newPartner);
            info("Added partner to communication pool\n");

            if (g_PrivateCommunicationThreadStarted.load() == false) {
                info("Starting private communication thread\n");
                hPrivateCommunicationThread = std::thread(startPrivateCommunicationThread);
                g_PrivateCommunicationThreadStarted.store(true);
            }

            // First stage: client send <pid>:connected
            if (!WriteFile(g_hCommunicationPipe, command.build().serialized, COMMUNICATION_BUFFER, NULL, NULL)) {
                error("Could not write to file. Broken communication protocol\n");
            }
            info("Sent transfer command\n");
    }
}