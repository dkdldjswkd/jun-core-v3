#include "Session.h"
#include <functional>
#include <iostream>
#include <MSWSock.h>

#pragma comment(lib, "ws2_32.lib")

Session::Session(SOCKET sock)
	: socket_(sock),
	  remote_address_info_({}),
	  closed_(false),
	  recv_context_(IO_TYPE::IO_RECV),
	  send_context_(IO_TYPE::IO_SEND)
{
	recv_wsa_buf_.buf = reinterpret_cast<char*>(recv_buffer_.GetWritePointer());
	recv_wsa_buf_.len = READ_BLOCK_SIZE;

	send_wsa_buf_.buf = nullptr;
	send_wsa_buf_.len = 0;

	int namelen = sizeof(remote_address_info_);
	getpeername(socket_, (SOCKADDR*)&remote_address_info_, &namelen);
}

Session::~Session()
{
	CloseSocket();
}

void Session::Start()
{
	AsyncRecv();
}

void Session::AsyncRecv()
{
	if (!IsOpen())
		return;

	recv_buffer_.Normalize();
	recv_buffer_.EnsureFreeSpace();
	recv_wsa_buf_.buf = reinterpret_cast<char*>(recv_buffer_.GetWritePointer());
	recv_wsa_buf_.len = static_cast<ULONG>(recv_buffer_.GetRemainingSpace());

	DWORD flags = 0;
	DWORD bytesReceived = 0;

	int ret = WSARecv(socket_, &recv_wsa_buf_, 1, &bytesReceived, &flags, &recv_context_.overlapped, NULL);
	if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING))
	{
		std::cerr << "WSARecv failed: " << WSAGetLastError() << std::endl;
		CloseSocket();
	}
}

void Session::HandleRecv(DWORD transferredBytes)
{
	if (!IsOpen())
		return;

	if (transferredBytes == 0)
	{
		CloseSocket();
		return;
	}

	recv_buffer_.WriteCompleted(transferredBytes);

	for (;;)
	{
		const size_t _recv_size = recv_buffer_.GetActiveSize();
		const int32 _recv_size_int32 = static_cast<int32>(_recv_size);

		if (_recv_size < HEADER_SIZE)
		{
			recv_buffer_.Normalize();
			break;
		}

		PacketHeader* _p_packet_header = (PacketHeader*)recv_buffer_.GetReadPointer();

		if (_recv_size < (HEADER_SIZE + _p_packet_header->len))
		{
			recv_buffer_.Normalize();
			break;
		}

		std::vector<char> _serialized_packet(_p_packet_header->len);
		std::memcpy(_serialized_packet.data(), recv_buffer_.GetReadPointer() + HEADER_SIZE, _p_packet_header->len);
		recv_buffer_.ReadCompleted(HEADER_SIZE + _p_packet_header->len);

		if (OnPacketReceiveCallback)
		{
			OnPacketReceiveCallback(shared_from_this(), _p_packet_header->pid, _serialized_packet);
		}
	}

	AsyncRecv();
}

MessageBuffer& Session::GetRecvBuffer()
{
	return recv_buffer_;
}

bool Session::Update()
{
	if (closed_)
		return false;

	while (ProcessSendQueue());
	return true;
}

bool Session::ProcessSendQueue()
{
	PacketBufferPtr _send_msg_ptr = nullptr;

	{
		std::lock_guard<std::mutex> _lock(send_queue_lock_);

		if (send_queue_.empty())
			return false;

		_send_msg_ptr = send_queue_.front();
	}

	send_wsa_buf_.buf = reinterpret_cast<char*>(_send_msg_ptr->GetPacketPos());
	send_wsa_buf_.len = static_cast<ULONG>(_send_msg_ptr->GetPacketSize());

	DWORD bytesSent = 0;
	int ret = WSASend(socket_, &send_wsa_buf_, 1, &bytesSent, 0, &send_context_.overlapped, NULL);
	if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING))
	{
		std::cerr << "WSASend failed: " << WSAGetLastError() << std::endl;
		CloseSocket();
		return false;
	}

	return true;
}

void Session::AsyncSend()
{
	ProcessSendQueue();
}

void Session::HandleSend(DWORD transferredBytes)
{
	if (!IsOpen())
		return;

	{
		std::lock_guard<std::mutex> _lock(send_queue_lock_);

		if (!send_queue_.empty())
		{
			send_queue_.pop();
		}
	}

	ProcessSendQueue();
}

void Session::CloseSocket()
{
	if (closed_)
		return;

	closed_ = true;

	if (socket_ != INVALID_SOCKET)
	{
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
	}

	if (OnCloseCallback)
	{
		OnCloseCallback(shared_from_this());
	}
}

bool Session::IsOpen() const
{
	return !closed_;
}

std::string Session::GetRemoteIpAddress() const
{
	char ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &remote_address_info_.sin_addr, ip_str, INET_ADDRSTRLEN);
	return std::string(ip_str);
}

uint16 Session::GetRemotePort() const
{
	return ntohs(remote_address_info_.sin_port);
}