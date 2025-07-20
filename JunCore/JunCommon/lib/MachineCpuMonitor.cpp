#include "MachineCpuMonitor.h"
#include <iostream>

// Ȯ�� ���μ��� �ڵ�
MachineCpuMonitor::MachineCpuMonitor() {
	// ���μ��� ����� == (���ð� / �����ھ� ��)
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	NumOfCore = SystemInfo.dwNumberOfProcessors; // ���� �ھ� ����
	UpdateCpuUsage();
}

// CPU ���� ���� (500 ~ 1000 ms �ֱ� ȣ��)
void MachineCpuMonitor::UpdateCpuUsage() {
	ULARGE_INTEGER curIdleTime;
	ULARGE_INTEGER curkernelTime;
	ULARGE_INTEGER curUserTime;

	// ���� �ھ� ��� �ð� (Ŀ��, ����, idle)
	if (GetSystemTimes((PFILETIME)&curIdleTime, (PFILETIME)&curkernelTime, (PFILETIME)&curUserTime) == false) {
		return;
	}

	// �ھ� ��� �ð� (��Ÿ, ���� - ������ ȣ��)
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
		// �ھ� ��� ��з� ���
		coreTotal = (float)((double)(Total - deltaIdleTime) / Total * 100.0f);
		coreUser = (float)((double)deltaUserTime / Total * 100.0f);
		coreKernel = (float)((double)(deltaKernelTime - deltaIdleTime) / Total * 100.0f);
	}

	prevKernelTime = curkernelTime;
	prevUserTime = curUserTime;
	prevIdleTime = curIdleTime;	
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