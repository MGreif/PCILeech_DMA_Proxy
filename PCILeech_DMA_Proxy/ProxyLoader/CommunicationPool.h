#pragma once
#include <Windows.h>
#include <stdio.h>
#include "log.hpp"
#include <vector>
inline int g_CommunicationPartnerCounter;
void startCommunicationThread();


class CommunicationPartner {
	unsigned int pid = 0;
	char privatePipeName[32] = { 0 };
	HANDLE privatePipe = INVALID_HANDLE_VALUE;
	bool pipeConnected = false;
public:
	CommunicationPartner() {
		sprintf_s(privatePipeName, "\\\\.\\pipe\\DMA_PROXY%d", g_CommunicationPartnerCounter++);
		privatePipe = CreateNamedPipeA(privatePipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 5, 1024, 1024, 10000, NULL);
		if (privatePipe == INVALID_HANDLE_VALUE) error("Could not create private pipe\n");
		info("New communicationPartner with pipe name: %s and handle %p\n", privatePipeName, privatePipe);
	}
	~CommunicationPartner() {
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

	void setPid(unsigned int pid) {
		this->pid = pid;
	}

	unsigned int getPid() {
		return this->pid;
	}

};

class CommunicationPool {
private:
	std::vector<CommunicationPartner*> communicationList = std::vector<CommunicationPartner*>();
	HANDLE hMutex = INVALID_HANDLE_VALUE;

public:
	CommunicationPool() {
		hMutex = CreateMutexA(NULL, FALSE, NULL);
	}
	void add(CommunicationPartner* partner) {
		DWORD locked = WaitForSingleObject(hMutex, 1000);
		switch (locked) {
		case WAIT_OBJECT_0:
			communicationList.push_back(partner);
		}
		ReleaseMutex(hMutex);
	}
	CommunicationPartner* get(int i) {
		DWORD locked = WaitForSingleObject(hMutex, 1000);
		CommunicationPartner* partner = nullptr;
		switch (locked) {
		case WAIT_OBJECT_0:
			partner = communicationList.at(i);
		}
		ReleaseMutex(hMutex);
		return partner;
	}
	size_t count() {
		return communicationList.size();
	}
};