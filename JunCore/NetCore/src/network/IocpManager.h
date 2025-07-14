#pragma once
#include <WinSock2.h>
#include <vector>
#include <thread>
#include <atomic>
#include "../common/IocpContext.h"

class IocpManager
{
public:
	IocpManager();
	~IocpManager();

	bool Start(int worker_thread_count);
	void Stop();

	HANDLE GetIocpHandle() const { return iocp_handle_; }

private:
	void WorkerThreadProc();

	HANDLE iocp_handle_;
	std::vector<std::thread> worker_threads_;
	std::atomic<bool> is_running_;
};