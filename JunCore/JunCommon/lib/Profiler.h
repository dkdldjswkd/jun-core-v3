#pragma once
#include <Windows.h>
#include <timeapi.h>

//#define UNUSE_PROFILE

#ifdef UNUSE_PROFILE
#define PRO_BEGIN(TagName)
#define PRO_END(TagName)
#define PRO_RESET()		
#define PRO_DATAOUT()	
#endif

#ifndef UNUSE_PROFILE
#define PRO_BEGIN(TagName)	Profiler::getInst().ProfileBegin(TagName)
#define PRO_END(TagName)	Profiler::getInst().ProfileEnd(TagName)
#define PRO_RESET()			Profiler::getInst().ProfileReset()
#define PRO_FILEOUT()		Profiler::getInst().ProfileFileOut()
#endif

#define MAX_THREAD_NUM		20
#define PROFILE_DATA_NUM	100
#define PROFILE_NAME_LEN	128

class Profiler {
private:
	Profiler();
public:
	~Profiler();

public:
	static Profiler& getInst() {
		static Profiler instance;
		return instance;
	}

private:
	struct ProfileData {
	public:
		bool useFlag = false;
		bool resetFlag = false;
		char name[PROFILE_NAME_LEN] = {0, }; // �������� ���� �̸�.

	public:
		DWORD64 totalTime	= 0;
		DWORD	totalCall	= 0;
		DWORD	startTime	= 0; // ProfileBegin

		// ��� ��� �� �����ϱ� ����
		DWORD	max[2] = { 0, 0 };				
		DWORD	min[2] = { MAXDWORD, MAXDWORD };

	public:
		// ���ܽ�ų �����Ͷ�� ret true (max, min ���ú� ����)
		bool VaildateData(DWORD profileTime) {
			for (int i = 0; i < 2; i++) {
				if (max[i] < profileTime) {
					max[i] = profileTime;
					return true;
				}
			}
			for (int i = 0; i < 2; i++) {
				if (min[i] > profileTime) {
					min[i] = profileTime;
					return true;
				}
			}
			return false;
		}
		void Init() {
			totalTime	= 0;
			totalCall	= 0;
			max[0] = 0;
			max[1] = 0;
			min[0] = MAXDWORD;
			min[1] = MAXDWORD;
		}
	};

private:
	const DWORD tls_index_;
	int profile_index_ = -1;
	ProfileData profile_data_[MAX_THREAD_NUM][PROFILE_DATA_NUM];

private:
	ProfileData* Get();

public:
	void ProfileBegin(const char* name);
	void ProfileEnd(const char* name);
	void ProfileFileOut();
	void ProfileReset();
};