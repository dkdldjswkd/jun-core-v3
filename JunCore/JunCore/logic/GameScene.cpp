#include "GameScene.h"

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

void GameScene::FlushJobs()
{
    std::function<void()> job;
    while (m_jobQueue.Dequeue(&job))
    {
        job();
    }
}
