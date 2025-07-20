#include "MachineCpuMonitor.h"
#include <iostream>

// 머신 전체 CPU 모니터링
MachineCpuMonitor::MachineCpuMonitor() {
	// 전체 프로세스 사용률 == (전체시간 / 전체코어 수)
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	NumOfCore = SystemInfo.dwNumberOfProcessors; // 전체 코어 개수
	UpdateCpuUsage();
}

// CPU 사용률 업데이트 (500 ~ 1000 ms 주기로 호출)
void MachineCpuMonitor::UpdateCpuUsage() {
	ULARGE_INTEGER curIdleTime;
	ULARGE_INTEGER curkernelTime;
	ULARGE_INTEGER curUserTime;

	// 현재 시스템 전체 시간 (커널, 유저, idle)
	if (GetSystemTimes((PFILETIME)&curIdleTime, (PFILETIME)&curkernelTime, (PFILETIME)&curUserTime) == false) {
		return;
	}

	// 이전 전체 시간 (델타, 현재 - 이전의 차이)
	ULONGLONG deltaIdleTime = curIdleTime.QuadPart - prevIdleTime.QuadPart;
	ULONGLONG deltaKernelTime = curkernelTime.QuadPart - prevKernelTime.QuadPart;
	ULONGLONG deltaUserTime = curUserTime.QuadPart - prevUserTime.QuadPart;
	ULONGLONG Total = deltaKernelTime + deltaUserTime;

	if (Total == 0) {
		coreUser = 0.0f;
		coreKernel = 0.0f;
		coreTotal = 0.0f;
	}
	else {
		// 이전 전체 사용률을 계산
		coreTotal = (float)((double)(Total - deltaIdleTime) / Total * 100.0f);
		coreUser = (float)((double)deltaUserTime / Total * 100.0f);
		coreKernel = (float)((double)(deltaKernelTime - deltaIdleTime) / Total * 100.0f);
	}

	prevKernelTime = curkernelTime;
	prevUserTime = curUserTime;
	prevIdleTime = curIdleTime;	// 다음 호출을 위해 저장
}

float MachineCpuMonitor::GetTotalCpuUsage(void) {
	return coreTotal;
}

float MachineCpuMonitor::GetUserCpuUsage(void) {
	return coreUser;
}

float MachineCpuMonitor::GetKernelCpuUsage(void) {
	return coreKernel;
}