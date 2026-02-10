# JunCore - Windows C++ Game Server Core

Windows 환경에서 고성능 게임 서버 개발을 목표로 하는 C++ 서버 코어 및 네트워크 라이브러리입니다.

---

## 프로젝트 개요

**JunCore**는 IOCP 기반의 네트워크 레이어와 게임 서버 구현에 필요한 공통 코어를 제공합니다.

### 구성 요소

- **JunCore**
  IOCP 기반 네트워크 라이브러리 (Static Library)

- **JunCommon**
  공통 유틸리티 및 기반 모듈 (Static Library)

- **EchoServer / EchoClient**
  네트워크 동작 확인용 에코 서버/클라이언트 예제

- **GameServer**
  실제 게임 서버 구현

- **StressClient**
  부하 테스트용 클라이언트

- **Test**
  Protobuf, 암호화 등 기능 테스트 프로젝트

---

## 기술 스택

- **Language**: C++20
- **Platform**: Windows 10 / 11
- **IDE**: Visual Studio 2022
- **Networking**: WinSock2, IOCP
- **Serialization**: Protocol Buffers

---

## 빌드 방법

### 빌드 환경

- Windows 10 / 11
- Visual Studio 2022
- vcpkg

### 프로젝트 빌드

Visual Studio에서 `JunCore.sln`을 열고 빌드합니다.

첫 빌드 시 `vcpkg.json`에 정의된 의존성이 자동으로 설치될 수 있으며, 이 경우 시간이 다소 소요될 수 있습니다.

### 의존성 관리

모든 프로젝트의 vcpkg 설정은 `Directory.Build.props`에서 일괄 관리됩니다:

| 속성 | 값 | 설명 |
|------|------|------|
| `VcpkgEnableManifest` | `true` | Manifest Mode - `vcpkg.json` 기반으로 의존성을 자동 관리 |
| `VcpkgTriplet` (x64) | `x64-windows-static` | 정적 링크 (별도 DLL 배포 불필요) |
| `VcpkgTriplet` (Win32) | `x86-windows-static` | 정적 링크 (별도 DLL 배포 불필요) |

> 각 프로젝트(.vcxproj)에서 개별 설정할 필요 없이, 이 파일 하나로 모든 프로젝트에 일괄 적용됩니다.

의존성 목록은 `vcpkg.json`에 정의되어 있으며 (protobuf, openssl, boost), 빌드 시 자동으로 설치되므로 별도 `vcpkg install` 불필요합니다.

### 빌드 출력 경로

```
build/
  bin/x64/Debug/      # 실행 파일 출력
  bin/x64/Release/
  obj/                # 중간 파일 출력
```

---

## Protobuf 코드 생성

### 기본 원칙

`.pb.cc`와 `.pb.h`는 `.proto`로부터 자동 생성되는 파일이므로 **Git에 커밋하지 않습니다** (`.gitignore`에 등록됨).

### 자동 생성

JunCore 프로젝트의 빌드 전 이벤트에서 Protobuf 코드가 자동 생성됩니다.

- `.proto` 파일이 변경된 경우에만 protoc가 실행됩니다
- 변경 없으면 스킵

### 수동 생성 (필요 시)

```bash
# 솔루션 루트에서 실행
.\generate_protobuf.bat
```
