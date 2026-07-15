# PRD.md — DummyDataGenerator

## 1. 개요

`DummyDataGenerator`는 "반도체 시료 생산주문관리 시스템"(S-Semi SampleOrderSystem) 과제의 PoC 산출물 중 하나로, **Test 를 위한 Dummy Data 를 생성하는 독립 모듈**이다. 단독 실행 프로그램이 아니라, 같은 과제의 다른 PoC 저장소(`ConsoleMVC`, `DataPersistence`, `DataMonitor`, `SampleOrderSystem`)에서 include/link 하여 재사용하는 **라이브러리 모듈**로 개발한다.

참고 문서: `../ref/requirements.pdf` ("반도체 시료 생산주문관리 시스템" 과제 요구사항), `../CLAUDE.md`, `dataModel/`(실제 데이터 모델), `storedData/`(그 모델을 저장한 결과 예시), `../DataPersistence/docs/PRE.md`.

## 2. 배경

S-Semi는 반도체 시료를 생산하여 연구소·팹리스·대학 연구실에 납품하는 가상 회사다. 주문 급증으로 엑셀/메모장 기반 관리가 한계에 부딪혀 "반도체 시료 생산주문관리 시스템"을 콘솔 기반으로 개발하기로 했다. 이 시스템은 시료 등록, 주문 접수/승인/거절, 생산라인, 모니터링, 출고 처리 기능으로 구성되며, 개발 과정에서 PoC 단계(MVC 스켈레톤, 데이터 영속성, 데이터 모니터링 Tool, Dummy 데이터 생성 Tool)를 먼저 검증한다.

`DummyDataGenerator`는 이 중 "Dummy 데이터 생성 Tool" PoC에 해당하며, 다른 PoC/본 프로젝트의 테스트 픽스처(시료 목록, 주문 목록 등)를 자동으로 채워주는 역할을 담당한다.

## 3. 목적 / 문제 정의

- 시료·주문 데이터를 수작업으로 준비하지 않고도, 다양한 상태 분포(재고 여유/부족/고갈, 주문 상태별 분포)를 가진 테스트 데이터를 즉시 생성할 수 있어야 한다.
- 모니터링, 생산라인, 출고 처리 등 각 기능이 실데이터 없이도 다양한 시나리오로 검증될 수 있어야 한다.
- 생성기는 SampleOrderSystem 본 프로젝트뿐 아니라 DataPersistence, DataMonitor 등 다른 PoC 저장소에서도 동일하게 재사용 가능해야 한다.

## 4. 대상 사용자

- 이 과제를 수행하는 개발자(본인) — 각 PoC/본 프로젝트의 테스트 코드 및 수동 검증 시 이 모듈을 호출.
- 향후 동일 도메인(시료/주문)을 다루는 다른 모듈 개발자.

## 5. 범위

이 프로젝트는 **PoC**다. 범용적으로 도메인을 감춘 추상 모듈이 아니라, `requirements.pdf`가 정의하는 시료/주문 도메인을 그대로 다루는 Dummy 데이터 생성 Tool을 만든다.

### 포함
- `Sample` 더미 데이터 생성 (`dataModel/Sample.h` 기준)
- `Order` 더미 데이터 생성 (`dataModel/Order.h` 기준, `Sample` 참조 무결성 포함)
- `ProductionQueueEntry` 더미 데이터 생성 (`dataModel/ProductionQueueEntry.h` 기준, `Order`/`Sample` 참조 및 부족분·실생산량·생산시간 계산 포함)
- 생성 수량, 값 범위, 상태 분포, 시드(seed)를 조정할 수 있는 공개 API
- 생성된 더미 데이터를 `storedData/`와 동일한 스키마로 연결된 저장소(DataPersistence 모듈의 CRUD API 등)에 실제로 추가하는 기능 — `requirements.pdf` 25p [미션1]이 "Dummy Data는 연결된 DB에 추가"라고 명시하므로, 단순히 값을 반환하는 것에서 그치지 않고 DB 반영까지 이 Tool의 역할로 포함한다.

