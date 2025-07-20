#include "ProcessCpuMonitor.h"
#include <iostream>

// 프로세스 CPU 모니터링
ProcessCpuMonitor::ProcessCpuMonitor(HANDLE hProcess){
	if (hProcess == INVALID_HANDLE_VALUE) {
		h_process = GetCurrentProcess();
	}

	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	NumOfCore = SystemInfo.dwNumberOfProcessors; // 전체 코어 개수
	UpdateCpuUsage();
}

// CPU 사용률 업데이트 (500 ~ 1000 ms 주기로 호출)
void ProcessCpuMonitor::UpdateCpuUsage() {
	ULARGE_INTEGER none; // 미사용
	ULARGE_INTEGER curTime;
	ULARGE_INTEGER curkernelTime;
	ULARGE_INTEGER curUserTime;

	// 현재시간 & 현재 코어 사용시간
	GetSystemTimeAsFileTime((LPFILETIME)&curTime);
	GetProcessTimes(h_process, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&curkernelTime, (LPFILETIME)&curUserTime);

	// 코어 전체 시간 (해당 프로세스만)
	ULONGLONG deltaTime			= curTime.QuadPart		 - prevTime.QuadPart;
	ULONGLONG deltaUserTime		= curUserTime.QuadPart	 - prevUserTime.QuadPart;
	ULONGLONG deltaKernelTime	= curkernelTime.QuadPart - prevKernelTime.QuadPart;
	ULONGLONG totalTime = deltaKernelTime + deltaUserTime;

	// 코어 전체 시간 사용률
	coreTotal = (float)(totalTime / (double)NumOfCore / (double)deltaTime * 100.0f);
	coreUser = (float)(deltaUserTime / (double)NumOfCore / (double)deltaTime * 100.0f);
	coreKernel = (float)(deltaKernelTime / (double)NumOfCore / (double)deltaTime * 100.0f);

	prevTime = curTime;
	prevKernelTime = curkernelTime;
	prevUserTime = curUserTime;
}

float ProcessCpuMonitor::GetTotalCpuUsage() {
	return coreTotal;
}

float ProcessCpuMonitor::GetUserCpuUsage() {
	return coreUser;
}

float ProcessCpuMonitor::GetKernelCpuUsage() {
	return coreKernel;
}