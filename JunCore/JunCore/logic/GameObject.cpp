#include "GameObject.h"
#include "GameScene.h"

GameObject::GameObject(GameScene* scene)
    : JobObject(scene->GetLogicThread())
    , m_pScene(scene)
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

        // 삭제 마킹 (이후 PostJob 거부, Flush 후 delete)
        MarkForDelete();
    });
}

void GameObject::MoveToScene(GameScene* newScene)
{
    if (newScene == nullptr)
        return;

    // Job #1: Exit + LogicThread 변경
    PostJob([this, newScene]() {
        // 같은 Scene이면 무시
        if (m_pScene == newScene)
            return;

        // 이전 Scene에서 Exit (Exit 내부에서 OnExit + m_pScene=nullptr 처리)
        if (m_pScene)
        {
            m_pScene->Exit(this);
        }

        // LogicThread 변경 (같은 스레드여도 호출, 내부에서 비교 로직 있을 수 있음)
        SetLogicThread(newScene->GetLogicThread());
    });

    // Job #2: Enter (새로운 LogicThread에서 실행)
    // 주의: Job #1에서 LogicThread 변경 시 JobObject::Flush()가
    //       자동으로 이 JobObject를 새 LogicThread로 이동시킴
    //       따라서 이 Job은 새 LogicThread에서 실행됨
    PostJob([this, newScene]() {
        newScene->Enter(this);  // Enter 내부에서 m_pScene 설정 + OnEnter 호출
    });
}

void GameObject::ExitScene()
{
    PostJob([this]() {
        if (m_pScene)
        {
            m_pScene->Exit(this);  // Exit 내부에서 OnExit + m_pScene=nullptr 처리
        }
    });
}
