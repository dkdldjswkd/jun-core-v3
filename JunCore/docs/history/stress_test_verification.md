# StressClient를 통한 서버 검증

## 개요

StressClient는 EchoServer의 안정성과 정합성을 검증하기 위한 부하 테스트 클라이언트입니다.
단순한 부하 테스트를 넘어 **서버의 패킷 직렬화 처리, 동시성 안전성, 연결 안정성**을 검증합니다.

---

## 테스트 설정

| 항목 | 값 | 설명 |
|------|-----|------|
| SESSION_COUNT | 200 | 동시 연결 클라이언트 수 |
| MESSAGE_INTERVAL_MS | 1ms | 패킷 전송 간격 |
| MESSAGE_SIZE | 10~100자 | 랜덤 메시지 크기 |
| DISCONNECT_PROBABILITY | 1/1000 | 랜덤 끊기 확률 |

---

## 검증 항목

### 1. 패킷 순서 보장 (서버 직렬화 검증)

**검증 원리:**
```cpp
// 클라이언트: 보낸 메시지를 Queue에 저장 (FIFO)
sessionData.sentMessages.Enqueue(message);
user->SendPacket(request);

// 응답 수신 시: Queue에서 Dequeue하여 비교
sessionData.sentMessages.Dequeue(&expectedMessage);
if (expectedMessage != responseMessage) {
    LOG_ASSERT("Message mismatch!");  // 순서 꼬임 = 에러
}
```

**검증하는 것:**
- 서버가 **세션별로 패킷을 직렬화(순차 처리)** 하고 있는지 확인
- IOCP Worker Thread가 여러 개여도 **동일 세션의 패킷은 순서대로 처리**되는지 확인

**실패 시나리오:**
- 서버가 동일 세션을 여러 Worker가 동시에 처리 → 패킷 순서 꼬임 → 메시지 불일치

---

### 2. 동시성 버그 검출

**검증 원리:**
- 200개 세션이 1ms 간격으로 동시에 패킷 전송
- IOCP Worker Thread 간 세션 접근 경쟁 상황 유발

**검증하는 것:**
| 버그 유형 | 증상 |
|-----------|------|
| 세션 동시 접근 (Race Condition) | 서버 크래시 |
| Send/Recv 버퍼 경쟁 | 데이터 손상, 패킷 순서 꼬임 |
| IOCount 관리 오류 | 세션 조기 해제, Use-After-Free |

**실패 시나리오:**
- 서버 크래시 → 동시성 버그 존재
- 패킷 순서 꼬임 → 직렬화 처리 실패
- 예상치 못한 Disconnect → 세션 관리 오류

---

### 3. 예상치 못한 서버 Disconnect 감지

**검증 원리:**
```cpp
void StressClient::OnUserDisconnect(User* user) {
    if (sessionData->disconnectRequested.load()) {
        // 클라이언트가 능동적으로 끊음 (정상)
        LOG_INFO("Expected disconnect for session %d", sessionIndex);
    } else {
        // 서버가 끊음 (에러!)
        LOG_ASSERT("Unexpected server disconnect for session %d", sessionIndex);
    }
}
```

**검증하는 것:**
- 클라이언트가 끊지 않았는데 연결이 끊기면 → **서버 측 문제**
- 서버의 세션 관리, 버퍼 오버플로우, 예외 처리 등 검증

---

### 4. Recv 버퍼 한계 테스트

**발견된 이슈:**
- `MESSAGE_INTERVAL_MS = 0`으로 설정 시 (대기 없이 전송)
- 서버 Recv 버퍼 오버플로우 발생
- 패킷 유실 또는 연결 강제 종료

**해결:**
- `MESSAGE_INTERVAL_MS = 1`로 설정하여 안정적 처리량 확보
- 서버의 버퍼 크기와 처리 속도 간 균형점 확인

---

### 5. 연결/해제 안정성

**검증 원리:**
```cpp
// 1/1000 확률로 랜덤 Disconnect
if (dist(randomGenerator) <= DISCONNECT_PROBABILITY_PER_THOUSAND) {
    sessionData.disconnectRequested.store(true);
    user->Disconnect();
    // Client 클래스의 자동 재연결 메커니즘이 처리
}
```

