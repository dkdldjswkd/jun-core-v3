#include "IocpManager.h"
#include <iostream>
#include <thread>

IocpManager::IocpManager()
	: iocp_handle_(INVALID_HANDLE_VALUE),
	  is_running_(false)
{
}

IocpManager::~IocpManager()
{
	Stop();
}

bool IocpManager::Start(int worker_thread_count)
{
	if (is_running_)
		return true;

	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_handle_ == INVALID_HANDLE_VALUE)
	{
		std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
		return false;
	}

	is_running_ = true;

	if (worker_thread_count == 0)
	{
		worker_thread_count = std::thread::hardware_concurrency();
		if (worker_thread_count == 0)
			worker_thread_count = 4;
	}

	std::cout << "Creating " << worker_thread_count << " worker threads..." << std::endl;

	for (int i = 0; i < worker_thread_count; ++i)
	{
		worker_threads_.emplace_back([this]() {
			WorkerThreadProc();
		});
	}

	std::cout << "IOCP Manager started with " << worker_thread_count << " worker threads." << std::endl;
	return true;
}

void IocpManager::Stop()
{
	if (!is_running_)
		return;

	is_running_ = false;

	for (size_t i = 0; i < worker_threads_.size(); ++i)
	{
		PostQueuedCompletionStatus(iocp_handle_, 0, 0, NULL);
	}

	for (auto& thread : worker_threads_)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}
	worker_threads_.clear();

	if (iocp_handle_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(iocp_handle_);
		iocp_handle_ = INVALID_HANDLE_VALUE;
	}
	std::cout << "IOCP Manager stopped." << std::endl;
}

void IocpManager::WorkerThreadProc()
{
	DWORD bytesTransferred = 0;
	ULONG_PTR completionKey = 0;
	LPOVERLAPPED lpOverlapped = NULL;

	while (is_running_)
	{
		BOOL ret = GetQueuedCompletionStatus(
			iocp_handle_,
			&bytesTransferred,
			&completionKey,
			&lpOverlapped,
			INFINITE
		);

		if (!ret || lpOverlapped == nullptr)
		{
			if (!is_running_)
				break;

			std::cerr << "GetQueuedCompletionStatus failed or received shutdown signal: " << GetLastError() << std::endl;
			continue;
		}

		IocpContext* context = reinterpret_cast<IocpContext*>(lpOverlapped);
		if (context == nullptr)
		{
			std::cerr << "Received null overlapped pointer." << std::endl;
			continue;
		}

		std::cout << "IOCP event received: Type " << static_cast<int>(context->io_type) << ", Bytes: " << bytesTransferred << std::endl;
	}
	std::cout << "Worker thread shutting down." << std::endl;
}