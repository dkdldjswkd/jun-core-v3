# JunCore - Windows C++ Game Server Core

고성능 Windows C++ 게임 서버 개발을 위한 네트워크 라이브러리 및 코어 시스템

## 🎯 프로젝트 개요

**JunCore**는 IOCP 기반의 고성능 네트워크 라이브러리와 게임 서버 구현을 제공합니다.

### 📦 구성 요소

- **JunCore** - IOCP 기반 네트워크 라이브러리 (Static Library)
- **JunCommon** - 공통 유틸리티 라이브러리 (Static Library)
- **EchoServer / EchoClient** - 에코 서버/클라이언트 예제
- **GameServer** - 게임 서버
- **StressClient** - 스트레스 테스트 클라이언트
- **Test** - Protobuf, 암호화 등 테스트

### 🛠 기술 스택

- **언어**: C++20
- **플랫폼**: Windows 10/11
- **IDE**: Visual Studio 2022
- **네트워킹**: WinSock2, IOCP
- **메시지**: Protocol Buffers
- **패키지 관리**: vcpkg

## 🚀 빌드 방법

### 1. 빌드 환경

- Windows 10/11
- Visual Studio 2022

### 2. vcpkg 설치

```bash
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### 3. 프로젝트 빌드

```bash
git clone <repository-url>
cd JunCore

# Visual Studio에서 JunCore.sln 열고 빌드
# 또는 명령줄에서:
msbuild JunCore.sln /p:Configuration=Debug /p:Platform=x64
```

### 의존성 관리

외부 라이브러리(protobuf, openssl, boost)는 **vcpkg Manifest Mode**로 관리됩니다.

- `vcpkg.json`에 의존성이 정의되어 있으며, 빌드 시 자동으로 설치됩니다
- 별도로 `vcpkg install` 명령을 실행할 필요가 없습니다
- 트리플렛은 각 프로젝트에서 `x64-windows-static`(정적 링크)으로 설정되어 있습니다

## 🔧 개발 워크플로우

### .proto 파일 수정/추가 시

```bash
# 1. .proto 파일 수정 또는 새로 생성
# 2. Protobuf 코드 재생성 (모든 .proto 파일을 자동 검색/컴파일)
.\generate_protobuf.bat

# 3. 새 파일인 경우, 생성된 .pb.h/.pb.cc를 해당 .vcxproj에 추가
# 4. 프로젝트 재빌드
msbuild JunCore.sln /p:Configuration=Debug /p:Platform=x64
```

---

**JunCore** - High Performance Game Server Core for Windows