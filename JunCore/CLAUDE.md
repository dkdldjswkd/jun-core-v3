# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Communication Language
- **Always communicate in Korean (한글)** when interacting with the user
- Use Korean for explanations, comments, and all user-facing communication
- Technical terms may be used in English when appropriate, but explanations should be in Korean

## Critical Thinking and Counter-Proposals
- **Do not blindly accept all user requests** - Think critically about proposed solutions
- **Actively provide counter-proposals** when better alternatives exist
- **Consider trade-offs** including performance, security, maintainability, and complexity
- **Question assumptions** - Ask "why" and "is this the right approach?"

### Examples of Critical Analysis:
- **Performance Impact**: "이 방법은 확장성이 좋지만, 매 패킷마다 암호화 오버헤드가 있습니다. 실제 TPS 목표가 얼마나 되나요?"
- **Security vs Usability**: "완전한 암호화는 보안성이 높지만, 디버깅과 모니터링이 어려워집니다. 로그 레벨별 선택적 암호화는 어떨까요?"
- **Complexity Trade-offs**: "이 구조는 유연하지만 복잡합니다. 현재 요구사항에는 더 간단한 방법이 적합할 수도 있습니다."
- **Alternative Approaches**: "제안하신 방법 외에도 X, Y 방식도 고려해볼 만합니다. 각각의 장단점은..."

### When to Push Back:
- Over-engineering for current requirements
- Performance-critical paths with unnecessary complexity  
- Security measures that don't match actual threat model
- Solutions that break established patterns without clear benefit

## Build Commands

### MSBuild Path (WSL Environment)
```bash
# MSBuild 실행 파일 경로 (매번 찾지 말고 이 경로 사용)
MSBUILD_PATH="/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"

# 사용 예시
"$MSBUILD_PATH" JunCore.sln /p:Configuration=Debug /p:Platform=x64
```

### Building the Solution
```bash
# Build entire solution (WSL에서 실행)
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" JunCore.sln /p:Configuration=Debug /p:Platform=x64
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" JunCore.sln /p:Configuration=Release /p:Platform=x64

# Build with error-only output (빠른 빌드 확인)
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" JunCore.sln /p:Configuration=Debug /p:Platform=x64 /clp:ErrorsOnly

# Build specific project
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" JunCore/JunCore.vcxproj /p:Configuration=Debug /p:Platform=x64
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" EchoServer/EchoServer.vcxproj /p:Configuration=Debug /p:Platform=x64
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" EchoClient/EchoClient.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Protobuf Code Generation
```bash
# Generate C++ code from all .proto files in the project
./generate_protobuf.bat
```

This script automatically searches for all `.proto` files in the project and generates corresponding `.pb.h` and `.pb.cc` files using the vcpkg-installed protoc compiler.

### Running Tests and Examples
```bash
# Run the test application (includes Protobuf and AES examples)
./x64/Debug/Test.exe

# Run Echo Server (requires ServerConfig.ini)
./x64/Debug/EchoServer.exe

# Run Echo Client  
./x64/Debug/EchoClient.exe
```

## Project Architecture

### New Directory Structure (2025-08 Reorganization)
JunCore has been reorganized using **Boost/STL-style** directory structure for better maintainability:

#### JunCore Structure
```
JunCore/
├── network/          # 네트워크 관련 (boost::asio 스타일)
│   ├── server.h/.cpp    (기존 NetServer)
│   ├── client.h/.cpp    (기존 NetClient) 
│   └── Session.h/.cpp   (세션 관리)
├── buffer/           # 버퍼/컨테이너 (std::container 스타일)
│   └── packet.h/.cpp    (기존 PacketBuffer)
├── protocol/         # 프로토콜 정의 (boost::protocol 스타일)
│   └── message.h        (기존 protocol.h)
└── core/            # 핵심 유틸리티
    └── base.h           (fnv1a, make_protocol_code 등)
```

**Important**: 파일명은 변경되었지만 클래스명은 그대로 유지 (NetServer, NetClient, PacketBuffer 등)

### Component Overview
This is a Windows C++ IOCP-based game server framework with the following components:

- **JunCore**: Core networking library providing IOCP-based server/client functionality
- **JunCommon**: Common utilities including lock-free data structures, logging, profiling, and system monitoring
- **EchoServer/EchoClient**: Example echo server and client implementations 
- **Test**: Protobuf integration test project

## 🚀 현재 네트워크 아키텍처 (2025-01 최신)

### 핵심 아키텍처: **공유 IOCP + Engine별 패킷 처리 모델**

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│  EchoServer │  GameServer  │  EchoClient  │ GameClient  │
├─────────────────────────────────────────────────────────┤
│            Engine Layer (NetBase 기반)                 │
│     Server (다중 세션)     │     Client (단일 세션)     │
├─────────────────────────────────────────────────────────┤
│              Network Layer (IOCPManager)                │
│         순수 I/O 이벤트 처리 + 패킷 조립               │
├─────────────────────────────────────────────────────────┤
│                    Session Layer                        │
│         개별 연결 상태 + shared_ptr 생명주기 관리       │
└─────────────────────────────────────────────────────────┘
```

### 🎯 **Engine별 패킷 핸들러 시스템** (2025-01 신규)

#### 전역 핸들러 완전 제거
- ❌ **기존**: `g_direct_packet_handler` (전역 상태, 멀티 엔진 충돌)
- ✅ **현재**: **Engine별 독립 패킷 핸들러 맵** (완전 분리, 타입 안전)

#### 새로운 패킷 처리 흐름
```cpp
[IOCPManager] 패킷 조립 → [NetBase::OnPacketReceived] 엔진별 디스패치 
→ [RegisteredHandler] 사용자 비즈니스 로직
```

#### 사용자 코드 패턴
```cpp
// EchoServer.h - 사용자는 이것만 구현하면 됨!
class EchoServer : public Server {
protected:
    void RegisterPacketHandlers() override {  // 순수 가상함수 강제 구현
        RegisterPacketHandler<echo::EchoRequest>([this](Session& session, const echo::EchoRequest& req) {
            HandleEchoRequest(session, req);  // 비즈니스 로직
        });
    }
    
    void HandleEchoRequest(Session& session, const echo::EchoRequest& req) {
        echo::EchoResponse response;
        response.set_message(req.message());
        SendPacket(session, response);
    }
};
```

### 🔧 **Two-Phase Construction 패턴** (필수)

#### 표준 초기화 패턴
```cpp
// Phase 1: 객체 생성 및 초기화
EchoServer server;
server.Initialize();  // 패킷 핸들러 등록 (필수!)

// Phase 2: 리소스 연결 및 시작
auto iocpManager = IOCPManager::Create().WithWorkerCount(4).Build();
server.AttachIOCPManager(iocpManager);
server.StartServer("127.0.0.1", 9090);  // 초기화 검사 통과 후 시작
```

