# AOI (Area of Interest) 시스템 설계

## 배경 및 목적

### 왜 필요한가
현재 브로드캐스트 구조:
```
1명 행동 → Scene의 모든 플레이어에게 패킷 전송 (O(N))
N명 동시 행동 → N × N = N² 패킷
```
6000명 기준 36,000,000 패킷/틱 → 불가능.

AOI로 브로드캐스트 대상을 인접 구역으로 한정하여 해결.

---

## 논의된 방식들

### Grid 방식 (채택)
맵을 격자로 분할, 인접 9셀에만 브로드캐스트.
단순하고 검증된 방식. 소규모 맵에 적합.

### Interest Bubble
플레이어 중심 반경 r 이내 플레이어만 관리.
매 틱 거리 계산 필요 → Grid로 후보군 축소 후 거리 계산하는 혼합 방식도 있음.
현재 맵 크기(30×30)에서는 Grid만으로 충분.

### Hysteresis (채택)
경계에서 깜빡임 방지. 깊이 기준으로 구현:
```
진입: 셀 경계 안쪽 버퍼 이상 들어와야 셀 이동 인정
이탈: 셀 경계 바깥쪽 버퍼 이상 나가야 셀 이탈 인정
```
섹터 크기를 늘리는 것과 다름 - 섹터 크기는 그대로 유지하면서 경계 판정만 늦춤.
나중에 시간 기준 추가 확장 가능 (타이머 추가).

---

## 최종 설계

### 클래스 구조
```
GameObject
├── float x, y, z                          (위치 소유)
├── SetPosition(x, y, z)                   (→ Scene 콜백)
├── virtual OnAppear(vector<GameObject*>&) {}
└── virtual OnDisappear(vector<GameObject*>&) {}

GameScene
├── AoiGrid                                (생성자 파라미터로 크기 결정)
└── OnObjectMoved(obj, x, z)               (→ AoiGrid 위임)

AoiGrid
├── Cell[n][n]                             (각 셀이 오브젝트 목록 보유)
├── UpdatePosition(obj, x, z)              (셀 추가/제거 + Hysteresis 판정)
│       → obj->OnAppear() / OnDisappear() 호출
└── GetNearbyObjects(x, z)                 (인접 9셀 오브젝트 반환)

MoveComponent
├── 목표 위치, 이동 속도
└── OnFixedUpdate → GameObject::SetPosition 호출
```

### 의존성 방향
```
MoveComponent → GameObject (SetPosition)
GameObject → GameScene (OnObjectMoved 콜백)
GameScene → AoiGrid (UpdatePosition, GetNearbyObjects)
AoiGrid → GameObject (OnAppear/OnDisappear 가상함수 호출)
```

### 책임 분리
- **MoveComponent**: 이동 로직만 (목표 위치, 이동 속도, 틱 처리)
- **GameObject**: 위치 소유, Scene에 위치 변경 알림
- **GameScene**: AoiGrid 소유, 오브젝트 위치 변경 전달
- **AoiGrid**: 공간 분할, 셀 추가/제거, Hysteresis 판정, 인접 오브젝트 반환
- **Player (사용자)**: OnAppear/OnDisappear 오버라이드하여 패킷 전송

---

## 설계 결정 사항

| 항목 | 결정 | 이유 |
|------|------|------|
| Grid vs Interest Bubble | Grid | 맵 크기 30×30, 단순함 |
| Hysteresis 기준 | 깊이 기준 | 타이머 불필요, 구현 단순 |
| Grid 크기 결정 | 생성자 파라미터 | 씬마다 맵 크기 다를 수 있음 |
| Grid 업데이트 시점 | SetPosition 즉시 | 위치와 Grid 항상 동기화 |
| APPEAR/DISAPPEAR 처리 | GameObject 가상함수 | Scene보다 오브젝트가 자신의 반응 처리하는 게 자연스러움 |
| interest_set 별도 관리 | 불필요 | Grid Cell이 오브젝트 목록 소유 |
| 위치 소유 | GameObject | MoveComponent 없는 오브젝트도 위치 가질 수 있음 |

---

## 작업 계획

### Phase 1: 코어 (JunCore)
- [x] `GameObject`에 x, y, z 추가 및 `SetPosition` 구현
- [x] `GameObject`에 `OnAppear` / `OnDisappear` 가상함수 추가 (public)
- [x] `AoiGrid.h` 신규 생성 (헤더 전용, Hysteresis 깊이 기준 구현)
- [x] `GameScene`에 AoiGrid 추가, 생성자 파라미터 추가, `OnObjectMoved` 구현
- [x] `MoveComponent` 수정 - `GameObject::SetPosition` 호출하도록
- [x] vcxproj 업데이트 (AoiGrid.h 추가)

### Phase 2: 게임 서버 (GameServer)
- [x] `Player`에 `OnAppear` / `OnDisappear` 오버라이드
- [x] 기존 브로드캐스트를 `GetNearbyObjects()` 기반으로 교체
- [x] `GC_PLAYER_APPEAR_NOTIFY` / `GC_PLAYER_DISAPPEAR_NOTIFY` 패킷 처리
- [x] `OnEnter`/`OnExit`에서 직접 브로드캐스트 제거 (AoiGrid 이벤트로 대체)