### 제외 (Non-goals)
- 데이터 영속성 계층(파일/JSON/DB 스토리지) 자체의 구현 — `DataPersistence` PoC의 책임. 이 Tool은 그 저장 API를 호출하는 쪽이다.
- 콘솔 UI/메뉴 표시 — `ConsoleMVC`, `SampleOrderSystem`의 책임
- 실시간 데이터 조회/모니터링 — `DataMonitor` PoC의 책임
- 생산라인 스케줄링(FIFO 큐 처리) 로직 자체 — 값 생성만 지원, 실행 로직은 본 프로젝트 책임

## 6. 도메인 데이터 모델

`dataModel/`에 이미 정의된 실제 데이터 모델(`DataPersistence::Model` 네임스페이스, `../DataPersistence/Model/`을 미러링) 기준. 요구사항 문서(Chapter 2 기능 명세)의 속성 정의와 일치한다.

### 6.1 `Sample` (`dataModel/Sample.h`)

| 필드 | 타입 | 설명 |
|---|---|---|
| `id` | `int` | 고유 식별자 |
| `name` | `std::string` | 반도체 시료명 (예: "Wafer-8in") |
| `averageProductionTimePerUnit` | `double` | 평균 생산시간 (min/ea) |
| `yieldRatio` | `double` | 수율, 0~1 사이 값 |
| `stockQuantity` | `int` | 현재 재고 수량 — 테스트 시나리오(여유/부족/고갈)를 만들 수 있도록 분포 조정 가능 |

### 6.2 `Order` (`dataModel/Order.h`)

| 필드 | 타입 | 설명 |
|---|---|---|
| `id` | `int` | 고유 식별자 |
| `sampleId` | `int` | 반드시 먼저 생성된 `Sample.id`를 참조 |
| `customerName` | `std::string` | 고객명 |
| `orderedQuantity` | `int` | 주문 수량 |
| `state` | `OrderState` | `RESERVED` / `CONFIRMED` / `PRODUCING` / `RELEASE` / `REJECTED` |

상태 의미 (요구사항 문서 8p 기준):

| 상태 | 의미 |
|---|---|
| RESERVED | 주문 접수 |
| REJECTED | 주문 거절 (정상 흐름 외, 모니터링 제외 대상) |
| PRODUCING | 승인 완료 + 재고 부족 → 생산 중 |
| CONFIRMED | 승인 완료 + 출고 대기 중 |
| RELEASE | 출고 완료 |

### 6.3 `ProductionQueueEntry` (`dataModel/ProductionQueueEntry.h`)

| 필드 | 타입 | 설명 |
|---|---|---|
| `orderId` | `int` | 반드시 먼저 생성된 `Order.id`를 참조 (natural key) |
| `sampleId` | `int` | 반드시 먼저 생성된 `Sample.id`를 참조 |
| `orderedQuantity` | `int` | 해당 주문의 주문 수량 |
| `shortageQuantity` | `int` | 부족분 (`orderedQuantity - Sample.stockQuantity`) |
| `actualProductionQuantity` | `int` | 실 생산량 = `ceil(shortageQuantity / Sample.yieldRatio)` |
| `totalProductionTime` | `double` | 총 생산 시간 = `Sample.averageProductionTimePerUnit * actualProductionQuantity` |
| `state` | `ProductionState` | `WAITING` / `PRODUCING` / `CONFIRMED` |

### 6.4 데이터 정합성 규칙

- `Order.sampleId`는 항상 생성된 `Sample.id` 집합의 부분집합이어야 한다.
- `ProductionQueueEntry.orderId`/`sampleId`는 항상 먼저 생성된 `Order`/`Sample`을 참조해야 한다.
- `PRODUCING` 상태 더미 `Order`는 "주문 수량 > 해당 시료 재고" 상태를 재현해야 한다.
- `CONFIRMED`/`RELEASE` 상태 더미 `Order`는 "재고 충분" 또는 "이미 처리 완료"라는 논리적으로 일관된 재고 상태를 재현해야 한다.
- `ProductionQueueEntry`의 파생값(`actualProductionQuantity`, `totalProductionTime`)은 위 공식대로 계산되어야 한다.

## 7. 기능 요구사항