#### 안전성 보장
- **컴파일 타임**: `RegisterPacketHandlers()` 구현 안 하면 컴파일 에러
- **런타임**: `Initialize()` 호출 안 하면 `StartServer()`/`Connect()` 실패
- **명확한 에러**: "Must call Initialize() before StartServer()!" 메시지

## 🏗️ 클래스별 책임과 역할 정의

### **NetBase** (추상 엔진 기반 클래스)
**핵심 책임**:
- 🔧 IOCP Manager 연결 인터페이스 (`AttachIOCPManager`, `DetachIOCPManager`)
- 📦 Engine별 패킷 핸들러 시스템 관리
- 🚀 Two-Phase Construction 패턴 구현 (`Initialize()`, `IsInitialized()`)
- 📤 공통 송신 인터페이스 (`SendPacket<T>` 템플릿)

**설계 원칙**:
- **추상 클래스**: 순수 가상함수 `RegisterPacketHandlers()` 강제 구현
- **타입 안전**: 템플릿 기반 패킷 핸들러 등록/실행
- **Engine 독립성**: 각 엔진별 독립된 패킷 핸들러 맵 보유
- **Zero Overhead**: 인라인 함수와 템플릿으로 런타임 오버헤드 없음

**금지 사항**:
- ❌ 직접 인스턴스화 불가 (추상 클래스)
- ❌ I/O 로직 직접 처리 (IOCPManager에게 위임)

### **IOCPManager** (I/O 이벤트 처리 전담)
**핵심 책임**:
- ⚡ **순수 I/O 이벤트 처리**: `GetQueuedCompletionStatus` 루프
- 📦 **패킷 조립**: `UnifiedPacketHeader` 기반 완전한 패킷 구성
- 🔄 **비동기 I/O 등록**: `PostAsyncSend`, `PostAsyncReceive`
- 🎯 **Engine으로 위임**: `NetBase::OnPacketReceived` 호출

**설계 특징**:
- **Builder 패턴**: `IOCPManager::Create().WithWorkerCount(4).Build()`
- **RAII 관리**: 자동 리소스 정리 (`HANDLE` 관리)
- **복사/이동 금지**: 리소스 안전성 보장
- **Single Responsibility**: 오직 네트워크 I/O만 담당

**처리하지 않는 것**:
- ❌ 비즈니스 로직 (Engine에게 위임)
- ❌ 패킷 해석 (바이트 스트림만 조립)
- ❌ 세션 생명주기 관리 (Session 자체 관리)

### **Server** (서버 엔진)
**핵심 책임**:
- 🎧 **Accept 스레드 관리**: 클라이언트 연결 수락
- 👥 **다중 세션 관리**: Lock-Free 세션 풀 (`LFStack<DWORD>`)
- 📞 **연결 이벤트 처리**: `OnClientJoin`, `OnClientLeave`
- 🛡️ **연결 요청 검증**: `OnConnectionRequest` (IP/Port 필터링)

**아키텍처 특징**:
- **세션 풀링**: 미리 할당된 세션 배열과 인덱스 스택
- **Lock-Free**: 세션 인덱스 할당/해제에서 뮤텍스 없음
- **자동 세션 정리**: IOCount 기반 자동 세션 반환
- **확장성**: 최대 세션 수 동적 설정 가능

### **Client** (클라이언트 엔진) 
**핵심 책임**:
- 🔗 **단일 서버 연결**: 하나의 서버에 대한 연결 관리
- 📡 **연결 상태 관리**: `Connect`, `Disconnect`, `IsConnected`
- 🎯 **단일 세션 최적화**: 클라이언트 특화 세션 관리
- 📤 **편의 함수 제공**: 문자열/바이너리 직접 전송 지원

**설계 특징**:
- **Server와 통일**: 동일한 세션 풀 패턴 사용 (1개 세션으로 최적화)
- **상태 자동 관리**: 연결/해제 시 자동 세션 정리
- **편의성**: 개발자 친화적인 전송 API 제공

### **Session** (연결 상태 관리)
**핵심 책임**:
- 🔐 **생명주기 관리**: IOCount 기반 자동 해제 시스템
- 📊 **연결 상태**: 소켓, 버퍼, 타임아웃 관리  
- 🔄 **I/O 버퍼 관리**: RingBuffer 기반 송수신 버퍼
- 🏷️ **Engine 연결**: 자신이 속한 Engine 포인터 보유

**🚀 현재 세션 관리 - shared_ptr 기반**:
```cpp
// Session 소멸자에서 자동 정리
Session::~Session() {
    if (engine_ && owner_user_) {
        engine_->OnSessionDisconnect(owner_user_);
    }
    Release();  // 소켓 정리 및 리소스 해제
}
```

**설계 원리**:
- **shared_ptr**: 참조 카운팅으로 자동 생명주기 관리
- **OverlappedEx**: I/O 완료까지 세션 참조 보장
- **RAII**: 소멸자에서 자동 리소스 정리
- **Thread-Safe**: shared_ptr 내장 원자적 연산 활용

### **역할 분리 원칙**

#### **Network Layer** (IOCPManager)
- ✅ 소켓 I/O 이벤트 처리
- ✅ 패킷 바이트 스트림 조립
- ✅ 비동기 I/O 등록/완료
- ❌ 패킷 내용 해석 금지
- ❌ 비즈니스 로직 처리 금지

#### **Engine Layer** (NetBase, Server, Client)  
- ✅ 패킷 핸들러 등록/실행
- ✅ 비즈니스 로직 처리
- ✅ 세션 생명주기 이벤트
- ❌ 직접 I/O 처리 금지
- ❌ 소켓 레벨 조작 금지

#### **Session Layer** (Session)
- ✅ 개별 연결 상태 관리
- ✅ 자동 생명주기 관리
- ✅ I/O 버퍼 관리
- ❌ 패킷 해석 금지
- ❌ 비즈니스 로직 금지

## 📋 개발 정책 및 규칙 (2025-01)

### 🚨 **Two-Phase Construction 패턴 필수 적용**

#### 모든 Engine은 반드시 이 순서를 따를 것
```cpp
// ✅ 올바른 사용법
Engine engine;
engine.Initialize();        // Phase 1: 패킷 핸들러 등록
engine.AttachIOCPManager(); // Phase 2: 리소스 연결
engine.Start();             // Phase 3: 실제 시작

// ❌ 잘못된 사용법 - 런타임 에러 발생
Engine engine;
engine.Start();  // "Must call Initialize() before Start()!" 에러
```

