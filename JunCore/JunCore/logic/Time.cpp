#include "Time.h"

//------------------------------
// TLS 변수 정의
//------------------------------
thread_local float Time::s_deltaTime = 0.0f;
thread_local float Time::s_fixedDeltaTime = 0.02f;  // 기본 50Hz
thread_local float Time::s_time = 0.0f;
thread_local int Time::s_frameCount = 0;
