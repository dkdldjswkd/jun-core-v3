# Job 시스템 아키텍처

## 핵심 원리: JobObject 중심 설계

**가장 중요한 개념**:
- **Job은 JobObject에 속함** (LogicThread에 속하는 것이 아님!)
- **JobObject가 어느 LogicThread에 있는지에 따라 Job 실행 위치가 결정됨**
- **LogicThread 변경 시 JobObject가 자동으로 새 스레드로 이동**

## Job 시스템 동작 흐름

```
1. PostJob(job) 호출
   └→ JobObject의 m_jobQueue에 Job 추가
   └→ JobObject를 현재 LogicThread의 처리 큐에 등록

2. LogicThread::Run()
   └→ 처리할 JobObject 꺼내기
   └→ JobObject::Flush() 호출

3. JobObject::Flush()
   └→ m_jobQueue에서 Job 하나씩 꺼내서 실행
   └→ 매 Job 실행 후 LogicThread 변경 확인
   └→ 변경되었으면 새 LogicThread에 JobObject 등록하고 종료

4. 새 LogicThread
   └→ 이동된 JobObject의 남은 Job들을 계속 처리
```

## 주요 클래스

### JobObject (JunCore/logic/JobObject.h)

```cpp
class JobObject
{
protected:
    LFQueue<Job> m_jobQueue;           // 이 객체에 쌓인 Job들
    std::atomic<bool> m_processing;     // 처리 중 플래그 (CAS용)
    LogicThread* m_pLogicThread;        // 소속 스레드

public:
    void PostJob(Job job);   // Job 추가 (네트워크 스레드에서 호출)
    void Flush();            // Job 처리 (LogicThread가 호출)
};
```

### LogicThread (JunCore/logic/LogicThread.h)

```cpp
class LogicThread
{
private:
    LFQueue<JobObject*> m_jobQueue;     // 처리할 JobObject들
    std::vector<GameScene*> m_scenes;   // 관리하는 Scene들

    // 메인 루프:
    // 1. Job 처리 (JobObject::Flush)
    // 2. FixedUpdate (50Hz)
    // 3. Update (매 프레임)
};
```

## JobObject::Flush() - 핵심 구현

```cpp
void JobObject::Flush()
{
    LogicThread* pOldThread = m_pLogicThread;  // 현재 스레드 저장

    Job job;
    while (m_jobQueue.Dequeue(&job))
    {
        job();  // Job 실행

        // 핵심: 매 Job 실행 후 스레드 변경 확인
        if (m_pLogicThread != pOldThread)
        {
            // 새 스레드에 등록하고 즉시 종료
            // m_processing은 true 유지 (아직 처리 중)
            m_pLogicThread->GetJobQueue()->Enqueue(this);
            return;  // 현재 스레드에서는 더 이상 처리하지 않음
        }
    }

    // 처리 완료
    m_processing.store(false);

    // Lost Wakeup 방지: Flush 완료 직후 새로운 Job이 들어왔는지 체크
    if (m_jobQueue.GetUseCount() > 0)
    {
        bool expected = false;
        if (m_processing.compare_exchange_strong(expected, true))
        {
            m_pLogicThread->GetJobQueue()->Enqueue(this);
        }
    }
}
```

**중요 포인트**:
- Job 실행 중 `SetLogicThread()`가 호출되면 `m_pLogicThread` 변경
- 변경 감지 시 즉시 새 스레드에 JobObject 등록하고 현재 스레드는 처리 중단
- 남은 Job들은 새 스레드에서 계속 처리됨

## GameObject와 Job 시스템

### GameObject는 JobObject 상속

```cpp
class GameObject : public Entity, public JobObject
{
protected:
    GameScene* m_pScene = nullptr;

    explicit GameObject(GameScene* scene)
        : JobObject(scene->GetLogicThread())  // Scene에서 LogicThread 가져옴
        , m_pScene(scene)
    {
    }
};
```

**설계 원칙**:
- GameObject는 **LogicThread를 직접 알지 못함**
- GameObject는 오직 **GameScene에만 관심**
- LogicThread는 GameScene의 구현 디테일

### Factory 패턴으로 안전한 생성

```cpp
// Factory 패턴 사용 (안전)
template<typename T, typename... Args>
static T* Create(GameScene* scene, Args&&... args)
{
    // 1. 객체 생성 (생성자 완료)
    T* obj = new T(scene, std::forward<Args>(args)...);

    // 2. Scene에 Enter Job 등록 (완전히 초기화된 객체 사용)
    obj->PostJob([obj, scene]() {
        scene->Enter(obj);
        obj->OnEnter();
    });

    return obj;
}

// 사용 예시
Player* player = GameObject::Create<Player>(lobbyScene, user, playerId);
```

**왜 Factory 패턴?**
- 생성자에서 `this`를 Job으로 넘기면 파생 클래스 생성 전일 수 있음
- Factory는 완전히 초기화된 객체만 Job에 넣음

### Scene 이동 구현 (MoveToScene)

```cpp
void GameObject::MoveToScene(GameScene* newScene)
{
    if (newScene == nullptr)
        return;

    // Job #1: Exit + LogicThread 변경
    PostJob([this, newScene]() {
        if (m_pScene == newScene)
            return;

        // 이전 Scene에서 Exit
        if (m_pScene)
        {
            m_pScene->Exit(this);
            OnExit();
        }

        // Scene 포인터 업데이트
        m_pScene = newScene;

        // LogicThread 변경
        SetLogicThread(newScene->GetLogicThread());
        // ↑ 이 시점에서 m_pLogicThread 변경됨
    });

    // Job #2: Enter (새로운 LogicThread에서 실행)
    // 주의: Job #1에서 LogicThread 변경 시 JobObject::Flush()가
    //       자동으로 이 JobObject를 새 LogicThread로 이동시킴
    //       따라서 이 Job은 새 LogicThread에서 실행됨
    PostJob([this, newScene]() {
        newScene->Enter(this);
        OnEnter();
    });
}
```

