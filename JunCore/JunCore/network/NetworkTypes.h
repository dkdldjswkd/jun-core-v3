#pragma once
#include "NetworkEngine.h"
#include "NetworkPolicy.h"

//------------------------------
// 편의를 위한 팩토리 함수들
//------------------------------
namespace NetworkFactory 
{
	// 서버 생성 함수
	template<typename ServerClass>
	std::unique_ptr<ServerClass> CreateServer()
	{
		return std::make_unique<ServerClass>();
	}

	// 클라이언트 생성 함수  
	template<typename ClientClass>
	std::unique_ptr<ClientClass> CreateClient()
	{
		return std::make_unique<ClientClass>();
	}

	// 정책 기반 생성 함수
	template<typename Policy>
	std::unique_ptr<NetworkEngine<Policy>> Create()
	{
		return std::make_unique<NetworkEngine<Policy>>();
	}
}

//------------------------------
// 컴파일 타임 정책 검증
//------------------------------
namespace PolicyTraits
{
	// 서버 정책인지 확인
	template<typename Policy>
	constexpr bool IsServerPolicy() { return Policy::IsServer; }
	
	// 클라이언트 정책인지 확인
	template<typename Policy>
	constexpr bool IsClientPolicy() { return !Policy::IsServer; }
	
	// 정책 이름 반환
	template<typename Policy>
	constexpr const char* GetPolicyName() { return Policy::PolicyName; }
}

//------------------------------
// 사용 예시 (주석으로 설명)
//------------------------------
/*
// 기존 방식
class EchoServer : public NetworkEngine<ServerPolicy> 
{
    // 구현...
};

// 새로운 방식 (Phase 3 완료 후)
class EchoServer : public NetServerEngine 
{
    // 동일한 구현으로 작동
};

// 또는 더 명시적으로
class EchoServer : public NetworkEngine<ServerPolicy>
{
    // 정책 기반 구현
};

// 팩토리를 사용한 생성
auto server = NetworkFactory::CreateServer<EchoServer>();
auto client = NetworkFactory::CreateClient<EchoClient>();

// 정책 기반 생성
auto serverEngine = NetworkFactory::Create<ServerPolicy>();
auto clientEngine = NetworkFactory::Create<ClientPolicy>();
*/