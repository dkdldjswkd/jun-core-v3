* 대표 클래스
  IOCPManager
  - IOCP 핸들과 다중 WorkerThread를 관리한다.
  - 등록된 Socket들의 네트워크 이벤트를 처리한다.
  - Recv 이벤트 발생 시 패킷을 조립하여 해당 Session의 NetBase 엔진으로 전달한다.
  - 순수 I/O 이벤트 처리만 담당하며, 비즈니스 로직은 Engine에게 위임한다.
  - WorkerThread 수 설정 기능을 제공한다.

  NetBase
  - Protobuf 기반 패킷 처리 시스템을 제공한다.
  - 패킷 핸들러 등록 기능을 지원한다.
  - 패킷 수신 시 등록된 핸들러가 자동 역직렬화 후 호출된다.

  Server : NetBase
  - StartServer() 호출 시 listen socket 생성 및 Accept 전용 스레드를 시작한다.
  - 미리 할당된 세션 배열과 락프리 세션 인덱스 스택을 활용한 세션 풀링을 제공한다.
  - 세션 생명주기 이벤트인 OnSessionConnect, OnSessionDisconnect 가상함수를 제공한다.

  Client : NetBase
  - 단일 서버 연결 정책: 하나의 서버에 대해서만 연결 가능하며, 생성자에서 서버 IP/Port를 설정한 후 불변이다.
  - Connect() 함수 제공: 호출 시 새로운 세션을 생성하고 서버에 연결한다.
  - 다중 세션 관리: Connect() 함수를 반복 호출하여 하나의 서버에 여러 세션을 연결할 수 있다.

  Session
  - IOCount 기반 생명주기: 비동기 I/O 작업 추적을 통한 안전한 세션 관리로, IOCount가 0이 되면 해당 스레드에서 자동으로 세션을 정리한다.
  - Lock-Free 송신 지원: LFQueue를 사용하여 멀티스레드 환경에서 락 없는 Send 함수를 제공한다.
  - 자신이 속한 NetBase 엔진 포인터를 보유한다.

* 주요 특징
  Lock-Free 네트워크 코어 :  고성능 멀티스레드 환경을 위한 락 없는 데이터 구조 기반 네트워크 시스템
  유연한 IOCP 리소스 관리 모델
    - 엔진별 IOCPManager 선택권: 각 Engine(Server/Client)마다 독립 또는 공유 IOCPManager 사용이 가능하다.
    - 워커 스레드 최적화: 엔진마다 별도 워커 생성 시 발생하는 과도한 스레드와 컨텍스트 스위칭 오버헤드를 방지한다.
    - 리소스 분리 전략:
      - 전체 공유: 모든 Engine이 단일 IOCPManager를 공유 (최대 효율성)
      - 역할별 분리: Server 전용 + Client들 공유 IOCPManager (성능과 효율성 균형)
      - 완전 분리: 각 Engine별 독립 IOCPManager (최대 격리)