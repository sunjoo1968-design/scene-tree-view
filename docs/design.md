# SceneAnchor Design Notes

## 목표

SceneAnchor는 OBS의 기본 장면 목록을 대체해서 장면을 생성하거나 삭제하는 도구가 아니다. 많은 장면을 폴더 트리로 분류하고, 방송 중 빠르게 찾고 전환하기 위한 보조 dock이다.

최우선 원칙은 방송 안정성이다. 트리 정리 작업은 OBS의 실제 장면 목록 순서, 장면 생성/삭제, 소스/필터 설정, transition override, multiview 설정을 직접 바꾸지 않는다.

## 범위

허용하는 기능:

- 폴더 생성, 이름 변경, 삭제, dissolve
- 폴더와 장면의 SceneAnchor 내부 위치 이동
- 장면 검색
- 장면 전환
- 폴더와 장면의 트리 내부 색상 라벨
- SceneAnchor 트리 상태의 undo/redo 및 저장

제외하는 기능:

- 장면 추가, 삭제, 복제, 이름 변경
- 필터 복사/붙여넣기 또는 필터 창 열기
- screenshot, projector 메뉴
- transition override, multiview visibility 조작
- 상단 최근 장면 빠른 버튼

장면 생성, 삭제, 복제, 이름 변경과 source 관련 관리는 OBS 기본 장면/소스 UI에서만 수행한다.

## 구조

- `tree_store`: 폴더/장면 배치 트리의 단일 데이터 소스. libobs나 Qt UI에 의존하지 않는다.
- `projection`: 저장된 트리와 OBS live scene 목록을 UI 행 계획으로 합친다.
- `tree_dock`: Qt dock UI. 검색, 트리 표시, drag-and-drop, 폴더 메뉴, 장면 전환 메뉴를 담당한다.
- `obs_bridge`: OBS frontend/libobs API 접점. 저장/로드, 이벤트, 장면 전환, hotkey, undo/redo 등록을 담당한다.
- `module`: 플러그인 로드/언로드 및 dock 등록만 담당한다.

## 저장 모델

트리 데이터는 scene collection JSON의 `modules.scene_anchor` 문자열 값으로 저장한다. OBS scene list 자체를 재정렬하지 않고, SceneAnchor 전용 JSON만 저장한다.

장면 노드는 UUID를 기본 식별자로 사용한다. scene collection 복제처럼 UUID가 바뀔 수 있는 상황에서는 저장된 이름을 fallback으로 사용해 가능한 한 기존 폴더 구조를 복구한다.

저장 포맷 버전이 현재 코드보다 높으면 foreign 상태로 보고 원본 문자열을 보존한다. 이 경우 트리는 live scene을 평면으로 표시하고, 저장 시 기존 blob을 덮어쓰지 않는다.

## 이동 정책

SceneAnchor 내부 drag-and-drop은 `TreeStore::moveNodes`와 `TreeStore::placeScene`만 호출한다. OBS scene reorder API를 호출하지 않는다.

따라서 SceneAnchor에서 장면 순서를 바꿔도 OBS 기본 장면 dock의 순서는 유지된다. 아직 SceneAnchor 트리에 배치되지 않은 live scene은 OBS 기본 순서대로 뒤에 표시되며, 사용자가 한 번 drag-and-drop하면 SceneAnchor 저장 순서로 고정된다.

검색 필터가 활성화된 동안에는 일부 행이 숨겨져 drop 위치 의미가 흐려지므로 drag-and-drop을 비활성화한다.

## 안정성 메모

- `needsRebuild` 연결은 `Qt::QueuedConnection`을 유지한다. Qt model signal 처리 중 rebuild가 직접 실행되면 해제된 item 접근 위험이 있다.
- 장면 전환은 `ObsBridge::switchToScene`과 `ObsBridge::transitionToScene`에만 남긴다.
- 장면 생성/삭제/복제/이름 변경 helper는 제거했다. SceneAnchor가 OBS scene lifecycle을 소유하지 않도록 하기 위함이다.
- 상단 최근 장면 빠른 버튼 제거 후 scene change 이벤트에서 MRU 저장도 제거했다. 방송 중 불필요한 scene collection 저장을 줄이기 위함이다.

## 검증 기준

- 폴더 생성/삭제/이름 변경/dissolve 후 장면 자체가 삭제되지 않는다.
- SceneAnchor 내부 장면 순서 변경 후 OBS 기본 장면 dock 순서가 바뀌지 않는다.
- 검색 중 drag-and-drop이 비활성화된다.
- 장면 우클릭 메뉴에 생성/삭제/복제/이름 변경/필터/projector/transition override/multiview 항목이 없다.
- 빈 영역 메뉴에 장면 추가와 최근 장면 표시 옵션이 없다.
- 하단 버튼은 폴더 추가와 폴더 제거만 제공한다.
- 선택 전환 옵션이 켜진 상태에서만 selection change가 장면 전환을 수행한다.