#### 사용자 구현 강제사항
- **필수 구현**: `RegisterPacketHandlers()` 순수 가상함수
- **컴파일 에러**: 구현하지 않으면 빌드 실패
- **런타임 안전**: 초기화 없이 시작 시도하면 명확한 에러 메시지


### 🏗️ **신규 엔진 생성 시 필수 체크리스트**

#### 1. NetBase 상속 및 필수 구현
```cpp
class NewGameServer : public Server {
protected:
    void RegisterPacketHandlers() override {  // 필수!
        RegisterPacketHandler<LoginRequest>([this](Session& s, const LoginRequest& req) {
            HandleLogin(s, req);
        });
        RegisterPacketHandler<GameCommand>([this](Session& s, const GameCommand& cmd) {
            HandleGameCommand(s, cmd);  
        });
    }
};
```

#### 2. EchoServer/EchoClient에서 검증 필수
- **변경된 코어 적용**: 새로운 네트워크 아키텍처 사용
- **컴파일 확인**: 모든 프로젝트 오류 없이 빌드
- **런타임 테스트**: 실제 서버-클라이언트 통신 확인
- **브레이크포인트 검증**: 새 코드 경로가 실제 실행되는지 확인
- **성능 비교**: 기존 대비 성능 저하 없는지 확인

#### 3. 마이그레이션 단계별 접근
- **Phase 1**: 새 엔진 클래스 작성 및 컴파일 확인
- **Phase 2**: EchoServer/Client를 새 구조로 업데이트  
- **Phase 3**: 기존 기능 동작 확인 (패킷 송수신)
- **Phase 4**: 성능 및 안정성 검증

### ⚡ **성능 최적화 원칙**

#### Zero Runtime Overhead 유지
- **템플릿 기반**: 모든 공통화가 컴파일 타임에 해결됨
- **인라인 함수**: 함수 호출 오버헤드 제거
- **가상함수 최소화**: 성능 중요 경로에서는 피할 것
- **캐시 친화적**: `alignas(64)` 적용으로 메모리 레이아웃 최적화

#### Lock-Free 원칙 준수
- **원자적 연산**: `InterlockedXXX` 함수 사용
- **뮤텍스 금지**: 고성능 경로에서 뮤텍스 사용 금지  
- **메모리 배리어**: 필요시 적절한 메모리 배리어 사용
- **ABA 방지**: 메모리 스탬핑 기법 활용

### 🎯 **패킷 핸들러 설계 원칙**

#### 타입 안전성 보장
```cpp
// ✅ 권장: 강타입 패킷 핸들러
RegisterPacketHandler<LoginRequest>([this](Session& session, const LoginRequest& req) {
    // 컴파일 타임에 타입 체크됨
    HandleLogin(session, req);  
});

// ❌ 금지: 바이트 배열 직접 처리
RegisterRawHandler(PACKET_ID_LOGIN, [](Session& session, const std::vector<char>& data) {
    // 런타임 에러 가능성 높음
});
```

#### Protobuf 패킷 ID 자동 생성
- **FNV-1a 해시**: Protobuf 타입명으로 자동 패킷 ID 생성
- **충돌 방지**: 패키지명 포함으로 네임스페이스 분리  
- **일관성**: 서버/클라이언트 간 동일한 ID 보장

### 🔧 **개발 도구 및 빌드 정책**

#### 필수 빌드 명령어
```bash
# 에러만 표시하는 빠른 빌드 확인
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" JunCore.sln /p:Configuration=Debug /p:Platform=x64 /clp:ErrorsOnly

# 작업 완료 후 반드시 전체 빌드 확인
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" JunCore.sln /p:Configuration=Debug /p:Platform=x64
```

#### Zero Warnings 정책
- **모든 경고 해결**: 컴파일 경고 0개 유지  
- **경고 무시 금지**: 경고는 반드시 근본적으로 해결
- **새 파일 생성 시**: UTF-8 BOM 인코딩 사용

#### 프로젝트 파일 관리
- **신규 파일 생성 시**: 반드시 해당 `.vcxproj` 파일에 추가
- **필터 파일 업데이트**: `.vcxproj.filters`도 함께 업데이트
- **빌드 전 확인**: 새 파일이 프로젝트에 포함되었는지 검증

### 📚 **문서화 규칙**

#### CLAUDE.md 업데이트 의무
- **아키텍처 변경 시**: 즉시 이 문서 업데이트
- **새 정책 추가 시**: 명확한 예시와 함께 문서화
- **성능 최적화 시**: 설계 원칙과 벤치마크 결과 기록

#### 주석 정책
- **이모지 금지**: 코드 주석에 이모지 사용 금지
- **한글 주석**: 비즈니스 로직은 한글로 설명  
- **영문 주석**: 시스템 아키텍처는 영문으로 설명

### Key Architecture Patterns

#### Session Management
- Sessions are managed through a lock-free stack (`LFStack<DWORD>`) for index allocation
- Each session has reference counting via `ioCount` for safe concurrent access
- Session lifecycle: Accept → OnClientJoin → OnRecv → OnClientLeave → Release

#### Memory Management
- Uses object pools (`LFObjectPool`, `ObjectPool`) for high-performance memory allocation
- PacketBuffer provides automatic memory management for network packets
- Lock-free data structures avoid mutex overhead in high-concurrency scenarios

#### Network Protocol
- Supports both LAN and NET protocols with different header formats
- LAN: Simple 2-byte length header
- NET: 5-byte header with protocol code, length, encryption key, and checksum
- Protocol configuration loaded via Parser from config files

#### Threading Model
- IOCP-based with configurable worker thread count
- Separate threads for: Accept, Worker (I/O completion), Timeout monitoring
- Worker threads handle all I/O completion events (recv/send/disconnect)

### Core Classes

#### NetServer (`JunCore/NetServer.h`)
Main server class providing:
- Pure virtual methods for user implementation: `OnConnectionRequest`, `OnClientJoin`, `OnRecv`, `OnClientLeave`, `OnServerStop`
- Session management and I/O completion handling
- Built-in performance monitoring (TPS counters)

#### Session (`JunCore/Session.h`)
Represents a client connection with:
- Reference counting for safe concurrent access
- Send/receive buffers and overlapped I/O structures
- Connection state and timeout tracking

#### PacketBuffer (`JunCore/PacketBuffer.h`)
Network packet wrapper providing:
- Automatic header management for LAN/NET protocols
- Reference counting for memory safety via thread-local object pool
- Serialization methods for various data types
- Throws `PacketException` for buffer overruns (GET_ERROR/PUT_ERROR types)
- Max payload size: 8000 bytes, max header size: 10 bytes

### JunCommon Library Structure

JunCommon is organized using **Boost-style** folder structure for better maintainability and code organization:

