#include "GameScene.h"
#include "GameObject.h"
#include "GameThread.h"
#include "GameObjectManager.h"

GameScene::GameScene(GameThread* gameThread)
    : m_pGameThread(gameThread)
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
    obj->m_pScene = this;
    obj->OnEnter();

    // OnEnter 완료 후 GameObjectManager에 등록
    // 다른 스레드에서 PostTo로 접근 가능해짐
    GameObjectManager::Instance().Register(obj);
}

void GameScene::Exit(GameObject* obj)
{
    obj->OnExit();
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