1. **`Sample` 더미 생성**: 개수(N), 재고 범위/분포(여유·부족·고갈 비율)를 파라미터로 받아 `Sample` 리스트를 생성한다.
2. **`Order` 더미 생성**: 개수(N), 상태 분포(각 상태별 비율 또는 개수), 참조할 `Sample` 목록을 파라미터로 받아 `Order` 리스트를 생성한다.
3. **`ProductionQueueEntry` 더미 생성**: `PRODUCING` 상태의 `Order`를 대상으로, 참조하는 `Sample`의 재고·수율·평균 생산시간을 이용해 부족분/실생산량/생산시간을 계산한 `ProductionQueueEntry`를 생성한다.
4. **시드 지정**: 동일 시드로 호출 시 동일한 더미 데이터셋이 재현되어야 한다 (테스트 재현성).
5. **범위/분포 커스터마이징**: 호출부에서 재고 수량, 주문 수량, 수율, 평균 생산시간의 최소/최대값을 지정할 수 있어야 한다.
6. **`storedData/` 반영**: 생성한 더미 데이터를 `storedData/samples.json`, `orders.json`, `production_queue.json`과 동일한 스키마로 실제 저장할 수 있어야 한다 — `../DataPersistence`의 `SampleRepository`/`OrderRepository`/`ProductionQueueEntryRepository`가 제공하는 `create()` 경로를 참고한다. 값 객체 컬렉션 반환도 함께 지원하여, 저장 없이 값만 필요한 테스트에도 사용할 수 있게 한다.

## 8. 개발 단계 (참고 문서 + 실제 Model/저장 결과 기준)

PoC이므로 아래 단계 순서로 진행하며, 각 단계 완료 여부는 `requirements.pdf`뿐 아니라 `dataModel/`·`storedData/`의 실제 구조와도 맞아떨어져야 판단한다. 자세한 내용은 `../CLAUDE.md`의 "개발 단계" 표를 따른다.

1. `Sample` 더미 데이터 생성 (→ `storedData/samples.json` 스키마와 일치)
2. `Order` 더미 데이터 생성, `sampleId`로 `Sample` 참조 (→ `storedData/orders.json` 스키마와 일치)
3. `ProductionQueueEntry` 더미 데이터 생성, `orderId`/`sampleId` 참조 및 부족분·실생산량·생산시간 계산 (→ `storedData/production_queue.json` 스키마와 일치)
4. 상태(`OrderState`/`ProductionState`)별 정합성 있는 더미 데이터 생성
5. 시드/개수/분포 파라미터화
6. Repository의 Create 경로를 참고해 `storedData/`에 실제 반영

## 9. 비기능 요구사항

- **재사용 가능한 API**: 공개 API는 하나의 헤더(예: `DummyDataGenerator.h`)로 노출하여 `ConsoleMVC`/`DataPersistence`/`SampleOrderSystem` 등 다른 저장소에서도 이 헤더를 include/link해 그대로 호출할 수 있어야 한다.
- **이식성**: C++20 표준(`stdcpp20`)만 사용하며, 정적 라이브러리 또는 헤더 전용 형태로 다른 프로젝트에 링크/포함 가능해야 한다.
- **스키마 일치**: `dataModel/`의 구조체와 그 JSON 직렬화(`NLOHMANN_DEFINE_TYPE_INTRUSIVE`)를 임의로 바꾸지 않는다 — `../DataPersistence/Model/`과의 일치가 재사용성의 핵심이다.

## 10. 제출 / 문서 관리 요건 (과제 규정)

- Repository 이름: `DummyDataGenerator-JongmunSong-1717`, Public.
- 평가 축: CLAUDE.md/PRD.md 등 문서 관리, Harness 도입, Test, Clean Code, Commit 이력.
- 커밋 메시지 컨벤션은 전역 CLAUDE.md(`[type] subject` 형식, 영어) 규칙을 따른다.

## 11. 성공 기준

- `Sample`/`Order`/`ProductionQueueEntry` 더미 데이터를 파라미터화된 방식으로 생성할 수 있다.
- 동일 시드로 재현 가능한 테스트 데이터셋을 만들 수 있다.
- 생성된 `Order`/`ProductionQueueEntry` 데이터가 상태-재고 정합성 규칙을 위반하지 않는다.
- 생성한 더미 데이터를 `storedData/samples.json`, `orders.json`, `production_queue.json`과 동일한 스키마로, 연결된 DB(DataPersistence 모듈)에 실제로 추가하는 시나리오가 동작한다.
- 8절의 개발 단계를 참고 문서 근거와 함께 순서대로 확인할 수 있다.
- 각 도메인 규칙(수율, 실 생산량, 상태 전이)에 대한 단위 테스트가 존재한다.
