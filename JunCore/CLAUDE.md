# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Building the Solution
```bash
# Build entire solution
msbuild JunCore.sln /p:Configuration=Debug /p:Platform=x64
msbuild JunCore.sln /p:Configuration=Release /p:Platform=x64

# Build specific project
msbuild JunCore/JunCore.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild EchoServer/EchoServer.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild EchoClient/EchoClient.vcxproj /p:Configuration=Debug /p:Platform=x64
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

### Lock-Free Data Structures (`JunCommon/lib/`)
- `LFStack`: Lock-free stack with ABA problem prevention
- `LFQueue`: Lock-free queue implementation
- `LFObjectPool`: Lock-free object pool for high-performance allocation
- Uses memory stamping technique to prevent ABA problems

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

#### Key Utility Classes
- `Logger`: Thread-safe logging system with log levels (FATAL, ERROR, WARN, INFO, DEBUG)
  - Use `LOG(fileName, logLevel, format, ...)` macro for logging
- `CrashDump`: Automatic crash dump generation for debugging
- `PerformanceCounter`: System and network performance monitoring via PDH
- `ProcessCpuMonitor`/`MachineCpuMonitor`: CPU usage monitoring
- `MemoryLogger`: Memory usage tracking and debugging

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