#### Core Components (`JunCommon/`)
- **`core/`**: Essential definitions, macros, and constants
  - `base.h`: Memory guards, crash macros, lock-free constants
- **`container/`**: Data structures and containers
  - `LFStack.h`: Lock-free stack with ABA problem prevention
  - `LFQueue.h`: Lock-free queue implementation  
  - `RingBuffer.h/.cpp`: Circular buffer for I/O operations
- **`pool/`**: Object pooling and memory management
  - `LFObjectPool.h`: Lock-free object pool for high-performance allocation
  - `LFObjectPoolTLS.h`: Thread-local storage object pool
  - `ObjectPool.h`: Standard object pool implementation
- **`log/`**: All logging systems
  - `Logger.h/.cpp`: Thread-safe logging with multiple levels
  - `MemoryLogger.h/.cpp`: Memory usage tracking and profiling
- **`timer/`**: Performance measurement and system monitoring
  - `Profiler.h/.cpp`: Code profiling and performance analysis
  - `PerformanceCounter.h/.cpp`: System performance monitoring via PDH
  - `MachineCpuMonitor.h/.cpp`: System-wide CPU usage monitoring
  - `ProcessCpuMonitor.h/.cpp`: Process-specific CPU monitoring
- **`network/`**: Network-related utilities
  - `ProtocolBuffer.h/.cpp`: Network packet management
- **`synchronization/`**: Thread synchronization primitives
  - `RecursiveLock.h/.cpp`: Recursive mutex implementation
- **`system/`**: System diagnostics and debugging
  - `CrashDump.h/.cpp`: Automatic crash dump generation
- **`algorithm/`**: Algorithms and utility functions
  - `Parser.h/.cpp`: Configuration file parsing
  - `StringUtils.h/.cpp`: String manipulation utilities
- **`crypto/`**: Cryptographic functions and utilities
  - `AES128.h/.cpp`: High-performance AES-128 encryption with CBC and ECB modes
  - `RSA2048.h/.cpp`: RSA 2048-bit asymmetric encryption

#### Memory Management Philosophy
- Uses object pools for high-performance memory allocation
- Lock-free data structures avoid mutex overhead in high-concurrency scenarios
- Memory stamping technique prevents ABA problems in lock-free implementations

### Development Notes

#### Adding New Protocol Messages (2025-01 신규 방식)
1. **Protobuf 메시지 정의**:
   ```protobuf
   // game_messages.proto
   syntax = "proto3";
   package game;
   
   message LoginRequest {
       string username = 1;
       string password = 2;
   }
   
   message LoginResponse {
       bool success = 1;
       string message = 2;
   }
   ```

2. **Protobuf 코드 생성**:
   ```bash
   ./generate_protobuf.bat  # C++ 코드 자동 생성
   ```

3. **패킷 핸들러 등록** (Engine에서):
   ```cpp
   // GameServer.h
   class GameServer : public Server {
   protected:
       void RegisterPacketHandlers() override {
           RegisterPacketHandler<game::LoginRequest>([this](Session& session, const game::LoginRequest& req) {
               HandleLogin(session, req);  // 비즈니스 로직 구현
           });
       }
       
   private:
       void HandleLogin(Session& session, const game::LoginRequest& req) {
           game::LoginResponse response;
           response.set_success(ValidateLogin(req.username(), req.password()));
           SendPacket(session, response);
       }
   };
   ```

4. **패킷 ID 자동 생성**: FNV-1a 해시로 `game.LoginRequest` → 패킷 ID 자동 변환
5. **타입 안전성**: 컴파일 타임에 타입 체크 및 자동 직렬화/역직렬화

#### Configuration System
- Uses `Parser` class to load configuration from text files (e.g., `ServerConfig.ini`)
- Supports section-based configuration with curly braces syntax:
  ```ini
  SectionName
  {
      key = value
      stringKey = "string value"
  }
  ```
- Access values via `parser.GetValue("section", "key", &variable)`
- Server configurations loaded via constructor parameters (systemFile, serverSection)

#### Error Handling
- Uses Windows structured exception handling
- `CRASH()` macro for intentional crash dumps during debugging
- Session disconnection handling via `CancelIoEx` and reference counting

#### Performance Monitoring
- Built-in TPS (Transactions Per Second) counters for accepts, sends, receives
- Session count tracking
- Call `UpdateTPS()` regularly to refresh performance metrics

#### Include Path Examples
When using JunCommon classes from JunCore, use the correct relative paths:
```cpp
// JunCore에서 JunCommon 참조 시 (../../ 사용)
#include "../../JunCommon/core/base.h"
#include "../../JunCommon/container/LFStack.h"
#include "../../JunCommon/container/LFQueue.h"
#include "../../JunCommon/container/RingBuffer.h"
#include "../../JunCommon/pool/LFObjectPool.h"
#include "../../JunCommon/pool/LFObjectPoolTLS.h"
#include "../../JunCommon/log/Logger.h"
#include "../../JunCommon/timer/Profiler.h"
#include "../../JunCommon/system/CrashDump.h"
#include "../../JunCommon/algorithm/Parser.h"

// JunCore 내부 상호 참조
#include "../buffer/packet.h"      // network에서 buffer 참조
#include "../protocol/message.h"   // 다른 폴더에서 protocol 참조
#include "../core/base.h"          // core utilities 참조
```

#### Key Utility Classes
- `Logger`: Thread-safe logging system with log levels (FATAL, ERROR, WARN, INFO, DEBUG)
  - Use `LOG(fileName, logLevel, format, ...)` macro for logging
- `CrashDump`: Automatic crash dump generation for debugging
- `PerformanceCounter`: System and network performance monitoring via PDH
- `ProcessCpuMonitor`/`MachineCpuMonitor`: CPU usage monitoring
- `MemoryLogger`: Memory usage tracking and debugging
- **`AES128`**: High-performance AES-128 encryption class
  - CBC mode: `Encrypt()`, `Decrypt()` (IV 필요)
  - ECB mode: `EncryptECB()`, `DecryptECB()` (IV 불필요, 간단한 데이터용)
  - Thread-local context 재사용으로 성능 최적화
  - `GenerateRandomKey()`: 안전한 16바이트 랜덤 키 생성
- **Hash Functions** (in `core/base.h`):
  - `fnv1a()`: FNV-1a 32비트 해시 함수 (문자열/바이트배열용)
  - `make_protocol_code()`: 프로토콜 버전 코드 생성 (fnv1a 기반)

#### SessionId Design
- 64-bit union combining 32-bit index and 32-bit unique identifier
- Prevents session ID reuse issues in high-concurrency scenarios
- Access via `sessionId.SESSION_INDEX` and `sessionId.SESSION_UNIQUE` macros

