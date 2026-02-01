#pragma once
#include "GameObject.h"
#include "GameScene.h"
#include "../core/Event.h"
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <cstdint>

//------------------------------
// GameObjectManager - 전역 GameObject 관리자
// 팩토리 패턴으로 생성 + SN 기반 조회 제공
//------------------------------
class GameObjectManager
{
private:
    std::unordered_map<uint64_t, GameObject*> m_objects;
    std::unordered_map<uint64_t, Subscription> m_subscriptions;
    mutable std::shared_mutex m_mutex;
    std::atomic<uint64_t> m_nextSN{1};

    // 싱글톤
    GameObjectManager() = default;
    ~GameObjectManager() = default;

public:
    GameObjectManager(const GameObjectManager&) = delete;
    GameObjectManager& operator=(const GameObjectManager&) = delete;

    //------------------------------
    // 싱글톤 접근
    //------------------------------
    static GameObjectManager& Instance()
    {
        static GameObjectManager instance;
        return instance;
    }

    //------------------------------
    // 팩토리: 생성 + SN 부여 + 등록 + Event 구독
    //------------------------------
    template<typename T, typename... Args>
    T* Create(GameScene* scene, Args&&... args)
    {
        static_assert(std::is_base_of<GameObject, T>::value, "T must derive from GameObject");

        // 1. SN 발급
        uint64_t sn = m_nextSN++;

        // 2. 객체 생성
        T* obj = new T(scene, std::forward<Args>(args)...);
        obj->m_sn = sn;

        // 3. 맵에 등록 + 삭제 이벤트 구독
        {
            std::unique_lock lock(m_mutex);
            m_objects[sn] = obj;

            m_subscriptions[sn] = obj->OnBeforeDestroy.Subscribe([this, sn]() {
                Unregister(sn);
            });
        }

        // 4. Scene에 Enter Job 등록
        obj->PostJob([obj, scene]() {
            scene->Enter(obj);
        });

        return obj;
    }

    //------------------------------
    // 조회 (SN으로 GameObject 찾기)
    //------------------------------
    GameObject* Find(uint64_t sn)
    {
        std::shared_lock lock(m_mutex);
        auto it = m_objects.find(sn);
        if (it == m_objects.end())
        {
            return nullptr;
        }

        // 삭제 마킹된 경우 null 반환
        if (it->second->IsMarkedForDelete())
        {
            return nullptr;
        }

        return it->second;
    }

    //------------------------------
    // 타입 캐스팅 조회
    //------------------------------
    template<typename T>
    T* Find(uint64_t sn)
    {
        return dynamic_cast<T*>(Find(sn));
    }

    //------------------------------
    // 크로스 스레드 Job 전달
    //------------------------------
    bool PostTo(uint64_t sn, Job job)
    {
        GameObject* obj = nullptr;
        {
            std::shared_lock lock(m_mutex);
            auto it = m_objects.find(sn);
            if (it == m_objects.end())
            {
                return false;
            }
            obj = it->second;
        }
        // 락 해제 후 PostJob 호출 (PostJob은 lock-free로 자체 스레드 안전)
        return obj->PostJob(std::move(job));
    }

    //------------------------------
    // 등록 해제 (OnBeforeDestroy에서 자동 호출)
    //------------------------------
    void Unregister(uint64_t sn)
    {
        std::unique_lock lock(m_mutex);
        m_objects.erase(sn);
        m_subscriptions.erase(sn);
    }

    //------------------------------
    // 등록된 객체 수
    //------------------------------
    size_t Count() const
    {
        std::shared_lock lock(m_mutex);
        return m_objects.size();
    }
};
