# Game Server Core 아키텍처 설계

## 1. 개요

Unity 스타일의 프레임 기반 게임 서버 코어 설계.
MMO와 캐주얼 게임 모두 지원 가능한 범용 구조.

---

## 2. Core 클래스 구조

### 2.1 Time (TLS 기반)

```cpp
// LogicThread가 여러 개일 수 있으므로 TLS로 관리
class Time
{
    static thread_local float s_deltaTime;
    static thread_local float s_fixedDeltaTime;
    static thread_local float s_time;
    static thread_local int s_frameCount;

public:
    static float deltaTime() { return s_deltaTime; }
    static float fixedDeltaTime() { return s_fixedDeltaTime; }
    static float time() { return s_time; }
    static int frameCount() { return s_frameCount; }

    // LogicThread에서만 호출
    static void SetDeltaTime(float dt) { s_deltaTime = dt; }
    static void SetFixedDeltaTime(float dt) { s_fixedDeltaTime = dt; }
    static void SetTime(float t) { s_time = t; }
    static void SetFrameCount(int count) { s_frameCount = count; }
};
```

### 2.2 JobObject

```cpp
class JobObject
{
protected:
    JobQueue m_jobQueue;
    std::atomic<bool> m_processing{false};
    LogicThread* m_pLogicThread = nullptr;

public:
    void PostJob(Job job)
    {
        m_jobQueue.Push(job);

        bool expected = false;
        if (m_processing.compare_exchange_strong(expected, true))
        {
            m_pLogicThread->GetJobQueue()->Push(this);
        }
    }

    void Flush()
    {
        LogicThread* pOldThread = m_pLogicThread;  // 현재 스레드 저장

        do {
            while (auto job = m_jobQueue.Pop())
            {
                job->Execute();

                // 스레드가 변경되었다면 (ChangeScene 등)
                if (m_pLogicThread != pOldThread)
                {
                    // 새 스레드에 등록하고 즉시 종료
                    // m_processing은 true 유지 (아직 처리 중)
                    m_pLogicThread->GetJobQueue()->Push(this);
                    return;
                }
            }
        } while (!m_jobQueue.Empty());

        m_processing.store(false);

        // 마지막 체크 - 그 사이에 들어온 것 확인
        if (!m_jobQueue.Empty())
        {
            bool expected = false;
            if (m_processing.compare_exchange_strong(expected, true))
            {
                m_pLogicThread->GetJobQueue()->Push(this);
            }
        }
    }

    LogicThread* GetLogicThread() { return m_pLogicThread; }
    void SetLogicThread(LogicThread* thread) { m_pLogicThread = thread; }
};
```

### 2.3 GameObject

```cpp
class GameObject
{
protected:
    GameScene* m_pScene = nullptr;

    virtual void OnEnter(GameScene* scene) {}
    virtual void OnExit(GameScene* scene) {}
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

    friend class GameScene;

public:
    GameScene* GetScene() { return m_pScene; }
};
```

### 2.4 GameScene

```cpp
class GameScene
{
protected:
    std::vector<GameObject*> m_objects;
    LogicThread* m_pLogicThread = nullptr;

    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

public:
    void Enter(GameObject* obj)
    {
        m_objects.push_back(obj);
        obj->m_pScene = this;
        obj->OnEnter(this);
    }

    void Exit(GameObject* obj)
    {
        obj->OnExit(this);
        obj->m_pScene = nullptr;
        std::erase(m_objects, obj);
    }

    void FixedUpdate()
    {
        OnFixedUpdate();
        for (auto& obj : m_objects)
            obj->OnFixedUpdate();
    }

    void Update()
    {
        OnUpdate();
        for (auto& obj : m_objects)
            obj->OnUpdate();
    }

    LogicThread* GetLogicThread() { return m_pLogicThread; }
    void SetLogicThread(LogicThread* thread) { m_pLogicThread = thread; }
};
```

### 2.5 LogicThread

