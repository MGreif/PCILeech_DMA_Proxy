#pragma once

#define COMMUNICATION_BUFFER 1024

#define COMMAND_CONNECTED_LITERAL "C"
#define COMMAND_TRANSFER_LITERAL "T"
#define COMMAND_FINISH_SETUP_LITERAL "F"
#define COMMAND_NO_HOOKING_LITERAL "N"
#define COMMAND_READY_FOR_RESUME_LITERAL "R"
#define COMMAND_NEW_PROCESS_LITERAL "P"
#define COMMAND_DELIMITER ':'
#define COMMAND_END ';'

#define OUT
#define OUT_REPLACED
#include <vector>
#include <Windows.h>



enum ECommandType {
	TRANSFER, //Server->Client 9999:T:<named-pipe-name>;
	CONNECTED, //Client->Server <pid>:C:<proxy-thread-id>;
	FINISH_SETUP, // Server->Client 9999:F:;
	NO_HOOKING, // Server->Client 9999:N:<specifier>; // Specifier is one of threads,console,modules,process,mem,dma
	READY_FOR_RESUME, // Client->Server <oid>:R:;
	NEW_PROCESS, // Client->Server <owner-pid>:P:<newprocess-pid>:<newprocess-main-tid>
	INVALID,
};

union CommandPayload
{
	char content[COMMUNICATION_BUFFER - 20];
	unsigned int* uValue;

};

struct BuiltCommand {
	char serialized[COMMUNICATION_BUFFER];
	void fillSerialized(char* input) {
		strncpy_s(serialized, input, strlen(input));
	}
};

struct CommandDTO {
	ECommandType type;
	unsigned int pid;
	CommandPayload payload;
	char flags;
};


class Command {
public:
	unsigned int pid;
	ECommandType type;
	Command(unsigned int _pid) {
		pid = _pid;
		type = ECommandType::INVALID;
	}
};

class ConnectedCommand : public Command {
public:
	unsigned int tid = 0;

	ConnectedCommand(unsigned int _pid, unsigned int _tid) : Command(_pid) {
		type = ECommandType::CONNECTED;
		pid = _pid;
		tid = _tid;
	}
	BuiltCommand build() {
		BuiltCommand builtCommand;
		sprintf_s(builtCommand.serialized, "%u:C:%u;", pid, tid);
		return builtCommand;
	}
};

class NoHookingCommand : public Command {
	char specifier[COMMUNICATION_BUFFER - 20] = { 0 };
public:
	NoHookingCommand() : Command(9999) {
		type = ECommandType::NO_HOOKING;
	}
	BuiltCommand build() {
		BuiltCommand builtCommand;
		sprintf_s(builtCommand.serialized, "%u:N:%s;", pid, specifier);

		return builtCommand;
	}
	NoHookingCommand* setSpecifier(const char specifier[COMMUNICATION_BUFFER - 20]) {
		strncpy_s(this->specifier, specifier, strlen(specifier));
		return this;
	}
	char* getSpecifier() {
		return specifier;
	}
};

class FinishSetupCommand : public Command {
public:
	FinishSetupCommand() : Command(9999) {
		type = ECommandType::FINISH_SETUP;
	}
	BuiltCommand build() {
		BuiltCommand builtCommand;
		sprintf_s(builtCommand.serialized, "%u:F:;", pid);
		return builtCommand;
	}
};

class ReadyForResumeCommand : public Command {
public:
	ReadyForResumeCommand() : Command(GetCurrentProcessId()) {
		type = ECommandType::READY_FOR_RESUME;
	}
	BuiltCommand build() {
		BuiltCommand builtCommand;
		sprintf_s(builtCommand.serialized, "%u:R:;", pid);
		return builtCommand;
	}
};

class NewProcessCommand : public Command {
public:
	unsigned int newProcessPid, newProcessTid;
	NewProcessCommand(unsigned int _newProcessPid, unsigned int _newProcessTid) : Command(GetCurrentProcessId()) {
		type = ECommandType::NEW_PROCESS;
		newProcessPid = _newProcessPid;
		newProcessTid = _newProcessTid;
	}
	BuiltCommand build() {
		BuiltCommand builtCommand;
		sprintf_s(builtCommand.serialized, "%u:P:%u:%u;", pid, newProcessPid, newProcessTid);
		return builtCommand;
	}
};

bool parseCommand(char* buffer, OUT_REPLACED Command** command);

class TransferCommand :public Command {
	ECommandType type = ECommandType::TRANSFER;
public:
	char namedPipeName[32] = { 0 };
	TransferCommand(char _namedPipeName[32]) : Command(0) {
		strncpy_s(namedPipeName, _namedPipeName, strlen(_namedPipeName));
		fflush(stdout);
	}
	BuiltCommand build() {
		BuiltCommand builtCommand;
		sprintf_s(builtCommand.serialized, "9999:T:%s;", namedPipeName);
		return builtCommand;
	}
	TransferCommand* from(char serialized[COMMUNICATION_BUFFER]) {
		TransferCommand* newCommand = nullptr;

		parseCommand(serialized, (Command**)&newCommand);
		return newCommand;
	}
};