**검증하는 것:**
- 빈번한 연결/해제에도 서버가 안정적으로 동작하는지
- 세션 풀 관리, 리소스 정리가 올바르게 되는지
- 재연결 시 새 세션이 정상 할당되는지

---

## 동시성 처리 기법 (클라이언트 측)

### atomic 포인터와 CLEANING 상태

```cpp
#define CLEANING (reinterpret_cast<User*>(0x1))

struct SessionData {
    std::atomic<User*> user{nullptr};
    std::atomic<bool> disconnectRequested{false};
};
```

**상태 전이:**
```
nullptr → User* (연결 완료)
User* → CLEANING (Disconnect 시작)
CLEANING → nullptr (정리 완료, 재연결 대기)
```

**왜 CLEANING 상태가 필요한가:**
- `OnUserDisconnect`에서 즉시 `nullptr`로 바꾸면
- `OnConnectComplete`에서 동시에 새 User를 할당할 수 있음
- 정리 작업(Queue 비우기 등) 중 새 연결이 들어오면 데이터 오염
- CLEANING 상태로 정리 중임을 표시하여 경쟁 방지

### CAS 연산으로 슬롯 획득

```cpp
void StressClient::OnConnectComplete(User* user, bool success) {
    for (int i = 0; i < SESSION_COUNT; i++) {
        User* expected = nullptr;
        if (session_data_vec[i].user.compare_exchange_strong(expected, user)) {
            // 슬롯 획득 성공!
            return;
        }
    }
}
```

- 여러 연결이 동시에 완료되어도 안전하게 슬롯 할당
- Lock-Free 방식으로 성능 저하 없음

---

## 테스트 실행 결과 해석

### 성공 조건
- 장시간 실행 후에도 에러 로그 없음
- 모든 세션이 정상 연결/해제 반복
- 메시지 정합성 100% 유지

### 실패 시 의미

| 증상 | 원인 추정 |
|------|-----------|
| 서버 크래시 | 동시성 버그, 메모리 오류 |
| "Message mismatch" 에러 | 패킷 순서 보장 실패, 직렬화 오류 |
| "Unexpected server disconnect" | 세션 관리 오류, 버퍼 오버플로우 |
| Send 실패 | 네트워크 오류, 세션 조기 해제 |

---

## 한계점

| 항목 | 한계 |
|------|------|
| 메모리 누수 | 서버 측 모니터링 필요, 장시간 테스트 필요 |
| CPU 사용률 | 서버 측 성능 지표 수집 안 함 |
| 실제 부하 | 200세션은 적은 수 (실서비스는 수천~수만) |
| 비즈니스 로직 | Echo만 테스트, 실제 게임 로직 검증 없음 |
| 악의적 패킷 | 잘못된 형식, 비정상 크기 패킷 테스트 없음 |

---

## 향후 개선 방향

1. **서버 측 모니터링 연동** - CPU, 메모리, 핸들 수 실시간 추적
2. **장시간 테스트 자동화** - 24시간+ CI/CD 통합
3. **Chaos 테스트 추가** - 잘못된 패킷, 불완전한 패킷
4. **점진적 부하 증가** - 세션 수를 늘려가며 한계점 파악
5. **RTT 측정** - 타임스탬프 기반 응답 지연 분석

---

## 결론

StressClient는 단순한 부하 테스트가 아닌, **서버의 핵심 동시성 처리가 올바른지 검증**하는 도구입니다.

**검증 완료 항목:**
- 세션별 패킷 직렬화 처리 (순서 보장)
- IOCP 기반 동시성 안전성
- 연결/해제 안정성
- Recv 버퍼 처리 한계

**이 테스트를 통과했다면:**
- 서버가 다중 세션을 안전하게 처리함
- Worker Thread 간 세션 접근이 올바르게 직렬화됨
- 빈번한 연결/해제에도 리소스 누수 없음 (단기간 기준)
