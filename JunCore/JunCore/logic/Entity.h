#pragma once
#include "Component.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>

//------------------------------
// Entity - 컴포넌트 컨테이너
// GameObject가 상속받아 사용
//------------------------------
class Entity
{
protected:
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_map<std::type_index, Component*> m_componentMap;

public:
    Entity() = default;
    virtual ~Entity() = default;

    // 복사 금지 (unique_ptr 멤버)
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    // 이동은 허용
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    //------------------------------
    // 컴포넌트 추가
    //------------------------------
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();

        ptr->m_pOwner = this;
        m_componentMap[std::type_index(typeid(T))] = ptr;
        m_components.push_back(std::move(component));

        ptr->OnAttach();

        return ptr;
    }

    //------------------------------
    // 컴포넌트 조회
    //------------------------------
    template<typename T>
    T* GetComponent()
    {
        auto it = m_componentMap.find(std::type_index(typeid(T)));
        if (it != m_componentMap.end())
        {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }

    template<typename T>
    const T* GetComponent() const
    {
        auto it = m_componentMap.find(std::type_index(typeid(T)));
        if (it != m_componentMap.end())
        {
            return static_cast<const T*>(it->second);
        }
        return nullptr;
    }

    //------------------------------
    // 컴포넌트 존재 여부
    //------------------------------
    template<typename T>
    bool HasComponent() const
    {
        return m_componentMap.find(std::type_index(typeid(T))) != m_componentMap.end();
    }

    //------------------------------
    // 컴포넌트 제거
    //------------------------------
    template<typename T>
    bool RemoveComponent()
    {
        auto mapIt = m_componentMap.find(std::type_index(typeid(T)));
        if (mapIt == m_componentMap.end())
        {
            return false;
        }

        Component* target = mapIt->second;
        target->OnDetach();

        // 벡터에서 찾아서 제거
        for (auto it = m_components.begin(); it != m_components.end(); ++it)
        {
            if (it->get() == target)
            {
                m_components.erase(it);
                break;
            }
        }

        m_componentMap.erase(mapIt);
        return true;
    }

    //------------------------------
    // 모든 컴포넌트 Update 호출
    //------------------------------
    void UpdateComponents()
    {
        for (auto& comp : m_components)
        {
            comp->OnUpdate();
        }
    }

    void FixedUpdateComponents()
    {
        for (auto& comp : m_components)
        {
            comp->OnFixedUpdate();
        }
    }
};