### Dependencies
- **vcpkg**: Package manager for C++ dependencies
- **protobuf**: Message serialization (installed via vcpkg)
- **openssl**: Cryptographic functions (recently added)
- **Windows SDK**: Required for IOCP and WinSock2 APIs
- **Visual Studio 2022**: Primary development environment

## 신규 파일 생성 시 프로젝트 추가 규칙

### Visual Studio 프로젝트 파일 업데이트 필수
- **신규 .cpp/.h 파일 생성 시 반드시 해당 .vcxproj 파일에 추가해야 함**
- 파일을 생성한 후 바로 프로젝트 파일을 업데이트하여 빌드 오류 방지
- 다음 명령으로 프로젝트 파일 구조 확인:
  ```bash
  # .vcxproj 파일에서 기존 파일 구조 파악
  grep -A 5 -B 5 "ClInclude\|ClCompile" JunCore/JunCore.vcxproj
  ```

### 프로젝트별 파일 추가 위치
- **JunCore 프로젝트**: `JunCore/JunCore.vcxproj`
- **JunCommon 프로젝트**: `JunCommon/JunCommon.vcxproj`  
- **EchoServer 프로젝트**: `EchoServer/EchoServer.vcxproj`
- **EchoClient 프로젝트**: `EchoClient/EchoClient.vcxproj`
- **Test 프로젝트**: `Test/Test.vcxproj`

### 파일 추가 예시
```xml
<!-- .vcxproj 파일에 헤더 파일 추가 -->
<ItemGroup>
  <ClInclude Include="network\IOCPManager.h" />
  <ClInclude Include="network\IPacketHandler.h" />
  <ClInclude Include="network\NetBase_New.h" />
  <ClInclude Include="network\NetworkArchitecture.h" />
</ItemGroup>

<!-- .vcxproj 파일에 소스 파일 추가 -->
<ItemGroup>
  <ClCompile Include="network\IOCPManager.cpp" />
  <ClCompile Include="network\NetBase_New.cpp" />
</ItemGroup>
```

### .vcxproj.filters 파일도 함께 업데이트
- Visual Studio 솔루션 탐색기에서 폴더 구조를 제대로 보여주기 위해 필요
- 필터 파일에도 동일하게 파일과 폴더 구조를 추가

## 코어 수정/신규 코어 생성 시 필수 규칙

### EchoServer/EchoClient 테스트 필수
- **네트워크 코어 수정 시**: 반드시 EchoServer와 EchoClient에서 해당 코어를 활용하도록 수정
- **신규 네트워크 아키텍처 생성 시**: 기존 EchoServer/EchoClient를 새로운 구조로 마이그레이션
- **검증 프로세스**:
  1. EchoServer와 EchoClient가 정상 빌드되는지 확인
  2. 서버-클라이언트 간 패킷 송수신이 정상 동작하는지 테스트
  3. 성능 저하가 없는지 확인 (TPS, 메모리 사용량 등)

### 브레이크포인트 디버깅 확인 필수
- **현재 문제**: `GetQueuedCompletionStatus` 호출이 여전히 `NetBase::RunWorkerThread`에서 발생
- **신규 아키텍처 적용 시**: 실제로 `IOCPManager::RunWorkerThread`에서 호출되는지 브레이크포인트로 검증 필요
- **검증 방법**:
  ```cpp
  // IOCPManager::RunWorkerThread에 브레이크포인트 설정
  BOOL retGQCS = GetQueuedCompletionStatus(iocpHandle, ...);  // 여기서 중단되어야 함
  
  // NetBase::RunWorkerThread에는 중단되지 않아야 함 (구 코드)
  ```

### 마이그레이션 체크리스트
1. **EchoServer.cpp/h 수정** - 새로운 네트워크 아키텍처 사용
2. **EchoClient.cpp/h 수정** - 새로운 네트워크 아키텍처 사용  
3. **컴파일 확인** - 모든 프로젝트가 오류 없이 빌드되는지 확인
4. **런타임 테스트** - 실제 서버-클라이언트 통신 테스트
5. **브레이크포인트 검증** - 새로운 코드 경로가 실행되는지 확인
6. **성능 비교** - 기존 대비 성능 저하 없는지 확인

### 주의사항
- **절대 기존 코어만 수정하고 방치하지 말 것** - 반드시 예제 프로젝트까지 함께 업데이트
- **단위별 검증** - 각 수정 사항마다 EchoServer/Client로 검증
- **문서화** - 주요 아키텍처 변경 시 이 파일(CLAUDE.md)에도 변경사항 기록

## 🚀 현재 세션 생명주기 관리 (2025-01 최신)

### 🎯 **shared_ptr 기반 세션 관리 정책**

#### 핵심 설계 원칙
**Session의 생명주기는 shared_ptr 참조 카운팅과 OverlappedEx를 통해 자동으로 관리됩니다.**

#### 1. 세션 생성 및 초기 참조 설정
```cpp
// Accept 시 shared_ptr로 세션 생성
auto session = std::make_shared<Session>();
session->Set(clientSocket, clientAddr.sin_addr, clientAddr.sin_port, this);

// 첫 번째 recv 등록으로 세션 활성화
session->PostAsyncReceive();
```

#### 2. 비동기 I/O 완료까지 세션 보장
```cpp
// OverlappedEx가 세션 shared_ptr을 보유
struct OverlappedEx {
    std::shared_ptr<Session> session_;  // I/O 완료까지 세션 생존 보장
    IOOperation operation_;
};

// 비동기 Send/Recv 등록 시
auto send_overlapped = new OverlappedEx(shared_from_this(), IOOperation::IO_SEND);
WSASend(sock_, wsaBuf, count, NULL, 0, &send_overlapped->overlapped_, NULL);
```

#### 3. I/O 완료 시 자동 참조 해제
```cpp
// IOCPManager::RunWorkerThread()에서 I/O 완료 처리
void IOCPManager::RunWorkerThread() {
    // ... IOCP 완료 처리 ...
    
    // overlapped 삭제 시 session shared_ptr 자동 해제
    delete p_overlapped;  // session 참조 카운트 자동 감소
}
```

### 🔧 **세션 생명주기 보장 메커니즘**

#### **I/O 진행 중 세션 보호**
- **Send 중**: `send_overlapped->session_`이 세션 참조 유지
- **Recv 중**: `recv_overlapped->session_`이 세션 참조 유지  
- **정상 세션**: 항상 recv가 걸려있어 최소 1개 참조 보장

#### **세션 해제 트리거**
1. **클라이언트 연결 해제** → recv 완료 (ioSize=0) → recv overlapped 삭제
2. **마지막 I/O 완료** → 모든 overlapped 삭제 → 참조 카운트 0
3. **shared_ptr 자동 소멸** → `Session::~Session()` 호출
4. **자동 정리** → `OnSessionDisconnect()` 호출 → 리소스 해제

