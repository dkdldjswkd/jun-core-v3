#pragma once

// Windows 헤더 충돌 방지를 위한 공통 include 헤더
// 모든 Windows API 관련 헤더는 이 파일을 include하여 일관성 보장

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Deprecated WinSock API 경고 비활성화
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

// Winsock 관련 헤더를 Windows.h보다 먼저 include (충돌 방지)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

// 추가 Windows 헤더들
#include <winnt.h>

// Winsock 라이브러리 링크
#pragma comment(lib, "ws2_32.lib")