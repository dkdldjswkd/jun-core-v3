#pragma once
#include "../JunCore/logic/Component.h"
#include "../JunCore/logic/Time.h"
#include "../JunCore/core/Event.h"

//--------------------------------------------------------------
// AttackComponent - 공격 상태 머신 컴포넌트
// 상태: Idle -> Attacking (딜레이 대기) -> 데미지 적용 -> Idle
// 이벤트: OnDamageApply (target_id, damage)
//--------------------------------------------------------------
class AttackComponent : public Component
{
public:
    //----------------------------------------------------------
    // 이벤트
    //----------------------------------------------------------
    Event<int32_t, int32_t> OnDamageApply;  // (target_id, damage)

public:
    AttackComponent() = default;
    ~AttackComponent() override = default;

    //----------------------------------------------------------
    // Component 인터페이스
    //----------------------------------------------------------
    void OnFixedUpdate() override
    {
        if (m_state != State::Attacking)
        {
            return;
        }

        m_elapsed += Time::fixedDeltaTime();

        if (m_elapsed >= DAMAGE_DELAY)
        {
            // 데미지 적용 이벤트 발행
            OnDamageApply(m_targetId, DAMAGE_AMOUNT);
            m_state = State::Idle;
        }
    }

    //----------------------------------------------------------
    // 공격 시작
    //----------------------------------------------------------
    void StartAttack(int32_t target_id)
    {
        m_targetId = target_id;
        m_elapsed = 0.0f;
        m_state = State::Attacking;
    }

    //----------------------------------------------------------
    // 공격 취소 (이동 등으로 인한 취소)
    //----------------------------------------------------------
    void CancelAttack()
    {
        m_state = State::Idle;
    }

    //----------------------------------------------------------
    // 상태 조회
    //----------------------------------------------------------
    bool IsAttacking() const { return m_state == State::Attacking; }

private:
    enum class State { Idle, Attacking };

    State m_state{State::Idle};
    int32_t m_targetId{0};
    float m_elapsed{0.0f};

    // 공격 모션 선딜 (데미지 적용까지 딜레이)
    static constexpr float DAMAGE_DELAY = 0.4f;

    // 데미지량
    static constexpr int32_t DAMAGE_AMOUNT = 1;
};