#### **세션 상태 전이**
```
활성 세션: recv overlapped 상시 대기 (참조 카운트 ≥ 1)
연결 해제: recv 완료 → overlapped 삭제 → 참조 카운트 감소  
세션 소멸: 참조 카운트 0 → Session::~Session() → 자동 정리
```

### 🛡️ **메모리 안전성 보장**

#### **자동 생명주기 관리**
- **더블 프리 방지**: shared_ptr 자동 관리로 중복 해제 불가능
- **댕글링 포인터 방지**: 참조 카운팅으로 사용 중 소멸 방지
- **메모리 누수 방지**: RAII 패턴으로 자동 리소스 정리

#### **스레드 안전성**
- **원자적 참조 카운팅**: shared_ptr 내부 원자적 연산 사용
- **경쟁 상태 방지**: overlapped 삭제와 세션 접근 간 동기화 자동 처리

## Coding Standards

### Naming Conventions
- Follow **Google C++ Style Guide** for variable naming
- Use `snake_case` for variables and functions
- Use `PascalCase` for class names
- Use `UPPER_CASE` for constants and macros

### Code Formatting
- **Brace Style**: Place opening braces on new line for functions
  ```cpp
  void MyFunction()
  {
      // function body
  }
  ```
- **If Statement Braces**: Always use braces for if statements, even for single-line statements
  ```cpp
  // ✅ 올바른 방식
  if (condition)
  {
      DoSomething();
  }
  
  // ❌ 잘못된 방식 - 한 줄이라도 반드시 중괄호 사용
  if (condition)
      DoSomething();
  ```
- Use consistent indentation (4 spaces or tabs)

### File Encoding
- **All new .cpp/.h files must be created with UTF-8 BOM encoding**
- **Claude가 편집하거나 새로 생성하는 모든 파일은 UTF-8 BOM 인코딩 사용**
- This ensures consistent encoding across the Visual Studio project
- Use UTF-8 BOM when creating any C++ source or header files
- **UTF-8 BOM prevents Korean text corruption and Visual Studio build warnings**

### Build Requirements
- **Zero Warnings Policy**: All code must compile without warnings
- Fix any compiler warnings before proceeding with development
- **Always build after completing work** to verify no compilation issues
- Use appropriate warning level settings in project configuration

### Comments and Documentation
- **No Emoji Comments**: Never use emoji characters in comments or code
  - Use plain, professional language in comments
  - Comments should be concise and descriptive
  - Example: Use "Initialize network" instead of "🚀 Initialize network"
- Keep comments clean and simple
- Use Korean for user-facing messages when appropriate

## Code Refactoring History (2025-08)

### NetServer/NetClient 중복 코드 제거 프로젝트
대규모 리팩토링을 통해 NetServer와 NetClient 간의 중복 코드를 대폭 제거하여 유지보수성과 코드 품질을 향상시켰습니다.

#### 🎯 주요 성과
- **SendCompletion**: 22줄 → 5줄 (77% 감소)
- **RecvCompletion**: 98줄 → 9줄 (91% 감소) 
- **WorkerFunc**: 93줄 → 3줄 (97% 감소)
- **전체 중복 코드**: 약 200+ 줄 제거

#### 🔧 리팩토링 완료 항목

##### 1. IoCommon.h 공통 함수 추출
**위치**: `JunCore/network/IoCommon.h`
- `SendCompletion()`: Send 완료 처리 템플릿 함수
- `RecvCompletionLan()`: LAN 모드 Recv 완료 처리  
- `RecvCompletionNet()`: NET 모드 Recv 완료 처리 (암호화 지원)
- `IOCPWorkerLoop()`: IOCP 메인 루프 템플릿
- `AsyncSend()`: 비동기 송신 등록
- `AsyncRecv()`: 비동기 수신 등록

**특징**: 모든 함수가 템플릿 기반으로 구현되어 런타임 오버헤드 없음

##### 2. Session 클래스 IO Count 관리 통합  
**위치**: `JunCore/network/Session.h`
```cpp
// 현재 구현된 함수들
inline void SetIOCP(HANDLE iocp_handle);
inline HANDLE GetIOCP();
inline void Disconnect() noexcept;
template<typename T> bool SendPacket(const T& packet);
void PostAsyncSend();
bool PostAsyncReceive();
```

**개선점**:
- shared_ptr 기반 자동 생명주기 관리로 명시적 IO Count 관리 불필요
- OverlappedEx를 통한 I/O 완료까지 세션 보장
- RAII 패턴으로 자동 리소스 정리

##### 3. 어댑터 패턴을 통한 WorkerFunc 통합
**NetServer 어댑터**: `ServerSessionManager` 클래스
**NetClient 어댑터**: `ClientSessionManager` 클래스

**구현 예시**:
```cpp
// 기존 93줄 → 새로운 3줄
void NetServer::WorkerFunc() 
{
    ServerSessionManager sessionManager(this);
    IoCommon::IOCPWorkerLoop(h_iocp, sessionManager);
}
```

#### 🚀 성능 최적화 원칙
1. **Zero Runtime Overhead**: 모든 공통화가 템플릿/inline 기반
2. **TPS 함수는 기존 유지**: 고성능이 중요한 부분은 최적화된 구조 보존  
3. **메모리 효율성**: 가상함수 테이블 생성 방지
4. **캐시 친화적**: alignas(64) 활용한 메모리 레이아웃 최적화

#### 🏗️ 아키텍처 개선 사항
- **관심사 분리**: 공통 네트워크 로직과 서버/클라이언트 특화 로직 분리
- **캡슐화 강화**: Session이 자신의 상태와 IO Count를 직접 관리
- **코드 재사용**: 템플릿 메타프로그래밍을 통한 안전한 코드 공유
- **유지보수성**: 핵심 로직 변경 시 IoCommon 한 곳만 수정하면 됨

#### 🔍 리팩토링 방법론
**3단계 점진적 접근법** 사용:
1. **Phase 1**: 공통 함수 추출 (IoCommon.h)
2. **Phase 2**: 기반 클래스 패턴 적용  
3. **Phase 3**: 템플릿 기반 완전 통합

**핵심 설계 원칙**:
- 기존 API 호환성 100% 유지
- 성능 저하 없는 리팩토링
- 컴파일 타임 에러 감지로 안전성 보장

## 현재 서버 아키텍처 구조 (2025-01)

### 🎯 핵심 아키텍처: 공유 IOCP + Engine별 처리 모델

