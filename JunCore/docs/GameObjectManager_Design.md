# GameObjectManager 설계 문서

## 개요

GameObjectManager는 모든 GameObject의 생성, 조회, 크로스 스레드 통신을 담당하는 코어 레벨 매니저입니다.

**구현 완료**: `JunCore/logic/GameObjectManager.h`

## 왜 필요한가?

### 문제 상황

```
Player A (Scene 1, LogicThread 1)
    ↓ 귓속말
Player B (Scene 2, LogicThread 2)
```

- 다른 Scene/Thread에 있는 GameObject에 접근해야 하는 경우
- 포인터 직접 참조 시 생명주기 문제 (삭제된 객체 접근 위험)
- 각 타입별 Manager (PlayerManager, MonsterManager...) 만들면 중복 코드

### 해결책

- **Serial Number(SN)** 기반 전역 GameObject 관리
- **팩토리 패턴**으로 생성 통일
- **PostTo** 패턴으로 크로스 스레드 안전한 통신
- **Event 구독**으로 삭제 시 자동 정리

---

## 왜 코어에서 지원하는가?

| 이유 | 설명 |
|------|------|
| 게임 로직과 무관 | SN 발급, 등록/조회는 인프라 레벨 기능 |
| 모든 게임에서 동일 | 타입 상관없이 동일한 패턴 |
| 크로스 스레드 통신 표준화 | PostTo 패턴 통일 |
| 생명주기 관리 통합 | 삭제 시 Event로 자동 해제 |

---

## 구현된 구조

### GameObjectManager 클래스

```cpp
class GameObjectManager
{
private:
    std::unordered_map<uint64_t, GameObject*> m_objects;
    std::unordered_map<uint64_t, Subscription> m_subscriptions;
    mutable std::shared_mutex m_mutex;
    std::atomic<uint64_t> m_nextSN{1};

public:
    static GameObjectManager& Get();  // 싱글톤

    // 팩토리: 생성 + SN 부여 + 등록 + Event 구독
    template<typename T, typename... Args>
    T* Create(GameScene* scene, Args&&... args);

    // 조회 (SN으로 GameObject 찾기)
    GameObject* Find(uint64_t sn);

    // 타입 캐스팅 조회
    template<typename T>
    T* Find(uint64_t sn);

    // 크로스 스레드 Job 전달
    bool PostTo(uint64_t sn, Job job);

    // 등록 해제 (OnBeforeDestroy에서 자동 호출)
    void Unregister(uint64_t sn);
};
```

### 핵심 특징

1. **Event 자동 구독**: 생성 시 `OnBeforeDestroy` 이벤트 구독하여 삭제 시 자동 정리
2. **스레드 안전**: `shared_mutex`로 읽기/쓰기 분리
3. **삭제 마킹 체크**: `Find()` 시 삭제 마킹된 객체는 null 반환

---

## JobObject 삭제 정책

### MarkForDelete 패턴

```cpp
// JobObject.h
class JobObject {
    std::atomic<bool> m_markedForDelete{false};

public:
    void MarkForDelete();           // 삭제 마킹
    bool IsMarkedForDelete() const; // 상태 확인
    bool PostJob(Job job);          // 삭제 마킹 시 false 반환
};
```

### 삭제 흐름

```
1. MarkForDelete() 호출 → m_markedForDelete = true
2. 이후 PostJob 시도 → 거부됨 (false 리턴)
3. 기존에 쌓인 Job들 모두 처리
4. Flush() 완료 → 삭제 큐에 등록
5. 프레임 끝에서 실제 delete
```

### LogicThread 삭제 큐

```cpp
class LogicThread {
    std::vector<JobObject*> m_deleteQueue;

    void Run() {
        while (m_running.load()) {
            // 1. Job 처리
            // 2. FixedUpdate
            // 3. Update
            // 4. 삭제 큐 처리 (프레임 끝)
            ProcessDeleteQueue();
            // 5. 프레임 대기
        }
    }

    void ProcessDeleteQueue() {
        for (auto* obj : m_deleteQueue)
            delete obj;
        m_deleteQueue.clear();
    }
};
```

---

## 참조 방식 정리

### 같은 Scene/LogicThread 내

**포인터 + Event 구독**

```cpp
class Monster {
    Player* m_target;
    Subscription m_targetDestroySub;

    void SetTarget(Player* player) {
        m_target = player;
        m_targetDestroySub = player->OnBeforeDestroy.Subscribe([this]() {
            m_target = nullptr;
        });
    }
};
```

- 같은 스레드라 동시성 문제 없음
- Event로 삭제 알림 받음
- RAII Subscription으로 자동 해제

### 다른 Scene/LogicThread 간

**SN + PostTo**

```cpp
// 귓속말 전송
void Player::SendWhisper(uint64_t targetSN, const std::string& msg) {
    GameObjectManager::Get().PostTo(targetSN, [msg]() {
        // 해당 Player의 LogicThread에서 실행
        ReceiveWhisper(msg);
    });
}
```

- SN으로 조회
- 없으면 무시 (삭제된 경우)
- PostJob으로 대상 스레드에서 실행

---

## 사용 예시

### GameObject 생성

```cpp
// GameObjectManager 팩토리 사용 (SN 자동 부여)
Player* player = GameObjectManager::Get().Create<Player>(scene, &user, playerId);
Monster* monster = GameObjectManager::Get().Create<Monster>(scene, monsterId);
```

### SN 기반 조회

```cpp
// 공격 대상 조회
void Player::HandleAttack(uint64_t targetSN) {
    auto* target = GameObjectManager::Get().Find<Player>(targetSN);
    if (!target) return;  // 없으면 무시

    target->PostJob([target]() {
        target->TakeDamage(10);
    });
}
```

### 크로스 스레드 통신

```cpp
// 귓속말
GameObjectManager::Get().PostTo(targetSN, [msg]() {
    ReceiveWhisper(msg);
});

// 파티 초대
GameObjectManager::Get().PostTo(targetSN, [inviterSN]() {
    ReceivePartyInvite(inviterSN);
});
```

### Disconnect 처리

```cpp
void User::OnDisconnect() {
    if (m_player) {
        m_player->Destroy();  // OnBeforeDestroy 발행 → MarkForDelete
        m_player = nullptr;
    }
}

// GameObject::Destroy() 내부
void GameObject::Destroy() {
    if (m_pScene)
        m_pScene->Exit(this);

    OnBeforeDestroy.Invoke();  // 구독자들에게 알림
    MarkForDelete();           // 삭제 마킹
}
```

---

## 관련 파일

| 파일 | 설명 |
|------|------|
| `JunCore/logic/GameObjectManager.h` | GameObjectManager 구현 |
| `JunCore/logic/GameObject.h` | SN, OnBeforeDestroy, Destroy() 추가됨 |
| `JunCore/logic/JobObject.h` | MarkForDelete 패턴 구현 |
| `JunCore/logic/LogicThread.h` | 삭제 큐 추가 |
| `JunCore/core/Event.h` | Event/Subscription 시스템 |

---

## 설계 원칙 요약

1. **SN 기반 전역 조회**: 어디서든 SN으로 GameObject 접근 가능
2. **Event로 자동 정리**: 삭제 시 GameObjectManager에서 자동 제거
3. **MarkForDelete 패턴**: 삭제 마킹 후 PostJob 거부, 기존 Job은 모두 처리
4. **프레임 끝 삭제**: LogicThread가 프레임 끝에서 일괄 삭제
5. **스레드 안전**: 크로스 스레드 접근은 PostTo로만
