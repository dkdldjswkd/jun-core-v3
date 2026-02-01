#include "GameObject.h"
#include "GameScene.h"
#include "LogicThread.h"
#include "GameObjectManager.h"

GameObject::GameObject(GameScene* scene)
    : JobObject(scene->GetLogicThread())
    , m_pScene(scene)
    , m_sn(GameObjectManager::Instance().GenerateSN())
{
}

GameObject::~GameObject()
{
}

void GameObject::Destroy()
{
    // PostJob으로 감싸서 LogicThread에서 실행되도록 보장
    // 외부 스레드에서 호출해도 안전
    PostJob([this]() {
        // Scene에서 Exit
        if (m_pScene)
        {
            m_pScene->Exit(this);
        }

        // 삭제 전 이벤트 발행 (구독자들에게 알림)
        OnBeforeDestroy.Invoke();

        // GameObjectManager에서 해제
        GameObjectManager::Instance().Unregister(m_sn);

        // 삭제 마킹 (이후 PostJob 거부, Flush 후 delete)
        MarkForDelete();
    });
}

void GameObject::MoveToScene(GameScene* newScene)
{
    if (newScene == nullptr)
    {
		return;
	}

    PostJob([this, newScene]() 
	{
        if (m_pScene == newScene)
        {
			return;
		}

        if (m_pScene)
        {
            m_pScene->Exit(this);
        }

        // LogicThread 변경
        SetJobThread(newScene->GetLogicThread());
    });

    // 위 Job에서 LogicThread를 변경하여, JobObject::Flush()에서 자동으로 이 JobObject를 새 LogicThread로 이동시킴
    // 즉, 이 새로운 씬에 입장하는 이 Job은 새 LogicThread에서 실행된다.
    PostJob([this, newScene]() {
        newScene->Enter(this);
    });
}

void GameObject::ExitScene()
{
    PostJob([this]() {
        if (m_pScene)
        {
            m_pScene->Exit(this);
        }
    });
}