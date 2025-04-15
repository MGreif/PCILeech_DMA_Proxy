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
    return ECommandType::INVALID;
}

bool handleConnected(CommandPayload* payload, OUT_REPLACED Command** command) {
    *command = new ConnectedCommand(*payload->uValue);
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
        if (!handleConnected((CommandPayload*)&thirdChunk, command)) return false; // Could not handle command
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
    }
    (*command)->pid = pid;
    (*command)->type = type;

    return true;
}