**핵심 메커니즘**:
1. Job #1 실행 → `SetLogicThread(newThread)` 호출
2. `Flush()`가 LogicThread 변경 감지
3. JobObject를 새 LogicThread에 등록하고 종료
4. Job #2는 새 LogicThread에서 실행됨

**왜 Job을 2개로 나눴는가?**
- Job #1: 현재 LogicThread에서 Exit 처리
- LogicThread 변경 (자동 이동)
- Job #2: 새 LogicThread에서 Enter 처리
- 스레드 안전성 보장!

## Job 시스템 사용 예시

### 패킷 핸들러를 Job으로 처리

```cpp
// NetworkThread에서 패킷 수신
void OnPacketReceived(User* user, const game::MoveRequest& req)
{
    Player* player = user->GetPlayer();

    // LogicThread에서 처리되도록 Job 등록
    player->PostJob([player, req]() {
        // 이 코드는 player의 LogicThread에서 실행됨
        player->SetDestPos(req.dest_pos());
    });
}
```

### Scene 전환과 LogicThread 이동

```cpp
// Player가 던전 입장
dungeonScene->SetLogicThread(dungeonLogicThread);  // 던전 전용 스레드

// Scene 이동 (자동으로 LogicThread도 이동)
player->MoveToScene(dungeonScene);
// ↑ player는 이제 dungeonLogicThread에서 처리됨
```

## Job 시스템 vs 기존 Thread Pool

| 항목 | Job 시스템 | Thread Pool |
|------|-----------|-------------|
| Job 소속 | JobObject에 속함 | Queue에 속함 |
| 스레드 이동 | JobObject 이동으로 자동 | 불가능 |
| 순서 보장 | JobObject별 순서 보장 | 보장 안 됨 |
| Actor 모델 | 완벽히 지원 | 미지원 |
| Scene 이동 | 자동 처리 | 수동 처리 필요 |

## 주의사항

1. **Job은 JobObject에 넣는 것!**
   - LogicThread에 Job을 직접 넣지 않음
   - JobObject에 PostJob() 호출

2. **LogicThread 변경 시 자동 이동**
   - SetLogicThread() 호출 시
   - Flush()가 자동으로 새 스레드에 JobObject 등록
   - 남은 Job들은 새 스레드에서 실행

3. **Factory 패턴 사용 권장**
   - 생성자에서 this 사용 시 파생 클래스 초기화 전
   - Factory로 완전한 초기화 보장

4. **Scene 이동은 Job으로**
   - MoveToScene() 사용
   - 직접 Exit/Enter 호출하지 말 것
   - 스레드 안전성 보장

---

## TODO: JobObject 생명주기 관리 (미구현)

### 현재 문제점

**JobObject는 생성은 명확하지만 삭제가 불명확함**

#### 문제 1: delete 시점
```cpp
player->PostJob([player]() {
    player->GetScene()->Exit(player);
    delete player;  // 위험! Flush()가 Job 실행 후 player에 접근
});
```

**왜 안되나**: `JobObject::Flush()`가 Job 실행 후 `m_pLogicThread` 확인 시 이미 delete된 메모리 접근

#### 문제 2: 일회용 JobObject
```cpp
// 1회용 작업 후 자동 삭제?
auto* job = new OneTimeJob();
job->PostJob([job]() {
    // 작업 실행
    // 삭제는 누가? 언제?
});
```

### 해결 방안 (미정)

#### 옵션 1: shared_ptr 기반 관리 (추천)
```cpp
class JobObject : public std::enable_shared_from_this<JobObject> {
    // ref counting 자동 관리
};

auto player = std::make_shared<Player>(...);
player->PostJob([player]() {  // shared_ptr 캡처로 생명주기 보장
    player->GetScene()->Exit(player.get());
});
// Job 완료 후 자동 삭제
```

**장점**:
- 자동 생명주기 관리
- 스레드 안전
- 표준 라이브러리 사용

**단점**:
- 기존 코드 대대적 수정 필요
- raw pointer와 혼용 시 위험

#### 옵션 2: 수동 ref counting
```cpp
class JobObject {
    std::atomic<int> refCount_{1};
public:
    void AddRef() { refCount_++; }
    void Release() { if (--refCount_ == 0) delete this; }
};

// 사용
player->AddRef();
player->PostJob([player]() {
    // ...
    player->Release();
});
```

#### 옵션 3: Owner 기반 관리
```cpp
// GameObject는 Scene이 소유 및 삭제
// 일회용 JobObject는 명시적 정책 (예: Job 완료 후 자동 삭제 플래그)
class JobObject {
    bool autoDelete_ = false;
public:
    void SetAutoDelete(bool enable) { autoDelete_ = enable; }
};
```

### 결정해야 할 사항

1. **GameObject 삭제 정책**: Scene이 소유? shared_ptr?
2. **일회용 JobObject 삭제**: 자동? 수동? ref counting?
3. **기존 코드 마이그레이션**: shared_ptr 전환? 점진적 적용?

**우선순위**: High - 메모리 누수 및 댕글링 포인터 위험
