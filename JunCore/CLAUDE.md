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

#### Adding New Protocol Messages
1. Create or modify `.proto` files in the appropriate project directory
2. Run `./generate_protobuf.bat` to generate C++ code
3. Add generated `.pb.h` and `.pb.cc` files to the Visual Studio project
4. Include the header and use the generated classes

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

## 🚨 핵심 세션 생명주기 관리 (절대 수정 금지)

### 핵심 원칙
**이 두 코드 블록은 JunCore의 핵심 아키텍처이므로 절대 수정하지 말 것. 수정이 필요하다면 반드시 사용자와 논의 후 진행.**

#### 1. Session::DecrementIOCount() - 세션 해제 트리거
```cpp
__forceinline void DecrementIOCount() noexcept
{
    if (0 == InterlockedDecrement(&io_count_))
    {
        // * release_flag_(0), IOCount(0) -> release_flag_(1), IOCount(0)
        if (0 == InterlockedCompareExchange64((long long*)release_flag_, 1, 0))
        {
            Release();
        }
    }
}
```

#### 2. IOCPManager::RunWorkerThread() - 필수 IOCount 감소
```cpp
DecrementIOCount:
    session->DecrementIOCount();
```

### 아키텍처 설계 원리

#### IOCount 기반 세션 생명주기
1. **비동기 I/O 등록 시**: `IncrementIOCount()` 호출
2. **IOCP 완료 통지 시**: 반드시 `DecrementIOCount()` 호출
3. **IOCount가 0 도달 시**: 해당 스레드에서 세션 정리 수행

#### 이중 보호 메커니즘
- **release_flag_**: 세션 해제 중복 방지
- **IOCount**: 활성 비동기 작업 수 추적
- **InterlockedCompareExchange64**: 원자적 해제 상태 전환

#### 세션 상태 전이
```
정상 세션: IOCount ≥ 1 (상시 recv 대기)
비정상 세션: IOCount → 0 (연결 끊김/에러)
해제 트리거: IOCount == 0 && release_flag_ == 0
```

### 핵심 보장 사항
1. **세션 정리 단일 책임**: IOCount를 0으로 만든 스레드가 정리 담당
2. **중복 해제 방지**: `release_flag_` CAS 연산으로 한 번만 해제
3. **상시 활성 상태**: 정상 세션은 recv 대기로 IOCount ≥ 1 유지
4. **자동 정리**: 비정상 종료 시 IOCount 수렴을 통한 자동 세션 해제

### 수정 금지 이유
- **스레드 안전성**: 정교한 원자적 연산 기반 구현
- **메모리 안전성**: 댕글링 포인터 및 더블 프리 방지
- **성능 최적화**: Lock-free 아키텍처로 고성능 보장
- **아키텍처 핵심**: 전체 네트워크 시스템의 근간

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
- Use consistent indentation (4 spaces or tabs)

### File Encoding
- **All new .cpp/.h files must be created with UTF-8 BOM encoding**
- This ensures consistent encoding across the Visual Studio project
- Use UTF-8 BOM when creating any C++ source or header files

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
// 추가된 함수들
inline void IncrementIOCount();
inline bool DecrementIOCount();
inline bool DecrementIOCountPQCS();
inline void DisconnectSession();
inline void SetIOCP(HANDLE iocp_handle);
```

**개선점**:
- IO Count 관리 로직을 Session에 캡슐화
- NetServer/NetClient에서 중복된 IO 관리 코드 제거
- IOCP 핸들 관리 중앙집중화

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