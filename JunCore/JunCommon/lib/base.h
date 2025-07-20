#pragma once

// define Func
#define CRASH() do{ *((volatile int*)0) = 0; }while(false)

// Stamp (LockFree)
constexpr ULONG_PTR kStampMask  = 0xFFFF800000000000; // x64 ȯ�濡�� ������� �ʴ� �ּҰ���
constexpr ULONG_PTR kUseBitMask = 0x00007FFFFFFFFFFF; // x64 ȯ�濡�� ����ϴ� �ּҰ���
constexpr DWORD64	kStampCount = 0x0000800000000000; // ������� �ʴ� �ּҰ����� �ּҴ���
constexpr BYTE		kUnusedBit = 17;

// ����� �����ѹ�
constexpr ULONG_PTR kMemGuard = 0xFDFDFDFDFDFDFDFD;