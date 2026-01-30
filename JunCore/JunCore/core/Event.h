#pragma once
#include <functional>
#include <unordered_map>
#include <cstdint>

//──────────────────────────────────────────────────────────────
// Subscription - RAII 기반 구독 토큰
// 소멸 시 자동으로 구독 해제됨
//──────────────────────────────────────────────────────────────
class Subscription
{
public:
    Subscription() = default;

    explicit Subscription(std::function<void()> unsubscribe)
        : m_unsubscribe(std::move(unsubscribe))
    {
    }

    ~Subscription()
    {
        Unsubscribe();
    }

    // 이동만 허용 (복사 금지)
    Subscription(Subscription&& other) noexcept
        : m_unsubscribe(std::move(other.m_unsubscribe))
    {
        other.m_unsubscribe = nullptr;
    }

    Subscription& operator=(Subscription&& other) noexcept
    {
        if (this != &other)
        {
            Unsubscribe();
            m_unsubscribe = std::move(other.m_unsubscribe);
            other.m_unsubscribe = nullptr;
        }
        return *this;
    }

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    void Unsubscribe()
    {
        if (m_unsubscribe)
        {
            m_unsubscribe();
            m_unsubscribe = nullptr;
        }
    }

    // 자동 해제 비활성화 (수동 관리하고 싶을 때)
    void Detach()
    {
        m_unsubscribe = nullptr;
    }

    // 유효한 구독인지 확인
    bool IsValid() const
    {
        return m_unsubscribe != nullptr;
    }

private:
    std::function<void()> m_unsubscribe;
};

//──────────────────────────────────────────────────────────────
// Event<Args...> - 타입 안전한 이벤트 시스템
// 다중 구독자 지원, RAII 자동 해제
//──────────────────────────────────────────────────────────────
template<typename... Args>
class Event
{
public:
    using Handler = std::function<void(Args...)>;

    Event() = default;
    ~Event() = default;

    // 복사/이동 금지 (이벤트는 소유자에게 귀속)
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = delete;
    Event& operator=(Event&&) = delete;

    // 구독 - RAII 토큰 반환
    [[nodiscard]]
    Subscription Subscribe(Handler handler)
    {
        uint64_t id = m_nextId++;
        m_handlers[id] = std::move(handler);

        return Subscription([this, id]()
        {
            m_handlers.erase(id);
        });
    }

    // 발행
    void Invoke(Args... args)
    {
        // 복사본으로 순회 (콜백 중 구독 해제해도 안전)
        auto handlersCopy = m_handlers;
        for (const auto& [id, handler] : handlersCopy)
        {
            // 순회 중 해제되었는지 확인
            if (m_handlers.contains(id))
            {
                handler(args...);
            }
        }
    }

    // operator() 오버로딩 - 자연스러운 호출
    void operator()(Args... args)
    {
        Invoke(std::forward<Args>(args)...);
    }

    // 구독자 수
    size_t Count() const
    {
        return m_handlers.size();
    }

    // 구독자 있는지 확인
    bool HasSubscribers() const
    {
        return !m_handlers.empty();
    }

    // 모든 구독 해제
    void Clear()
    {
        m_handlers.clear();
    }

private:
    std::unordered_map<uint64_t, Handler> m_handlers;
    uint64_t m_nextId{0};
};
