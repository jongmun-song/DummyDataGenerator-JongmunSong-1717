# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

`DummyDataGenerator`는 "반도체 시료 생산주문관리 시스템"(SampleOrderSystem, `../ref/requirements.pdf` 참고) 개발 과제의 PoC 산출물 중 하나로, **Test 를 위한 Dummy Data 를 생성하는 독립 모듈**이다. 이 저장소는 단독 실행 파일이 아니라, `ConsoleMVC`/`DataPersistence`/`SampleOrderSystem` 등 다른 저장소에서 모듈(라이브러리)로 include/link 하여 재사용하는 것을 전제로 설계·개발한다.

이 프로젝트는 **PoC**다. 추상적으로 도메인을 감춘 범용 모듈이 목적이 아니라, 전체 프로젝트(`../CLAUDE.md` 상위 레벨, `../DataPersistence`)가 이미 정의한 실제 데이터 모델과 저장 결과물을 그대로 사용하는 Dummy 데이터 생성 Tool을 만드는 것이 목적이다.

- **`dataModel/`** — `DataPersistence::Model` 네임스페이스의 `Sample`, `Order`, `ProductionQueueEntry` 구조체(원본: `../DataPersistence/Model/`). 전체 프로젝트에서 이 Model을 기준으로 데이터를 저장한다.
- **`storedData/`** — 위 Model을 실제로 저장했을 때 나오는 JSON 결과 파일(`samples.json`, `orders.json`, `production_queue.json`, 원본 구조: `../DataPersistence`의 `SampleRepository`/`OrderRepository`/`ProductionQueueEntryRepository`가 산출).

즉 **이 Tool이 만들어야 할 "테스트용 데이터"는 `dataModel/`의 구조체를 채운 값들이며, 그 결과가 `storedData/`에 있는 JSON 파일들과 같은 형태로 남아야 한다.** `../DataPersistence/docs/PRE.md`도 이를 명시한다: "`DummyDataGenerator` | 동일한 Create 경로를 참고해 더미 데이터 삽입 도구를 구현".

## 빌드 / 실행

- IDE: Visual Studio (Platform Toolset v145, C++20, Win32/x64, Debug/Release 구성)
- CLI 빌드 예시 (Developer PowerShell):
  ```
  msbuild DummyDataGenerator.slnx /p:Configuration=Debug /p:Platform=x64
  ```
- gtest는 `packages.config`(`Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn.1.8.1.8`)로 이미 등록되어 있다. 테스트 프로젝트/러너 구성이 아직 없다면 추가 시 `.vcxproj`에 함께 반영한다.
- 콘솔 실행형 데모/자체 테스트가 필요하면 `main.cpp`는 라이브러리 코드와 분리하고, 모듈 자체는 다른 프로젝트가 링크할 수 있는 형태(정적 라이브러리 또는 헤더 전용)로 구성한다.

## 데이터 모델 (`dataModel/`)

`../DataPersistence/Model/`을 그대로 미러링한 순수 데이터 구조체다. 이 프로젝트에서 새로 도메인을 정의하지 말고 이 구조체를 그대로 사용한다.

### `dataModel/Sample.h` — `DataPersistence::Model::Sample`
| 필드 | 타입 |
|---|---|
| `id` | `int` |
| `name` | `std::string` |
| `averageProductionTimePerUnit` | `double` |
| `yieldRatio` | `double` (0~1) |
| `stockQuantity` | `int` |

### `dataModel/Order.h` — `DataPersistence::Model::Order`
| 필드 | 타입 |
|---|---|
| `id` | `int` |
| `sampleId` | `int` (Sample.id 참조) |
| `customerName` | `std::string` |
| `orderedQuantity` | `int` |
| `state` | `OrderState`: `RESERVED`/`CONFIRMED`/`PRODUCING`/`RELEASE`/`REJECTED` |

### `dataModel/ProductionQueueEntry.h` — `DataPersistence::Model::ProductionQueueEntry`
| 필드 | 타입 |
|---|---|
| `orderId` | `int` (Order.id 참조, natural key) |
| `sampleId` | `int` (Sample.id 참조) |
| `orderedQuantity` | `int` |
| `shortageQuantity` | `int` |
| `actualProductionQuantity` | `int` (= `ceil(shortageQuantity / yieldRatio)`) |
| `totalProductionTime` | `double` (= `averageProductionTimePerUnit * actualProductionQuantity`) |
| `state` | `ProductionState`: `WAITING`/`PRODUCING`/`CONFIRMED` |

