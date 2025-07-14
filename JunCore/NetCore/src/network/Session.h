#pragma once
#include "../common/define.h"
#include <vector>
#include <queue>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "../common/MessageBuffer.h"
#include "../packet/PacketBuffer.h"
#include "../common/IocpContext.h"

#define READ_BLOCK_SIZE 4096

class NetworkThread;
class ServerManager;
class Session;
using SessionPtr = std::shared_ptr<Session>;
using SessionPtrVec = std::vector<SessionPtr>;

class Session : public std::enable_shared_from_this<Session>
{
	friend NetworkThread;

public:
	Session(SOCKET sock);
	~Session();

public:
	std::function<void(std::shared_ptr<Session>)> OnCloseCallback = nullptr;
	std::function<void(SessionPtr, int32, const std::vector<char>&)> OnPacketReceiveCallback = nullptr;

public:
	template <typename T>
	void SendPacket(int32 _packet_id, const T& _packet)
	{
		const int32 _payload_size = static_cast<int32>(_packet.ByteSizeLong());

		PacketBufferPtr _packet_buffer_ptr = std::make_shared<PacketBuffer>();
		if (_packet_buffer_ptr->GetFreeSize() < static_cast<size_t>(_payload_size))
		{
			return;
		}

		_packet.SerializeToArray(_packet_buffer_ptr->GetWritePos(), _payload_size);
		_packet_buffer_ptr->MoveWp(_payload_size);
		_packet_buffer_ptr->SetHeader(_packet_id);

		{
			std::lock_guard<std::mutex> lock(send_queue_lock_);
			send_queue_.push(_packet_buffer_ptr);
		}
	}

public:
	std::string GetRemoteIpAddress() const;
	uint16 GetRemotePort() const;

private:
	void Start();
	bool Update();
	void CloseSocket();

public:
	bool IsOpen() const;

private:
	void AsyncRecv();
	void HandleRecv(DWORD transferredBytes);
	MessageBuffer& GetRecvBuffer();

private:
	bool ProcessSendQueue();
	void AsyncSend();
	void HandleSend(DWORD transferredBytes);

private:
	SOCKET socket_;
	MessageBuffer recv_buffer_;
	std::mutex send_queue_lock_;
	std::queue<PacketBufferPtr> send_queue_;

	SOCKADDR_IN remote_address_info_;
	std::atomic<bool> closed_;

	IocpContext recv_context_;
	IocpContext send_context_;
	WSABUF recv_wsa_buf_;
	WSABUF send_wsa_buf_;
};