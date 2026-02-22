#include "GameScene.h"
#include "GameObject.h"
#include "GameThread.h"
#include "GameObjectManager.h"

std::atomic<int32_t> GameScene::s_nextSceneId{1};

GameScene::GameScene(GameThread* gameThread,
                     float mapMinX, float mapMinZ,
                     float mapMaxX, float mapMaxZ,
                     float cellSize, float hysteresisBuffer)
    : m_pGameThread(gameThread)
    , m_id(s_nextSceneId.fetch_add(1))
    , m_aoiGrid(mapMinX, mapMinZ, mapMaxX, mapMaxZ, cellSize, hysteresisBuffer)
{
    if (m_pGameThread)
    {
        m_pGameThread->AddScene(this);
    }
}

GameScene::~GameScene()
{
    if (m_pGameThread)
    {
        m_pGameThread->RemoveScene(this);
    }
}

void GameScene::Enter(GameObject* obj)
{
    m_objects.push_back(obj);
    m_aoiGrid.AddObject(obj, obj->GetX(), obj->GetZ());

    obj->m_pScene = this;
    obj->OnEnter();

    GameObjectManager::Instance().Register(obj);
}

void GameScene::Exit(GameObject* obj)
{
    obj->OnExit();

    // AoiGrid에서 제거 (주변 오브젝트에게 OnDisappear 발행)
    m_aoiGrid.RemoveObject(obj);

    obj->m_pScene = nullptr;

    auto it = std::find(m_objects.begin(), m_objects.end(), obj);
    if (it != m_objects.end())
    {
        m_objects.erase(it);
    }
}

void GameScene::FixedUpdate()
{
    OnFixedUpdate();

    for (auto* obj : m_objects)
    {
        obj->OnFixedUpdate();
    }
}

void GameScene::Update()
{
    OnUpdate();

    for (auto* obj : m_objects)
    {
        obj->OnUpdate();
    }
}
