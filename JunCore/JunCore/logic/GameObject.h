#pragma once

class GameScene;

//------------------------------
// GameObject - Unity 스타일 게임 객체
// Player, Monster, NPC 등의 기반 클래스
//------------------------------
class GameObject
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

public:
    GameObject() = default;
    virtual ~GameObject() = default;

    //------------------------------
    // Scene 접근
    //------------------------------
    GameScene* GetScene() { return m_pScene; }
    bool IsInScene() const { return m_pScene != nullptr; }
};
