#include "GameScene.h"
#include "GameObject.h"
#include "LogicThread.h"

GameScene::GameScene(LogicThread* logicThread)
    : m_pLogicThread(logicThread)
{
    if (m_pLogicThread)
    {
        m_pLogicThread->AddScene(this);
    }
}

GameScene::~GameScene()
{
    if (m_pLogicThread)
    {
        m_pLogicThread->RemoveScene(this);
    }
}

void GameScene::Enter(GameObject* obj)
{
    m_objects.push_back(obj);
    obj->m_pScene = this;
    obj->OnEnter(this);
}

void GameScene::Exit(GameObject* obj)
{
    obj->OnExit(this);
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
