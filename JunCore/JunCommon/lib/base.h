#pragma once

// define Func
#define CRASH() do{ *((volatile int*)0) = 0; }while(false)

// Stamp (LockFree)
constexpr ULONG_PTR kStampMask  = 0xFFFF800000000000; // x64 환경에서 사용하지 않는 주소영역
constexpr ULONG_PTR kUseBitMask = 0x00007FFFFFFFFFFF; // x64 환경에서 사용하는 주소영역
constexpr DWORD64	kStampCount = 0x0000800000000000; // 사용하지 않는 주소영역의 최소단위
constexpr BYTE		kUnusedBit	= 17;

// 메모리 가드패턴
constexpr ULONG_PTR kMemGuard = 0xFDFDFDFDFDFDFDFD;