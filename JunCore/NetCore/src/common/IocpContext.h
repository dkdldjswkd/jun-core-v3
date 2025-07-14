#pragma once
#include <WinSock2.h>

enum class IO_TYPE
{
	IO_ACCEPT,
	IO_RECV,
	IO_SEND,
	IO_CONNECT
};

struct IocpContext
{
	OVERLAPPED overlapped;
	IO_TYPE io_type;

	IocpContext(IO_TYPE type) : io_type(type)
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
	}
};