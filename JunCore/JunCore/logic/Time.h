#pragma once

//------------------------------
// Time - Unity 스타일 시간 관리 (TLS 기반)
// GameThread별 독립적인 시간 관리
//------------------------------
class Time
{
private:
    static thread_local float s_deltaTime;
    static thread_local float s_fixedDeltaTime;
    static thread_local float s_time;
    static thread_local int s_frameCount;

public:
    //------------------------------
    // 게임 로직에서 사용 (읽기 전용)
    //------------------------------
    static float deltaTime() { return s_deltaTime; }
    static float fixedDeltaTime() { return s_fixedDeltaTime; }
    static float time() { return s_time; }
    static int frameCount() { return s_frameCount; }

    //------------------------------
    // GameThread에서만 호출 (내부용)
    //------------------------------
    static void SetDeltaTime(float dt) { s_deltaTime = dt; }
    static void SetFixedDeltaTime(float dt) { s_fixedDeltaTime = dt; }
    static void SetTime(float t) { s_time = t; }
    static void SetFrameCount(int count) { s_frameCount = count; }
};
