#pragma once

#include <chrono>
#include <array>

// 슬라이딩 윈도우로 시간별 통계를 관리하는 카운터
// Add()로 값을 넣으면 1초 단위로 쌓이고, GetAverage()로 지정한 구간의 평균을 구할 수 있음
// 최대 60초까지 저장 가능하고, 시간이 지나면 자동으로 이전 데이터는 사라짐
// 스레드 안전하지 않으므로 TLS 변수로 쓰거나 단일 스레드에서만 사용할 것
template<typename T = uint64_t>
class TimeWindowCounter
{
public:
    static constexpr int MAX_SAMPLES = 60;  // 최대 60초

private:
    std::array<T, MAX_SAMPLES> samples_{};  // 값 초기화
    int write_pos_ = 0;
    std::chrono::steady_clock::time_point last_update_;

public:
    TimeWindowCounter() : last_update_(std::chrono::steady_clock::now()) {}

    // 현재 시간에 맞는 샘플에 값 추가
    void Add(T value) 
    {
        UpdatePosition();
        samples_[write_pos_] += value;
    }
    
    // 지정한 초 동안의 평균값을 구함 
    // 주의: "현재 시간"이 아니라 "마지막 Add 시점"에서 역산함
    // 예) write_pos가 5초고 GetAverage(3) 호출하면 -> 5초, 4초, 3초 슬롯을 합쳐서 평균냄
    double GetAverage(int seconds) const 
    {
        if (seconds <= 0 || seconds > MAX_SAMPLES)
        {
            return 0.0;
        }
        
        T total = 0;
        for (int i = 0; i < seconds; ++i) 
        {
            int index = (write_pos_ - i + MAX_SAMPLES) % MAX_SAMPLES;
            total += samples_[index];
        }
        return static_cast<double>(total) / seconds;
    }
    
private:
    // 시간이 지나면 write_pos 업데이트
    void UpdatePosition() 
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_).count();
        
        if (elapsed >= 1) 
        {
            // 지난 초만큼 위치 이동하고 새 슬롯들 초기화
            for (long long i = 0; i < elapsed && i < MAX_SAMPLES; ++i) 
            {
                write_pos_ = (write_pos_ + 1) % MAX_SAMPLES;
                samples_[write_pos_] = 0;  // 새 슬롯 초기화
            }
            last_update_ = now;
        }
    }
};