세 구조체 모두 `NLOHMANN_DEFINE_TYPE_INTRUSIVE`로 JSON 직렬화가 이미 구현되어 있다(nlohmann/json 필요, `../DataPersistence/external/nlohmann` 참고). 더미 데이터도 이 직렬화를 그대로 사용해 `storedData/`와 동일한 스키마의 JSON을 만든다.

## 목표 산출물 (`storedData/`)

이 폴더의 기존 파일들은 위 Model을 Repository로 저장했을 때 나오는 실제 결과물의 예시다. 더미 데이터 생성 결과는 이 파일들과 **같은 스키마·같은 배열 형태**여야 한다.

- `storedData/samples.json` — `Sample[]`
- `storedData/orders.json` — `Order[]`
- `storedData/production_queue.json` — `ProductionQueueEntry[]`

## 개발 단계 (참고 문서 + 실제 Model/저장 결과 기준)

PoC이므로 아래 단계 순서로 진행 상황을 확인한다. 각 단계는 `requirements.pdf`뿐 아니라 `dataModel/`·`storedData/`의 실제 구조와도 맞아떨어져야 완료로 판단한다.

| 단계 | 내용 | 근거 |
|---|---|---|
| 1 | `Sample` 더미 데이터 생성 | `dataModel/Sample.h`, `storedData/samples.json`, requirements.pdf 12p 시료 관리 |
| 2 | `Order` 더미 데이터 생성 (`sampleId`로 Sample 참조) | `dataModel/Order.h`, `storedData/orders.json`, requirements.pdf 14p 시료 주문 |
| 3 | `ProductionQueueEntry` 더미 데이터 생성 (`orderId`/`sampleId` 참조, 부족분·실생산량·생산시간 계산) | `dataModel/ProductionQueueEntry.h`, `storedData/production_queue.json`, requirements.pdf 20p 생산라인 |
| 4 | 상태(`OrderState`/`ProductionState`)별 정합성 있는 더미 데이터 생성 | requirements.pdf 8p 주문 상태 흐름, 18p 모니터링(여유/부족/고갈) |
| 5 | 시드/개수/분포 파라미터화 | 테스트 재현성 확보 |
| 6 | Repository의 Create 경로를 참고해 `storedData/`에 실제 반영 | `../DataPersistence/docs/PRE.md` "DummyDataGenerator: 동일한 Create 경로를 참고해 더미 데이터 삽입 도구를 구현", `SampleRepository::create`/`OrderRepository::create`/`ProductionQueueEntryRepository::create` 시그니처 |

각 단계는 순서대로 진행한다. 이전 단계가 다음 단계의 전제 조건이다(예: `Order`는 `Sample`이 먼저 있어야 하고, `ProductionQueueEntry`는 `Order`가 먼저 있어야 함).

## 데이터 간 정합성

- `Order.sampleId`는 반드시 먼저 생성된 `Sample.id` 집합에 존재해야 한다.
- `ProductionQueueEntry.orderId`/`sampleId`는 반드시 먼저 생성된 `Order`/`Sample`을 참조해야 한다.
- `PRODUCING` 상태의 `Order`는 "재고 부족"(`orderedQuantity` > 해당 `Sample.stockQuantity`) 상황을 재현해야 하고, `CONFIRMED`/`RELEASE`는 재고가 충분하거나 이미 처리된 상황을 재현해야 한다.
- `ProductionQueueEntry.actualProductionQuantity = ceil(shortageQuantity / yieldRatio)`, `totalProductionTime = averageProductionTimePerUnit * actualProductionQuantity` 공식을 지켜 파생값을 계산한다.

## 개발 시 유의사항

- 이 저장소는 개인 과제 제출물이며, Repository 이름 규칙은 `DummyDataGenerator-본인영문이름-사번`, Public으로 생성해야 한다(제출 규정).
- 과제 평가 축(문서 관리, Harness, Test, Clean Code, Commit 이력)에 맞춰 PRD.md 등 설계 문서와 커밋 이력을 관리한다.
- 언어 표준은 C++20(`stdcpp20`)을 유지한다. 새 소스 파일 추가 시 `.vcxproj`의 `ClCompile`/`ClInclude` 항목 그룹에도 반영해야 빌드에 포함된다.
- `dataModel/`의 구조체를 임의로 변경하지 않는다 — `../DataPersistence/Model/`과의 스키마 일치가 재사용성의 핵심이다. 변경이 꼭 필요하면 `DataPersistence` 쪽 변경과 동기화한다.
