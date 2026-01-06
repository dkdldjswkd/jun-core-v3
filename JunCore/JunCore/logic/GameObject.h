#pragma once
#include "JobObject.h"

class GameScene;

//------------------------------
// GameObject - Unity 스타일 게임 객체
// Player, Monster, NPC 등의 기반 클래스
//
// JobObject 상속:
// - GameObject는 Scene에 종속되어 존재
// - Scene 이동 시 LogicThread도 함께 이동 가능
// - 모든 상태 변경을 Job으로 처리하여 스레드 안전 보장
//------------------------------
class GameObject : public JobObject
{
protected:
    GameScene* m_pScene = nullptr;

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnEnter(GameScene* scene) {}
    virtual void OnExit(GameScene* scene) {}
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

    friend class GameScene;

    //------------------------------
    // 생성자 (protected - Factory 패턴 사용)
    //------------------------------
    explicit GameObject(GameScene* scene);

public:
    virtual ~GameObject();

    //------------------------------
    // Factory 메서드 (완전한 초기화 보장)
    // 템플릿 구현은 이 헤더 끝에 정의됨 (GameScene.h include 후)
    //------------------------------
    template<typename T, typename... Args>
    static T* Create(GameScene* scene, Args&&... args);

    //------------------------------
    // Scene 관리
    //------------------------------
    void MoveToScene(GameScene* newScene);
    void ExitScene();

    //------------------------------
    // Scene 접근
    //------------------------------
    GameScene* GetScene() { return m_pScene; }
    bool IsInScene() const { return m_pScene != nullptr; }
};

//------------------------------
// 템플릿 구현 (GameScene.h 필요)
//------------------------------
#include "GameScene.h"

template<typename T, typename... Args>
inline T* GameObject::Create(GameScene* scene, Args&&... args)
{
    static_assert(std::is_base_of<GameObject, T>::value, "T must derive from GameObject");

    // 1. 객체 생성 (생성자 완료)
    T* obj = new T(scene, std::forward<Args>(args)...);

    // 2. Scene에 Enter Job 등록 (완전히 초기화된 객체 사용)
    // GameScene::Enter()가 내부에서 OnEnter() 호출함
    obj->PostJob([obj, scene]() {
        scene->Enter(obj);
    });

    return obj;
}