#### 현재 구조 개요
```
SharedIOCPManager
    ├── IOCP Handle (모든 엔진 공유)
    ├── Worker Threads (순수 I/O 이벤트 처리)
    └── 패킷 조립 완료 → Session의 Engine으로 전달

NetEngine (NetBase 상속)
    ├── Session 관리 (engine 포인터 이미 구현됨)
    ├── 패킷 핸들링 (OnRecv 구현)
    └── [향후] 전용 ThreadPool 추가 예정
```

#### 현재 구현된 핵심 컴포넌트

**IOCPManager.h** - IOCP 이벤트 처리 전담
```cpp
class IOCPManager final {
private:
    HANDLE iocpHandle;                    // 공유 IOCP 핸들
    std::vector<std::thread> workerThreads; // I/O 워커들
    
public:
    void RunWorkerThread();               // 순수 I/O 이벤트만 처리
    void DeliverPacketToHandler(Session* session, PacketBuffer* packet);
};
```

**Session.h** - 엔진 포인터 연결 구조
```cpp
class Session {
private:
    class NetBase* engine = nullptr;      // 이 세션을 소유한 엔진
    
public:
    inline void SetEngine(class NetBase* eng) { engine = eng; }
    inline class NetBase* GetEngine() const { return engine; }
};
```

### 🚀 다음 구현 단계: Engine별 스레드풀 분리

#### Phase 1: 패킷 처리를 Engine 스레드에게 위임
현재: `IOCPManager::RunWorkerThread`에서 직접 `OnRecv()` 호출
향후: Engine별 ThreadPool에 PacketJob으로 전달

```cpp
// 목표 구조
class NetEngine : public NetBase {
private:
    std::unique_ptr<ThreadPool> packetHandlerPool;  // 엔진별 전용 스레드풀
    LFQueue<PacketJob*> packetQueue;               // 비동기 패킷 처리 큐
    
public:
    void ProcessPacketAsync(Session* session, PacketBuffer* packet);
};
```

#### Phase 2: 우선순위 기반 리소스 배분
```cpp
// 엔진별 스레드 수 설정
GameServer Engine: 4개 핸들러 스레드 (고성능)
Client Engine: 1개 핸들러 스레드 (경량화)
```

### 핵심 설계 원칙
- **역할 분리**: IOCP는 순수 I/O만, Engine은 패킷 핸들링만
- **공유 리소스**: 하나의 IOCP 핸들을 여러 엔진이 효율적으로 활용
- **확장성**: 엔진별 독립적 성능 튜닝 가능
- **Zero Overhead**: 템플릿/인라인 기반 구현으로 런타임 오버헤드 없음

### 현재 상태
- ✅ IOCPManager 구현 완료 (Builder 패턴)
- ✅ Session-Engine 연결 구조 구현 완료
- ✅ 패킷 조립 로직 완료
- 🔄 **다음**: Engine별 ThreadPool + PacketJob 구현 예정

---

## 🎮 Job 시스템 아키텍처 (2025-01)

### 🎯 **핵심 원리: JobObject 중심 설계**

**가장 중요한 개념**:
- **Job은 JobObject에 속함** (LogicThread에 속하는 것이 아님!)
- **JobObject가 어느 LogicThread에 있는지에 따라 Job 실행 위치가 결정됨**
- **LogicThread 변경 시 JobObject가 자동으로 새 스레드로 이동**

### 📋 Job 시스템 동작 흐름

```
1. PostJob(job) 호출
   └→ JobObject의 m_jobQueue에 Job 추가
   └→ JobObject를 현재 LogicThread의 처리 큐에 등록

2. LogicThread::RunWorkerThread()
   └→ 처리할 JobObject 꺼내기
   └→ JobObject::Flush() 호출

3. JobObject::Flush()
   └→ m_jobQueue에서 Job 하나씩 꺼내서 실행
   └→ 매 Job 실행 후 LogicThread 변경 확인
   └→ 변경되었으면 새 LogicThread에 JobObject 등록하고 종료

4. 새 LogicThread
   └→ 이동된 JobObject의 남은 Job들을 계속 처리
```

### 🔧 **JobObject::Flush() - 핵심 구현**

```cpp
void JobObject::Flush()
{
    LogicThread* pOldThread = m_pLogicThread;  // 현재 스레드 저장

    Job job;
    while (m_jobQueue.Dequeue(&job))
    {
        job();  // Job 실행

        // ⚠️ 핵심: 매 Job 실행 후 스레드 변경 확인
        if (m_pLogicThread != pOldThread)
        {
            // 새 스레드에 등록하고 즉시 종료
            // m_processing은 true 유지 (아직 처리 중)
            m_pLogicThread->GetJobQueue()->Enqueue(this);
            return;  // 현재 스레드에서는 더 이상 처리하지 않음
        }
    }

    // 처리 완료
    m_processing.store(false);
}
```

**중요 포인트**:
- Job 실행 중 `SetLogicThread()`가 호출되면 `m_pLogicThread` 변경
- 변경 감지 시 즉시 새 스레드에 JobObject 등록하고 현재 스레드는 처리 중단
- 남은 Job들은 새 스레드에서 계속 처리됨

### 🎮 **GameObject와 Job 시스템**

#### GameObject는 JobObject 상속

```cpp
class GameObject : public JobObject
{
protected:
    GameScene* m_pScene = nullptr;

    explicit GameObject(GameScene* scene)
        : JobObject(scene->GetLogicThread())  // Scene에서 LogicThread 가져옴
        , m_pScene(scene)
    {
    }
};
```

**설계 원칙**:
- GameObject는 **LogicThread를 직접 알지 못함**
- GameObject는 오직 **GameScene에만 관심**
- LogicThread는 GameScene의 구현 디테일

#### Factory 패턴으로 안전한 생성

```cpp
// ✅ Factory 패턴 사용 (안전)
template<typename T, typename... Args>
static T* Create(GameScene* scene, Args&&... args)
{
    // 1. 객체 생성 (생성자 완료)
    T* obj = new T(scene, std::forward<Args>(args)...);

    // 2. Scene에 Enter Job 등록 (완전히 초기화된 객체 사용)
    obj->PostJob([obj, scene]() {
        scene->Enter(obj);
        obj->OnEnter(scene);
    });

    return obj;
}

// 사용 예시
Player* player = GameObject::Create<Player>(lobbyScene, user, "Player1");
```

**왜 Factory 패턴?**
- 생성자에서 `this`를 Job으로 넘기면 파생 클래스 생성 전일 수 있음
- Factory는 완전히 초기화된 객체만 Job에 넣음

#### Scene 이동 구현 (MoveToScene)

