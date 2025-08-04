﻿#include "CrashDump.h"

long CrashDump::_DumpCount;
bool CrashDump::wait = false;

CrashDump::CrashDump() {
	_DumpCount = 0;

	_invalid_parameter_handler oldHandler, newHandler;
	newHandler = mylnvalidParameterHandler;

	oldHandler = _set_invalid_parameter_handler(newHandler); // crt 함수에 null 포인터 등을 넣었을 때
	_CrtSetReportMode(_CRT_WARN, 0);	// crt 경고 메시지 표시하지 않음. 바로 정지.
	_CrtSetReportMode(_CRT_ASSERT, 0);	// crt 어설션 메시지 표시하지 않음. 바로 정지.
	_CrtSetReportMode(_CRT_ERROR, 0);	// crt 에러 메시지 표시하지 않음. 바로 정지.

	_CrtSetReportHook(_custom_Report_hook);

	// pure virtual function called 에러 핸들러를 설정하여 함수 포인터를 호출할 수 있게 함.
	_set_purecall_handler(myPurecallHandler);

	SetHandlerDump();
}

void CrashDump::Crash() {
	int* p = nullptr;
	*p = 0;
}

LONG WINAPI CrashDump::MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer) {
	if (InterlockedExchange8((CHAR*)&wait, true) != 0)
		Sleep(INFINITE);

	int iWorkingMemory = 0;
	SYSTEMTIME		stNowTime;
	long DumpCount = InterlockedIncrement(&_DumpCount);

	// 현재 프로세스가 사용하는 메모리 용량을 구한다.
	HANDLE hProcess = 0;
	PROCESS_MEMORY_COUNTERS pmc;

	hProcess = GetCurrentProcess();

	if (NULL == hProcess)
		return 0;

	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
		iWorkingMemory = (int)(pmc.WorkingSetSize / 1024 / 1024);
	}
	CloseHandle(hProcess);

	// 현재 시스템 시간을 알아온다.
	WCHAR filename[MAX_PATH];

	GetLocalTime(&stNowTime);
	wsprintf(filename, L"Dump_ %d%02d%02d_%02d.%02d.%02d_%d_%dMB.dmp",
		stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount, iWorkingMemory);

	wprintf(L"\n\n\n !!! Crash Error !!! %d.%d.%d / %d:%d:%d \n",
		stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
	wprintf(L"Now Save dump file... vn");

	HANDLE hDumpFile = ::CreateFile(filename,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDumpFile != INVALID_HANDLE_VALUE) {
		_MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptionInformation;

		MinidumpExceptionInformation.ThreadId = ::GetCurrentThreadId();
		MinidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
		MinidumpExceptionInformation.ClientPointers = TRUE;

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			hDumpFile,
			MiniDumpWithFullMemory,
			&MinidumpExceptionInformation,
			NULL,
			NULL);

		CloseHandle(hDumpFile);

		wprintf(L"CrashDump Save Finish !");
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void CrashDump::SetHandlerDump() {
	SetUnhandledExceptionFilter(MyExceptionFilter);
}

void CrashDump::mylnvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved) {
	Crash();
}

int CrashDump::_custom_Report_hook(int ireposttype, char* message, int* returnvalue) {
	Crash();
	return true;
}

void CrashDump::myPurecallHandler(void) {
	Crash();
}