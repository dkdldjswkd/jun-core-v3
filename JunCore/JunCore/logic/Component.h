#pragma once

class Entity;

//------------------------------
// Component - 기능 단위 컴포넌트 기반 클래스
// MovementComponent, RenderComponent 등의 기반
//------------------------------
class Component
{
protected:
    Entity* m_pOwner = nullptr;

    friend class Entity;

public:
    Component() = default;
    virtual ~Component() = default;

    // 복사/이동 금지
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate() {}
    virtual void OnFixedUpdate() {}

    //------------------------------
    // Owner 접근
    //------------------------------
    Entity* GetOwner() const { return m_pOwner; }
};