```cpp
void GameObject::MoveToScene(GameScene* newScene)
{
    if (newScene == nullptr)
        return;

    // Job #1: Exit + LogicThread 변경
    PostJob([this, newScene]() {
        if (m_pScene == newScene)
            return;

        // 이전 Scene에서 Exit
        if (m_pScene)
        {
            m_pScene->Exit(this);
            OnExit(m_pScene);
        }

        // Scene 포인터 업데이트
        m_pScene = newScene;

        // LogicThread 변경
        SetLogicThread(newScene->GetLogicThread());
        // ↑ 이 시점에서 m_pLogicThread 변경됨
    });

    // Job #2: Enter (새로운 LogicThread에서 실행)
    // 주의: Job #1에서 LogicThread 변경 시 JobObject::Flush()가
    //       자동으로 이 JobObject를 새 LogicThread로 이동시킴
    //       따라서 이 Job은 새 LogicThread에서 실행됨
    PostJob([this, newScene]() {
        newScene->Enter(this);
        OnEnter(newScene);
    });
}
```

**핵심 메커니즘**:
1. Job #1 실행 → `SetLogicThread(newThread)` 호출
2. `Flush()`가 LogicThread 변경 감지
3. JobObject를 새 LogicThread에 등록하고 종료
4. Job #2는 새 LogicThread에서 실행됨

**왜 Job을 2개로 나눴는가?**
- Job #1: 현재 LogicThread에서 Exit 처리
- LogicThread 변경 (자동 이동)
- Job #2: 새 LogicThread에서 Enter 처리
- 스레드 안전성 보장!

### 🚀 **Job 시스템 사용 예시**

#### 패킷 핸들러를 Job으로 처리

```cpp
// NetworkThread에서 패킷 수신
void OnPacketReceived(User* user, const game::MoveRequest& req)
{
    Player* player = user->GetPlayer();

    // LogicThread에서 처리되도록 Job 등록
    player->PostJob([player, req]() {
        // 이 코드는 player의 LogicThread에서 실행됨
        player->SetDestPos(req.dest_pos());
    });
}
```

#### Scene 전환과 LogicThread 이동

```cpp
// Player가 던전 입장
dungeonScene->SetLogicThread(dungeonLogicThread);  // 던전 전용 스레드

// Scene 이동 (자동으로 LogicThread도 이동)
player->MoveToScene(dungeonScene);
// ↑ player는 이제 dungeonLogicThread에서 처리됨
```

### 📊 **Job 시스템 vs 기존 Thread Pool**

| 항목 | Job 시스템 | Thread Pool |
|------|-----------|-------------|
| Job 소속 | JobObject에 속함 | Queue에 속함 |
| 스레드 이동 | JobObject 이동으로 자동 | 불가능 |
| 순서 보장 | JobObject별 순서 보장 | 보장 안 됨 |
| Actor 모델 | ✅ 완벽히 지원 | ❌ 미지원 |
| Scene 이동 | ✅ 자동 처리 | ❌ 수동 처리 필요 |

### ⚠️ **주의사항**

1. **Job은 JobObject에 넣는 것!**
   - ❌ LogicThread에 Job을 직접 넣지 않음
   - ✅ JobObject에 PostJob() 호출

2. **LogicThread 변경 시 자동 이동**
   - SetLogicThread() 호출 시
   - Flush()가 자동으로 새 스레드에 JobObject 등록
   - 남은 Job들은 새 스레드에서 실행

3. **Factory 패턴 사용 권장**
   - 생성자에서 this 사용 시 파생 클래스 초기화 전
   - Factory로 완전한 초기화 보장

4. **Scene 이동은 Job으로**
   - MoveToScene() 사용
   - 직접 Exit/Enter 호출하지 말 것
   - 스레드 안전성 보장

---

## 🚧 **TODO: JobObject 생명주기 관리** (미구현)

### 현재 문제점

**JobObject는 생성은 명확하지만 삭제가 불명확함**

#### 문제 1: delete 시점
```cpp
player->PostJob([player]() {
    player->GetScene()->Exit(player);
    delete player;  // ❌ 위험! Flush()가 Job 실행 후 player에 접근
});
```

**왜 안되나**: `JobObject::Flush()`가 Job 실행 후 `m_pLogicThread` 확인 시 이미 delete된 메모리 접근

#### 문제 2: 일회용 JobObject
```cpp
// 1회용 작업 후 자동 삭제?
auto* job = new OneTimeJob();
job->PostJob([job]() {
    // 작업 실행
    // 삭제는 누가? 언제?
});
```

### 해결 방안 (미정)

#### 옵션 1: shared_ptr 기반 관리 (추천)
```cpp
class JobObject : public std::enable_shared_from_this<JobObject> {
    // ref counting 자동 관리
};

auto player = std::make_shared<Player>(...);
player->PostJob([player]() {  // shared_ptr 캡처로 생명주기 보장
    player->GetScene()->Exit(player.get());
});
// Job 완료 후 자동 삭제
```

**장점**:
- 자동 생명주기 관리
- 스레드 안전
- 표준 라이브러리 사용

**단점**:
- 기존 코드 대대적 수정 필요
- raw pointer와 혼용 시 위험

#### 옵션 2: 수동 ref counting
```cpp
class JobObject {
    std::atomic<int> refCount_{1};
public:
    void AddRef() { refCount_++; }
    void Release() { if (--refCount_ == 0) delete this; }
};

// 사용
player->AddRef();
player->PostJob([player]() {
    // ...
    player->Release();
});
```

**장점**:
- 세밀한 제어 가능
- COM 스타일 익숙함

**단점**:
- 수동 관리 부담
- 실수 가능성

#### 옵션 3: Owner 기반 관리
```cpp
// GameObject는 Scene이 소유 및 삭제
// 일회용 JobObject는 명시적 정책 (예: Job 완료 후 자동 삭제 플래그)
class JobObject {
    bool autoDelete_ = false;
public:
    void SetAutoDelete(bool enable) { autoDelete_ = enable; }
};
```

**장점**:
- 명확한 소유권
- 기존 패턴 유지

**단점**:
- 일회용 객체 관리 복잡

### 현재 임시 조치

```cpp
void GameServer::OnUserDisconnect(User* user) {
    Player* player = user->GetPlayer();

    player->PostJob([player]() {
        if (player->GetScene())
            player->GetScene()->Exit(player);
        // TODO: delete player - 생명주기 관리 정책 필요
    });

    user->ClearPlayer();
}
```

**주의**: Player 메모리 누수 발생! 정책 결정 후 해결 필요

### 결정해야 할 사항

1. **GameObject 삭제 정책**: Scene이 소유? shared_ptr?
2. **일회용 JobObject 삭제**: 자동? 수동? ref counting?
3. **기존 코드 마이그레이션**: shared_ptr 전환? 점진적 적용?

**우선순위**: High - 메모리 누수 및 댕글링 포인터 위험