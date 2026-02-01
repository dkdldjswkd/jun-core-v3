#pragma once
#include "JobObject.h"
#include "GameObject.h"
#include "GameScene.h"
#include <unordered_map>
#include <atomic>
#include <cstdint>
#include <type_traits>

//------------------------------
// GameObjectManager - 전역 GameObject 관리자
// JobObject 상속으로 코어 JobThread에서 실행됨
// - SN 발급: atomic (락 불필요)
// - 등록/해제/조회: JobThread에서 직렬 처리 (락 불필요)
//------------------------------
class GameObjectManager : public JobObject
{
private:
    std::unordered_map<uint64_t, GameObject*> m_objects;
    std::atomic<uint64_t> m_nextSN{1};

    // 싱글톤
    GameObjectManager();
    ~GameObjectManager();

public:
    GameObjectManager(const GameObjectManager&) = delete;
    GameObjectManager& operator=(const GameObjectManager&) = delete;

    //------------------------------
    // 싱글톤 접근
    //------------------------------
    static GameObjectManager& Instance();

    //------------------------------
    // 초기화 (Server에서 호출)
    // 코어 JobThread 설정
    //------------------------------
    void Initialize(JobThread* coreThread);

    //------------------------------
    // SN 발급 (atomic - 락 불필요, 어디서든 호출 가능)
    //------------------------------
    uint64_t GenerateSN()
    {
        return m_nextSN.fetch_add(1);
    }

    //------------------------------
    // 팩토리: 생성 + Scene Enter Job 등록
    // Lock 불필요 (SN은 atomic, 등록은 Enter에서 Job)
    //------------------------------
    template<typename T, typename... Args>
    T* Create(GameScene* scene, Args&&... args)
    {
        static_assert(std::is_base_of<GameObject, T>::value, "T must derive from GameObject");

        T* obj = new T(scene, std::forward<Args>(args)...);

        // Scene Enter Job 등록 (Enter에서 OnEnter + GM 등록)
        obj->PostJob([obj, scene]() {
            scene->Enter(obj);
        });

        return obj;
    }

    //------------------------------
    // GameObject 등록 (GameScene::Enter에서 호출)
    // JobObject의 PostJob으로 처리됨
    //------------------------------
    void Register(GameObject* obj);

    //------------------------------
    // GameObject 해제 (OnBeforeDestroy에서 호출)
    // JobObject의 PostJob으로 처리됨
    //------------------------------
    void Unregister(uint64_t sn);

    //------------------------------
    // 크로스 스레드 Job 전달
    // JobThread에서 대상 찾아서 PostJob
    //------------------------------
    void PostTo(uint64_t sn, Job job);
};
