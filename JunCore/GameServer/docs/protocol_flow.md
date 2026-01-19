# 게임 서버 프로토콜 플로우

이 문서는 클라이언트-서버 간 로그인부터 Scene 진입까지의 프로토콜 플로우를 설명합니다.

---

## 1. 전체 플로우 개요

```
┌─────────────────────────────────────────────────────────────────────┐
│                        첫 로그인 플로우                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  [Client]                              [Server]                     │
│     │                                      │                        │
│     │─────── CG_LOGIN_REQ ────────────────►│                        │
│     │        (인증 정보)                    │                        │
│     │                                      │ Player ID 발급         │
│     │                                      │ 마지막 Scene/Pos 조회   │
│     │◄────── GC_LOGIN_RES ─────────────────│                        │
│     │        (player_id, scene_id,         │                        │
│     │         spawn_pos)                   │                        │
│     │                                      │                        │
│     │  [클라이언트 씬 로딩 시작]            │                        │
│     │  - scene_id로 씬 에셋 로드            │                        │
│     │  - spawn_pos 주변 리소스 준비         │                        │
│     │  [클라이언트 씬 로딩 완료]            │                        │
│     │                                      │                        │
│     │─────── CG_SCENE_READY_REQ ──────────►│                        │
│     │        (로딩 완료 알림)              │                        │
│     │                                      │ Player 객체 생성       │
│     │                                      │ Scene에 Enter          │
│     │◄────── GC_SCENE_ENTER_NOTIFY ────────│                        │
│     │        (scene_id, player_id,         │                        │
│     │         spawn_pos)                   │                        │
│     │                                      │                        │
│     │  [게임 플레이 시작]                   │                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. 맵 이동 플로우

```
┌─────────────────────────────────────────────────────────────────────┐
│                        맵 이동 플로우                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  [Client]                              [Server]                     │
│     │                                      │                        │
│     │─────── CG_SCENE_CHANGE_REQ ─────────►│                        │
│     │        (target_scene_id)             │                        │
│     │                                      │ 이동 조건 검증         │
│     │                                      │ 새 Scene spawn_pos 조회│
│     │◄────── GC_SCENE_CHANGE_RES ──────────│                        │
│     │        (scene_id, spawn_pos)         │                        │
│     │                                      │                        │
│     │  [클라이언트 새 씬 로딩 시작]         │ Player::MoveToScene()  │
│     │  - 새 scene_id로 씬 에셋 로드         │ - 현재 Scene Exit      │
│     │  - spawn_pos 주변 리소스 준비         │ - LogicThread 변경     │
│     │  [클라이언트 새 씬 로딩 완료]         │                        │
│     │                                      │                        │
│     │─────── CG_SCENE_READY_REQ ──────────►│                        │
│     │        (로딩 완료 알림)              │                        │
│     │                                      │ 새 Scene Enter         │
│     │◄────── GC_SCENE_ENTER_NOTIFY ────────│                        │
│     │        (scene_id, player_id,         │                        │
│     │         spawn_pos)                   │                        │
│     │                                      │                        │
│     │  [새 맵에서 게임 플레이]              │                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 패킷 정의

### 3.1 로그인 관련

```protobuf
// 클라이언트 → 서버: 로그인 요청
message CG_LOGIN_REQ {
    string token = 1;           // 인증 토큰
}

// 서버 → 클라이언트: 로그인 응답
message GC_LOGIN_RES {
    int32 player_id = 1;        // 발급된 Player ID
    int32 scene_id = 2;         // 진입할 Scene ID
    Pos spawn_pos = 3;          // 스폰 위치
}
```

### 3.2 Scene 진입 관련

```protobuf
// 클라이언트 → 서버: 씬 로딩 완료 알림
message CG_SCENE_READY_REQ {
    // 로딩 완료만 알림 (추가 데이터 불필요)
}

// 서버 → 클라이언트: Scene 진입 완료 알림
message GC_SCENE_ENTER_NOTIFY {
    int32 scene_id = 1;         // 진입한 Scene ID
    int32 player_id = 2;        // Player ID
    Pos spawn_pos = 3;          // 최종 스폰 위치
}
```

### 3.3 맵 이동 관련

```protobuf
// 클라이언트 → 서버: 맵 이동 요청
message CG_SCENE_CHANGE_REQ {
    int32 scene_id = 1;         // 이동할 Scene ID
}

// 서버 → 클라이언트: 맵 이동 승인
message GC_SCENE_CHANGE_RES {
    int32 scene_id = 1;         // 이동할 Scene ID
    Pos spawn_pos = 2;          // 새 Scene의 스폰 위치
}
```

---

## 4. 서버 측 핸들러 로직

### 4.1 HandleLoginRequest

