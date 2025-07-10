# JunCore - Windows C++ Game Server Core

고성능 Windows C++ 게임 서버 개발을 위한 네트워크 라이브러리 및 코어 시스템

## 🎯 프로젝트 개요

**JunCore**는 IOCP 기반의 고성능 네트워크 라이브러리와 게임 서버 구현을 제공합니다.

### 📦 구성 요소

- **NetCore** - IOCP 기반 네트워크 라이브러리 (Static Library)
- **GameServer** - 게임 서버 애플리케이션
- **TestClient** - 테스트 클라이언트 애플리케이션

### 🛠 기술 스택

- **언어**: C++20
- **플랫폼**: Windows 10/11
- **IDE**: Visual Studio 2022
- **네트워킹**: WinSock2, IOCP
- **메시지**: Protocol Buffers
- **패키지 관리**: vcpkg

## 🚀 빌드 환경 설정

### 1. 필수 요구사항

- Windows 10/11
- Visual Studio 2022 (Community 이상)

### 2. vcpkg 설치 및 설정

```bash
# C 드라이브에 vcpkg 설치
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Visual Studio에 vcpkg 연결
.\vcpkg integrate install

# protobuf 설치
.\vcpkg install protobuf:x64-windows
```

### 3. 프로젝트 빌드

```bash
# 프로젝트 클론
git clone <repository-url>
cd JunCore

# Protobuf 파일 생성
.\generate_protobuf.bat

# Visual Studio로 빌드
# 또는 명령줄에서:
msbuild JunCore.sln /p:Configuration=Debug /p:Platform=x64
```

## 🔧 개발 워크플로우

### .proto 파일 수정 시

```bash
# 1. .proto 파일 수정
# 2. Protobuf 재생성
.\generate_protobuf.bat

# 3. 프로젝트 재빌드
msbuild JunCore.sln /p:Configuration=Debug /p:Platform=x64
```

### 새로운 .proto 파일 추가 시

1. 프로젝트 내 원하는 위치에 `.proto` 파일 생성
2. `.\generate_protobuf.bat` 실행 (자동으로 모든 .proto 검색/컴파일)
3. 생성된 `.pb.h`, `.pb.cc` 파일을 프로젝트에 추가
4. 프로젝트 재빌드

---

**JunCore** - High Performance Game Server Core for Windows