#include <Windows.h>
#include "Communication.h"
#include <vector>


ECommandType parseType(char* buffer) {
    if (strncmp(buffer, COMMAND_CONNECTED_LITERAL, strlen(COMMAND_CONNECTED_LITERAL)) == 0) {
        return ECommandType::CONNECTED;
    }
    else if (strncmp(buffer, COMMAND_TRANSFER_LITERAL, strlen(COMMAND_TRANSFER_LITERAL)) == 0) {
        return ECommandType::TRANSFER;
    }
    else if (strncmp(buffer, COMMAND_FINISH_SETUP_LITERAL, strlen(COMMAND_FINISH_SETUP_LITERAL)) == 0) {
        return ECommandType::FINISH_SETUP;
    }
    else if (strncmp(buffer, COMMAND_NO_HOOKING_LITERAL, strlen(COMMAND_NO_HOOKING_LITERAL)) == 0) {
        return ECommandType::NO_HOOKING;
    }
    else if (strncmp(buffer, COMMAND_READY_FOR_RESUME_LITERAL, strlen(COMMAND_READY_FOR_RESUME_LITERAL)) == 0) {
        return ECommandType::READY_FOR_RESUME;
    }
    else if (strncmp(buffer, COMMAND_NEW_PROCESS_LITERAL, strlen(COMMAND_NEW_PROCESS_LITERAL)) == 0) {
        return ECommandType::NEW_PROCESS;
    }
    return ECommandType::INVALID;
}

bool handleConnected(CommandPayload* payload, OUT_REPLACED Command** command, unsigned int pid) {
    *command = new ConnectedCommand(pid, *payload->uValue);
    return true;
}

bool handleTransfer(CommandPayload* payload, OUT_REPLACED Command** command) {
    *command = new TransferCommand(*(char**)payload->content);
    return true;
}

bool handleFinishSetup(CommandPayload* payload, OUT_REPLACED Command** command) {
    *command = new FinishSetupCommand();
    return true;
}

bool handleNoHooking(CommandPayload* payload, OUT_REPLACED Command** command) {
    *command = new NoHookingCommand();
    ((NoHookingCommand*) command)->setSpecifier(*(char**)payload->content);
    return true;
}

bool handleReadyForResume(CommandPayload* payload, OUT_REPLACED Command** command) {
    *command = new ReadyForResumeCommand();
    return true;
}

bool handleNewProcess(CommandPayload* payload, OUT_REPLACED Command** command) {
    char payloadContentClone[COMMUNICATION_BUFFER - 20];
    memcpy(payloadContentClone, payload->content, strlen(payload->content));
    char tidString[16] = { 0 };
    char pidString[16] = { 0 };
    char* firstDelimiter = strchr(payloadContentClone, COMMAND_DELIMITER);
    *firstDelimiter = '\00';
    strncpy_s(pidString, payloadContentClone, strlen(payloadContentClone));
    strncpy_s(tidString, firstDelimiter+1, strlen(firstDelimiter+1));
    *command = new NewProcessCommand(strtoull(pidString, nullptr, 10), strtoull(tidString, nullptr, 10));
    return true;
}

bool parseCommand(char* buffer, OUT_REPLACED Command** command) {
    size_t actualSize = strlen(buffer);
    if (actualSize == 0) return false; // Null command

    char bufferCopy[COMMUNICATION_BUFFER] = { 0 };
    memcpy(bufferCopy, buffer, actualSize);

    char* firstDelimeter = strchr(bufferCopy, COMMAND_DELIMITER);
    if (firstDelimeter == nullptr) return false; // invalid command schema

    *firstDelimeter = '\00';
    char* secondChunk = firstDelimeter + 1;
    unsigned int pid = strtoull(bufferCopy, nullptr, 10);

    char* secondDelimiter = strchr(secondChunk, COMMAND_DELIMITER);
    if (secondDelimiter != nullptr) {
        *secondDelimiter = '\00';
    }
    char* thirdChunk = secondDelimiter + 1;
    ECommandType type = parseType(secondChunk);
    if (type == ECommandType::INVALID || pid <= 0) return false; // Invalid command type

    switch (type) {
    case CONNECTED:
    {
        if (!handleConnected((CommandPayload*)&thirdChunk, command, pid)) return false;
        break;
    }
    case TRANSFER: {
        if (!handleTransfer((CommandPayload*)&thirdChunk, command)) return false;
        break;
    }
    case FINISH_SETUP: {
        if (!handleFinishSetup((CommandPayload*)&thirdChunk, command)) return false;
        break;
    }
    case NO_HOOKING: {
        if (!handleNoHooking((CommandPayload*)&thirdChunk, command)) return false;
        break;
    }
    case READY_FOR_RESUME: {
        if (!handleReadyForResume((CommandPayload*)&thirdChunk, command)) return false;
        break;
    }
    }
    (*command)->pid = pid;
    (*command)->type = type;

    return true;
}