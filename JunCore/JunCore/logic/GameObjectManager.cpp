#include "GameObjectManager.h"
#include "GameObject.h"
#include <stdexcept>

// 싱글톤 생성 (JobThread 없이)
GameObjectManager::GameObjectManager()
    : JobObject()  // 기본 생성자 사용
{
}

GameObjectManager::~GameObjectManager()
{
}

GameObjectManager& GameObjectManager::Instance()
{
    static GameObjectManager instance;
    return instance;
}

void GameObjectManager::Initialize(JobThread* coreThread)
{
    if (coreThread == nullptr)
    {
        throw std::invalid_argument("GameObjectManager::Initialize: JobThread cannot be null");
    }
    SetJobThread(coreThread);
}

void GameObjectManager::Register(GameObject* obj)
{
    if (obj == nullptr)
    {
        return;
    }

    uint64_t sn = obj->GetSN();
    PostJob([this, sn, obj]() {
        m_objects[sn] = obj;
    });
}

void GameObjectManager::Unregister(uint64_t sn)
{
    PostJob([this, sn]() {
        m_objects.erase(sn);
    });
}

void GameObjectManager::PostTo(uint64_t sn, Job job)
{
    PostJob([this, sn, job = std::move(job)]() {
        auto it = m_objects.find(sn);
        if (it == m_objects.end())
        {
            return;
        }

        if (GameObject* obj = it->second)
        {
			obj->PostJob(std::move(job));
		}
    });
}