```
1. 이미 로그인된 상태인지 확인
2. 인증 처리 (TODO: DB 조회)
3. Player ID 발급 (GameServer::GeneratePlayerId)
4. 마지막 Scene ID 조회 (TODO: DB 조회, 현재는 기본값 0)
5. 스폰 위치 조회 (TODO: DB 조회, 현재는 원점)
6. User에 정보 저장 (player_id, scene_id, spawn_pos)
7. GC_LOGIN_RES 전송
```

### 4.2 HandleSceneReadyRequest

```
1. 로그인 확인 (player_id != 0)
2. 이미 Player가 있는 경우 (맵 이동 후 로딩 완료)
   - 현재는 첫 진입만 지원
3. 첫 진입인 경우
   - User에서 player_id, scene_id 조회
   - Scene 검증
   - Player 객체 생성 (GameObject::Create<Player>)
   - User에 Player 연결
   - Player::OnEnter에서 GC_SCENE_ENTER_NOTIFY 전송
```

### 4.3 HandleSceneChangeRequest

```
1. Player 존재 확인
2. Scene 검증
3. 이동 조건 검증 (TODO: 레벨, 퀘스트 등)
4. 새 Scene의 스폰 위치 조회 (TODO: DB)
5. User에 새 Scene 정보 저장
6. GC_SCENE_CHANGE_RES 전송
7. Player::MoveToScene 호출
```

---

## 5. Player Scene 이동 메커니즘

### 5.1 MoveToScene 구현

```cpp
void Player::MoveToScene(GameScene* new_scene)
{
    // Job #1: 현재 Scene에서 Exit + LogicThread 변경
    PostJob([this, new_scene]() {
        if (scene_) {
            scene_->Exit(this);
            OnExit(scene_);
        }
        scene_ = new_scene;
        SetLogicThread(new_scene->GetLogicThread());
    });

    // Job #2: 새 Scene에서 Enter (새 LogicThread에서 실행)
    PostJob([this, new_scene]() {
        new_scene->Enter(this);
        OnEnter(new_scene);
    });
}
```

### 5.2 동작 원리

1. **Job #1 실행**: 현재 LogicThread에서 Exit 처리
2. **SetLogicThread 호출**: m_pLogicThread 변경
3. **JobObject::Flush()**: LogicThread 변경 감지 → 새 스레드에 JobObject 등록
4. **Job #2 실행**: 새 LogicThread에서 Enter 처리

---

## 6. 설계 원칙

### 6.1 SCENE_READY_REQ 패턴

**왜 클라이언트가 READY를 보내야 하는가?**

- 클라이언트 씬 로딩은 시간이 걸림 (에셋 로드, 인스턴스화 등)
- 서버가 Player를 먼저 생성하면 로딩 중 패킷 손실 가능
- 클라이언트가 준비 완료 후 READY를 보내면 서버가 Player 생성
- 이 패턴은 대부분의 MMORPG에서 사용하는 표준 방식

### 6.2 LOGIN_RES에 spawn_pos 포함

**왜 로그인 응답에 스폰 위치를 주는가?**

- 클라이언트가 씬 로딩 전에 필요한 정보를 미리 알 수 있음
- 카메라 위치 설정, 주변 리소스 프리로드 가능
- 네트워크 왕복 횟수 감소

### 6.3 통합된 SCENE_READY_REQ

**왜 GAME_START_REQ와 SCENE_ENTER_REQ를 분리하지 않는가?**

- 첫 로그인과 맵 이동 모두 "씬 로딩 완료 후 진입"이라는 동일한 흐름
- 하나의 SCENE_READY_REQ로 통일하면 코드 중복 감소
- 클라이언트/서버 모두 단순해짐

---

## 7. 에러 처리

### 7.1 에러 코드

```protobuf
enum ErrorCode {
    SUCCESS = 0;
    AUTH_FAILED = 1;           // 인증 실패
    ALREADY_LOGGED_IN = 2;     // 이미 로그인됨
    NOT_LOGGED_IN = 3;         // 로그인 안됨
    PLAYER_NOT_FOUND = 4;      // Player 객체 없음
    SCENE_NOT_FOUND = 5;       // Scene 없음
}
```

### 7.2 에러 처리 플로우

- 모든 에러는 GC_ERROR_NOTIFY로 전송
- 에러 발생 시 현재 상태 유지 (롤백 없음)
- 클라이언트는 에러 수신 시 적절한 UI 표시

---

## 8. TODO

- [ ] DB 연동: 마지막 Scene ID, 스폰 위치 조회
- [ ] 맵 이동 조건 검증 (레벨, 퀘스트 등)
- [ ] 맵 이동 후 SCENE_READY_REQ 처리 구현
- [ ] 주변 플레이어 정보 동기화 (PLAYER_APPEAR_NOTIFY)
- [ ] 재접속 처리 로직
