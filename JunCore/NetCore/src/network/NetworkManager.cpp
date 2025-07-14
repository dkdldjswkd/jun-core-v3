#include "NetworkManager.h"
#include "Session.h"

NetworkManager::NetworkManager()
{
}

NetworkManager::~NetworkManager() 
{
}

bool NetworkManager::StartServer(std::string const& _bind_ip, uint16 _port, int _worker_cnt)
{
	// CHECK_RETURN(0 < worker_cnt, false);

	// 1. acceptor 생성
	io_context_ = new boost::asio::io_context;
	acceptor_	= new AsyncAcceptor(*io_context_, _bind_ip, _port);

	// 2. liten
	if (!acceptor_->Listen())
	{
		// LOG_ERROR

		delete acceptor_;
		delete io_context_;
		return false;
	}

	network_threads_.reserve(_worker_cnt);

	// 3. worker thread 생성
	for (int i = 0; i < _worker_cnt; ++i)
	{
		auto _worker = new NetworkThread();
		network_threads_.emplace_back(_worker);

		_worker->Start();
	}

	// 4. Packet Hnadler 초기화
	InitPacketHandlers();

	// 5. Accept
	this->AsyncAccept();

	// 6. Accept thread 생성
	accept_thread_ = std::thread([this]() { io_context_->run(); });
	return true;
}

void NetworkManager::StopServer()
{
	// 1. acceptor 종료
	acceptor_->Close();

	// 2. acceptor ioconext stop
	io_context_->stop();

	// 3. accept thread join wait
	accept_thread_.join();

	// 4. acceptor 정리
	delete acceptor_;
	acceptor_ = nullptr;

	// 5. acceptor ioconext 정리
	delete io_context_;
	io_context_ = nullptr;

	// 6. workers ioconext stop
	for (const auto& worker : network_threads_)
	{
		worker->Stop();
	}

	// 7. workers join wait
	for (const auto& worker : network_threads_)
	{
		worker->Wait();
	}
}

void NetworkManager::AsyncAccept()
{
	uint32 min_network_thread_index = 0;

	for (int worker_index = 0; worker_index < network_threads_.size(); ++worker_index)
	{
		if (network_threads_[worker_index]->GetConnectionCount() < network_threads_[min_network_thread_index]->GetConnectionCount())
			min_network_thread_index = worker_index;
	}

	tcp::socket* accept_sock = network_threads_[min_network_thread_index]->GetSocketForAccept();

	acceptor_->AsyncAccept(*accept_sock
		// accept handler
		, [this, accept_sock, min_network_thread_index](boost::system::error_code error)
		{
			if (!error)
			{
				std::cout << "AsyncAccept callback" << std::endl;

				// 비동기 소켓 설정
				accept_sock->non_blocking(true);

				// 세션 생성
				SessionPtr new_network_session = std::make_shared<Session>(std::move(*accept_sock), this);

				// callback bind
				new_network_session->disconnect_handler_ = [this](SessionPtr session_ptr) {
					this->OnSessionClose(session_ptr);
				};

				network_threads_[min_network_thread_index]->AddNewSession(new_network_session);
				new_network_session->Start();

				// 사용자 재정의 callback
				OnAccept(new_network_session);
			}

			if (!acceptor_->IsClosed())
			{
				this->AsyncAccept();
			}
		}
	);
}

void NetworkManager::OnAccept(SessionPtr session_ptr)
{
}

void NetworkManager::OnSessionClose(SessionPtr session_ptr)
{
}

bool NetworkManager::StartClient(int worker_cnt)
{
	// Packet Hnadler 초기화
	InitPacketHandlers();

	network_threads_.reserve(worker_cnt);

	for (int i = 0; i < worker_cnt; ++i)
	{
		auto worker = new NetworkThread();
		network_threads_.emplace_back(worker);

		worker->Start();
	}

	return true;
}

void NetworkManager::Connect(const tcp::endpoint& endpoint)
{
	// server session의 경우 server_session_flag = true 등 처리 필요?
	//		ㄴ server session인걸 알아야하는 경우가 있나?

	// server session의 경우 client session과 worker thread 분리 필요?
	//		ㄴ client session의 메시지가 server session의 메시지 처리에 영향을 주지 않게 하기위해서 분리 필요
 
	////////////////////////////////////////////////////////////////////////

	uint32 min_network_thread_index = 0;

	for (int worker_index = 0; worker_index < network_threads_.size(); ++worker_index)
	{
		if (network_threads_[worker_index]->GetConnectionCount() < network_threads_[min_network_thread_index]->GetConnectionCount())
			min_network_thread_index = worker_index;
	}

	tcp::socket* connect_sock = network_threads_[min_network_thread_index]->GetSocketForConnect();

	connect_sock->async_connect(endpoint,
		[this, connect_sock, min_network_thread_index](boost::system::error_code ec) {
			if (!ec)
			{
				 //비동기 소켓으로 설정
				 connect_sock->non_blocking(true);

				// todo 고려
				SessionPtr new_network_session = std::make_shared<Session>(std::move(*connect_sock), this);
				network_threads_[min_network_thread_index]->AddNewSession(new_network_session);
				new_network_session->Start();

				// 사용자 제정의 callback
				OnConnect(new_network_session);
			}
			else
			{
				// Timer에 reconnect 시도 등록
				// ...
			}
		}
	);
}

void NetworkManager::OnConnect(SessionPtr session_ptr)
{
}