```cpp
class LogicThread
{
    std::vector<GameScene*> m_scenes;
    JobObjectQueue m_jobQueue;

    float m_fixedTimeAccum = 0.0f;
    float m_fixedTimeStep = 0.02f;  // 50Hz
    bool m_running = true;

public:
    void AddScene(GameScene* scene)
    {
        m_scenes.push_back(scene);
        scene->SetLogicThread(this);
    }

    void RemoveScene(GameScene* scene)
    {
        scene->SetLogicThread(nullptr);
        std::erase(m_scenes, scene);
    }

    JobObjectQueue* GetJobQueue() { return &m_jobQueue; }

    void Run()
    {
        while (m_running)
        {
            float dt = CalcDeltaTime();

            // Time 갱신 (TLS)
            Time::SetDeltaTime(dt);
            Time::SetTime(Time::time() + dt);
            Time::SetFrameCount(Time::frameCount() + 1);
            Time::SetFixedDeltaTime(m_fixedTimeStep);

            m_fixedTimeAccum += dt;

            // ──────── 1. Job 처리 (패킷 핸들러) ────────
            while (auto* obj = m_jobQueue.Pop())
            {
                obj->Flush();
            }

            // ──────── 2. FixedUpdate ────────
            while (m_fixedTimeAccum >= m_fixedTimeStep)
            {
                for (auto& scene : m_scenes)
                    scene->FixedUpdate();

                m_fixedTimeAccum -= m_fixedTimeStep;
            }

            // ──────── 3. Update ────────
            for (auto& scene : m_scenes)
                scene->Update();

            // ──────── 4. 프레임 대기 ────────
            SleepUntilNextFrame();
        }
    }

private:
    float CalcDeltaTime();
    void SleepUntilNextFrame();
};
```

---

## 3. LogicThread 파이프라인

```
┌─────────────────────────────────────────────────────┐
│  1. Job 처리 (패킷 핸들러)                           │
│     - GameScene Enter/Exit                          │
│     - 게임 로직 요청                                 │
├─────────────────────────────────────────────────────┤
│  2. FixedUpdate (고정 간격, 여러 번 호출 가능)       │
│     - 물리 시뮬레이션                               │
│     - 전투 판정                                     │
├─────────────────────────────────────────────────────┤
│  3. Update (프레임당 1회)                           │
│     - 이동 처리                                     │
│     - AI 로직                                       │
│     - 일반 게임 로직                                │
└─────────────────────────────────────────────────────┘
```

---

## 4. 패킷 처리 흐름

```
네트워크 스레드                         LogicThread
      │                                      │
  패킷 수신                                  │
      │                                      │
  Player.PostJob(핸들러)                     │
      │                                      │
      └──→ JobQueue.Push(player) ──────────→ │
                                             │
                                   ┌─────────┴─────────┐
                                   │  1. Job 처리       │
                                   │  (패킷 핸들러 실행) │
                                   ├───────────────────┤
                                   │  2. FixedUpdate    │
                                   ├───────────────────┤
                                   │  3. Update         │
                                   └───────────────────┘
```

---

## 5. GameScene 이동 (스레드 변경)

```cpp
void Player::ChangeScene(GameScene* newScene)
{
    GameScene* oldScene = GetScene();
    LogicThread* oldThread = oldScene->GetLogicThread();
    LogicThread* newThread = newScene->GetLogicThread();

    // 1. 기존 Scene에서 Exit (현재 스레드에서)
    oldScene->Exit(this);

    // 2. 스레드가 다르면 변경
    if (oldThread != newThread)
    {
        SetLogicThread(newThread);
    }

    // 3. 새 Scene에 Enter (새 스레드에서 실행되도록 Job으로)
    PostJob([this, newScene]{
        newScene->Enter(this);
    });
}
```

**Flush에서 스레드 변경 감지**: Job 실행 중 `SetLogicThread`가 호출되면, Flush는 이를 감지하고 새 스레드에 자신을 등록한 후 즉시 종료합니다. 나머지 Job은 새 스레드에서 처리됩니다.

---

## 6. Game Layer 사용 예시

```cpp
// ═══════════════════ Game Layer ═══════════════════

// Player = GameObject + JobObject
class Player : public GameObject, public JobObject
{
protected:
    void OnEnter(GameScene* scene) override
    {
        // 맵 입장 처리
    }

    void OnExit(GameScene* scene) override
    {
        // 맵 퇴장 처리
    }

    void OnFixedUpdate() override
    {
        // 전투 판정
    }

    void OnUpdate() override
    {
        float dt = Time::deltaTime();
        // 이동 처리
    }

public:
    void HandleMovePacket(MovePacket& packet)
    {
        // 패킷 처리
    }
};

// NPC는 Job 필요 없으면 GameObject만
class NPC : public GameObject
{
    void OnUpdate() override
    {
        // AI 로직
    }
};

// BattleScene
class BattleScene : public GameScene
{
    void OnFixedUpdate() override
    {
        // 스폰 타이머
    }

    void OnUpdate() override
    {
        // 맵 이벤트
    }
};
```

