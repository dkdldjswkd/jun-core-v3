#pragma once
#include "AoiGrid.h"
#include "../../JunCommon/container/LFQueue.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <utility>
#include <atomic>

class GameThread;
class GameObject;

//------------------------------
// GameScene - Unity 스타일 씬
// Map, Dungeon, Lobby 등의 기반 클래스
//------------------------------
class GameScene
{
protected:
    std::vector<GameObject*> m_objects;
    GameThread* m_pGameThread = nullptr;
    AoiGrid m_aoiGrid;

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

public:
    // mapMinX/Z ~ mapMaxX/Z: 맵 범위
    // cellSize: 셀 한 변의 길이
    // hysteresisBuffer: 셀 이동 판정 버퍼 (경계 깜빡임 방지)
    GameScene(GameThread* gameThread,
              float mapMinX, float mapMinZ,
              float mapMaxX, float mapMaxZ,
              float cellSize, float hysteresisBuffer);

    virtual ~GameScene();

    //------------------------------
    // GameObject 관리
    //------------------------------
    void Enter(GameObject* obj);
    void Exit(GameObject* obj);

    //------------------------------
    // 프레임 업데이트 (GameThread에서 호출)
    //------------------------------
    void FixedUpdate();
    void Update();

    //------------------------------
    // GameThread 관리
    //------------------------------
    GameThread* GetGameThread() { return m_pGameThread; }

    //------------------------------
    // GameObject 조회
    //------------------------------
    const std::vector<GameObject*>& GetObjects() const { return m_objects; }

    //------------------------------
    // AOI: 인접 9셀 오브젝트 반환
    //------------------------------
    std::vector<GameObject*> GetNearbyObjects(GameObject* obj, bool includeSelf = false) const
    {
        return m_aoiGrid.GetNearbyObjects(obj->GetX(), obj->GetZ(), includeSelf ? nullptr : obj);
    }

    //------------------------------
    // AOI: 인접 9셀 오브젝트 순회
    //------------------------------
    void ForEachAdjacentObjects(GameObject* obj, bool includeSelf, const std::function<void(GameObject*)>& fn) const
    {
        m_aoiGrid.ForEachAdjacentObjects(obj->GetX(), obj->GetZ(), [&](GameObject* o)
        {
            if (!includeSelf && o == obj)
            {
                return;
            }
            fn(o);
        });
    }

    //------------------------------
    // AOI: 오브젝트 위치 갱신 (GameObject::SetPosition에서 호출)
    //------------------------------
    void OnObjectMoved(GameObject* obj, float x, float z)
    {
        m_aoiGrid.UpdatePosition(obj, x, z);
    }

    //------------------------------
    // Scene ID
    //------------------------------
    int32_t GetId() const { return m_id; }

private:
    static std::atomic<int32_t> s_nextSceneId;
    int32_t m_id{0};
};
