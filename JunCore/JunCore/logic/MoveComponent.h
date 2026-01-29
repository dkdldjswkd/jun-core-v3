#pragma once
#include "Component.h"
#include <cmath>

//------------------------------
// MoveComponent - 이동 기능 컴포넌트
// 위치, 목표 위치, 이동 속도 관리
//------------------------------
class MoveComponent : public Component
{
public:
    MoveComponent() = default;
    MoveComponent(float speed) : m_moveSpeed(speed) {}
    ~MoveComponent() override = default;

    //------------------------------
    // Component 인터페이스
    //------------------------------
    void OnFixedUpdate() override
    {
        if (!HasReachedDestination())
        {
            MoveTowardsDestination();
        }
    }

    //------------------------------
    // 위치 설정/조회
    //------------------------------
    void SetPosition(float x, float y, float z)
    {
        m_currentX = x;
        m_currentY = y;
        m_currentZ = z;
    }

    void GetPosition(float& x, float& y, float& z) const
    {
        x = m_currentX;
        y = m_currentY;
        z = m_currentZ;
    }

    float GetX() const { return m_currentX; }
    float GetY() const { return m_currentY; }
    float GetZ() const { return m_currentZ; }

    //------------------------------
    // 목표 위치 설정/조회
    //------------------------------
    void SetDestination(float x, float y, float z)
    {
        m_destX = x;
        m_destY = y;
        m_destZ = z;
    }

    void GetDestination(float& x, float& y, float& z) const
    {
        x = m_destX;
        y = m_destY;
        z = m_destZ;
    }

    float GetDestX() const { return m_destX; }
    float GetDestY() const { return m_destY; }
    float GetDestZ() const { return m_destZ; }

    //------------------------------
    // 이동 속도
    //------------------------------
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    float GetMoveSpeed() const { return m_moveSpeed; }

    //------------------------------
    // 이동 상태
    //------------------------------
    bool HasReachedDestination() const
    {
        float dx = m_destX - m_currentX;
        float dz = m_destZ - m_currentZ;
        float distanceSq = dx * dx + dz * dz;

        return distanceSq < (ARRIVAL_THRESHOLD * ARRIVAL_THRESHOLD);
    }

    bool IsMoving() const
    {
        return !HasReachedDestination();
    }

private:
    void MoveTowardsDestination()
    {
        float dx = m_destX - m_currentX;
        float dz = m_destZ - m_currentZ;
        float distance = std::sqrt(dx * dx + dz * dz);

        if (distance < 0.001f)
        {
            return;
        }

        float dirX = dx / distance;
        float dirZ = dz / distance;

        float moveDistance = m_moveSpeed;
        if (moveDistance > distance)
        {
            moveDistance = distance;
        }

        m_currentX += dirX * moveDistance;
        m_currentZ += dirZ * moveDistance;
    }

private:
    // 현재 위치
    float m_currentX{0.0f};
    float m_currentY{0.0f};
    float m_currentZ{0.0f};

    // 목표 위치
    float m_destX{0.0f};
    float m_destY{0.0f};
    float m_destZ{0.0f};

    // 이동 속도 (FixedUpdate당 이동 거리)
    float m_moveSpeed{0.1f};

    // 도착 판정 임계값
    static constexpr float ARRIVAL_THRESHOLD = 0.1f;
};