---

## 7. 초기화 및 사용

```cpp
// 서버 초기화
LogicThread* thread1 = new LogicThread();
LogicThread* thread2 = new LogicThread();

BattleScene* scene1 = new BattleScene();
BattleScene* scene2 = new BattleScene();

thread1->AddScene(scene1);
thread2->AddScene(scene2);

// 스레드 시작
std::thread t1([&]{ thread1->Run(); });
std::thread t2([&]{ thread2->Run(); });

// 플레이어 입장
Player* player = new Player();
player->SetLogicThread(thread1);
scene1->Enter(player);

// 패킷 수신 시 (네트워크 스레드에서)
player->PostJob([player, packet]{
    player->HandleMovePacket(packet);
});
```

---

## 8. Job 처리 패턴 (Flag + Re-check)

Lost wakeup 문제를 방지하는 lock-free 패턴.

```cpp
// 생산자 (PostJob)
void PostJob(Job job)
{
    m_queue.Push(job);

    bool expected = false;
    if (m_processing.compare_exchange_strong(expected, true))
    {
        m_thread->Push(this);  // 처리 중이 아니었으면 등록
    }
}

// 소비자 (Flush) - 스레드 변경 감지 포함
void Flush()
{
    LogicThread* pOldThread = m_pLogicThread;  // 현재 스레드 저장

    do {
        while (auto job = m_queue.Pop())
        {
            job->Execute();

            // 스레드가 변경되었다면
            if (m_pLogicThread != pOldThread)
            {
                // 새 스레드에 등록하고 종료 (m_processing은 true 유지)
                m_pLogicThread->GetJobQueue()->Push(this);
                return;
            }
        }
    } while (!m_queue.Empty());

    m_processing.store(false);  // 처리 완료

    // 마지막 체크 - 그 사이에 들어온 것 확인
    if (!m_queue.Empty())
    {
        bool expected = false;
        if (m_processing.compare_exchange_strong(expected, true))
        {
            m_thread->Push(this);  // 놓친 것 있으면 다시 등록
        }
    }
}
```

### 스레드 변경 시 동작

| 상황 | 처리 |
|------|------|
| 스레드 변경됨 | 새 스레드에 Push, `m_processing = true` 유지, return |
| 스레드 유지 | 계속 처리, 끝나면 `m_processing = false` |

스레드 변경 시 `m_processing`을 false로 안 바꾸는 이유: 아직 Job이 남아있고, 새 스레드에서 계속 처리할 예정이므로 "처리 중" 상태를 유지해야 합니다.

### 장점 (vs Count +-1 패턴)

| 상황 | Count 패턴 | Flag 패턴 |
|------|-----------|-----------|
| Job 100개 추가 | fetch_add 100회 | CAS 1회 |
| Job 100개 소비 | fetch_sub 100회 | store 1회 |

---

## 9. 클래스 관계도

```
┌─────────────────────────────────────────────────────┐
│                      Core                           │
├─────────────────────────────────────────────────────┤
│                                                     │
│  LogicThread ─────┬───→ GameScene                   │
│       │           │         │                       │
│       │           │         └───→ GameObject        │
│       │           │                                 │
│       └───→ JobObjectQueue ───→ JobObject           │
│                                                     │
│  Time (TLS)                                         │
│                                                     │
└─────────────────────────────────────────────────────┘
                          │
                          │ 상속
                          ▼
┌─────────────────────────────────────────────────────┐
│                      Game                           │
├─────────────────────────────────────────────────────┤
│                                                     │
│  BattleScene : GameScene                            │
│                                                     │
│  Player : GameObject, JobObject                     │
│                                                     │
│  NPC : GameObject                                   │
│                                                     │
│  Monster : GameObject                               │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 10. 핵심 설계 원칙

1. **Unity 스타일 인터페이스**: Update() 인자 없음, Time 클래스로 접근
2. **역할 분리**: JobObject(패킷), GameObject(게임 로직), GameScene(컨테이너)
3. **프레임 동기화**: 같은 Scene의 오브젝트들은 같은 프레임에서 Update
4. **Job 먼저 처리**: Update 전에 패킷 핸들러 실행 (Enter/Exit 등)
5. **Scene이 LogicThread 소유**: Scene 이동 시 스레드도 함께 변경
6. **Flush에서 스레드 변경 감지**: Job 실행 중 스레드 변경 시 새 스레드에 등록 후 즉시 종료
