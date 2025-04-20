#pragma once
#include <Windows.h>
#include <stdio.h>
#include "log.hpp"
#include <vector>
inline int g_CommunicationPartnerCounter;
void startCommunicationThread();

struct RemoteProcessInfo {
	DWORD pid;
	DWORD tid; // Main thread id
	DWORD proxyTid;
};

class Process;

class PrivateCommunicationChannel {
	char privatePipeName[32] = { 0 };
	HANDLE privatePipe = INVALID_HANDLE_VALUE;
	bool pipeConnected = false;
public:
	Process* pCarryingProcess = nullptr;
	PrivateCommunicationChannel() {
		sprintf_s(privatePipeName, "\\\\.\\pipe\\DMA_PROXY%d", g_CommunicationPartnerCounter++);
		privatePipe = CreateNamedPipeA(privatePipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 5, 1024, 1024, 10000, NULL);
		if (privatePipe == INVALID_HANDLE_VALUE) error("Could not create private pipe\n");
		info("New communicationPartner with pipe name: %s and handle %p\n", privatePipeName, privatePipe);
	}
	~PrivateCommunicationChannel() {
		CloseHandle(privatePipe);
	}
	HANDLE getPipe() {
		return privatePipe;
	}
	char* getPipeName() {
		return privatePipeName;
	}
	bool isPipeConnected() {
		return this->pipeConnected;
	}

	void setPipeConnected(bool val) {
		this->pipeConnected = val;
	}
};

class Process {
private:
	RemoteProcessInfo ids = { 0 };

public:
	PrivateCommunicationChannel* communicationPartner;

	Process() {
		
	}

	bool addCommunicationPartner (PrivateCommunicationChannel* partner) {
		if (communicationPartner == nullptr) {
			communicationPartner = partner;
			return true;
		}
		else {
			// Already has a cmmunicationChannel. It shouldnt be set twice
			return false;
		}
	}

	void setPid(unsigned int pid) {
		this->ids.pid = pid;
	}

	unsigned int getPid() {
		return this->ids.pid;
	}

	void setTid(unsigned int tid) {
		this->ids.tid = tid;
	}

	unsigned int getTid() {
		return this->ids.tid;
	}

	void setProxyTid(unsigned int tid) {
		this->ids.proxyTid = tid;
	}

	unsigned int getProxyTid() {
		return this->ids.proxyTid;
	}
};

class ProcessPool {
private:
	std::vector<Process*> processList = std::vector<Process*>();
	HANDLE hMutex = INVALID_HANDLE_VALUE;

public:
	ProcessPool() {
		hMutex = CreateMutexA(NULL, FALSE, NULL);
	}
	void add(Process* process) {
		DWORD locked = WaitForSingleObject(hMutex, 1000);
		switch (locked) {
		case WAIT_OBJECT_0:
			debug("Added process pid %d to pool\n", process->getPid());
			processList.push_back(process);
		}
		ReleaseMutex(hMutex);
	}

	Process* getByPid(unsigned int pid) {
		for (Process* p : processList) {
			if (p->getPid() == pid) return p;
		}
		return nullptr;
	}

	Process* get(int i) {
		DWORD locked = WaitForSingleObject(hMutex, 1000);
		Process* process = nullptr;
		switch (locked) {
		case WAIT_OBJECT_0:
			process = processList.at(i);
		}
		ReleaseMutex(hMutex);
		return process;
	}
	size_t count() {
		return processList.size();
	}
};