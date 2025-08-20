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

### Build Requirements
- **Zero Warnings Policy**: All code must compile without warnings
- Fix any compiler warnings before proceeding with development
- **Always build after completing work** to verify no compilation issues
- Use appropriate warning level settings in project configuration

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