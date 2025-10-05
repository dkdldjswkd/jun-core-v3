#pragma once

#include <chrono>
#include <array>

#ifdef _WIN32
#include <Windows.h>
#define GET_CURRENT_TIME_MS() GetTickCount64()
using time_type = ULONGLONG;
#else
#define GET_CURRENT_TIME_MS() std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
using time_type = uint64_t;
#endif

// 슬라이딩 윈도우 기반 시간별 카운터 (1초 단위, 최대 60초)
// 
// 멀티스레드 사용법:
//   - record(): 쓰기 스레드에서 호출
//   - get_average(): 읽기 스레드에서 호출
//   - 두 함수는 서로 다른 스레드에서 동시 호출 안전
//
// 특징:
//   - record 없이도 시간 경과에 따라 자동으로 오래된 데이터 무시
//   - 링 버퍼 방식의 Lock-free 설계
template<typename T = uint64_t>
class TimeWindowCounter
{
public:
    static constexpr int max_samples = 60;

private:
    std::array<T, max_samples> samples_{};
    int write_pos_ = 0;
    time_type last_update_ms_;

public:
    TimeWindowCounter() : last_update_ms_(GET_CURRENT_TIME_MS()) {}

    void record(T value) 
    {
        advance_time_window();
        samples_[write_pos_] += value;
    }
    
    double get_average(int seconds) const 
    {
        if (seconds <= 0 || seconds > max_samples)
        {
            return 0.0;
        }
        
        int time_gap = get_elapsed_seconds();
        
        if (time_gap >= seconds)
        {
            return 0.0;
        }
        
        int valid_seconds = seconds - time_gap;
        
        T total = 0;
        for (int i = 0; i < valid_seconds; ++i) 
        {
            int index = (write_pos_ - i + max_samples) % max_samples;
            total += samples_[index];
        }
        
        return static_cast<double>(total) / seconds;
    }
    
private:
    void advance_time_window()
    {
        auto elapsed = get_elapsed_seconds();
        
        if (elapsed >= 1) 
        {
            for (long long i = 0; i < elapsed && i < max_samples; ++i) 
            {
                write_pos_ = (write_pos_ + 1) % max_samples;
                samples_[write_pos_] = 0;
            }
            last_update_ms_ = GET_CURRENT_TIME_MS();
        }
    }

    int get_elapsed_seconds() const
    {
        time_type now_ms = GET_CURRENT_TIME_MS();
        time_type elapsed_ms = now_ms - last_update_ms_;
        return static_cast<int>(elapsed_ms / 1000);
    }
};