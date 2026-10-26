# OBS33 정식 검증 — Scene Tree View 1.1.1

2026-10-26 · SunjooAn · Windows x64 / OBS33.0.0 정식 / MSVC2022 / Qt6.11.1

조합: Controller0.2.1 / Multiview0.6.1 / Tally1.7.2-obs33.1 / Scene Tree View1.1.1 / Source Switcher0.4.4-sunjoo.obs33.1.

- 세 native DLL 정식 SDK 재빌드. 새 plugins/module/module.dll 경로에서 실제 로드와 버전 확인.
- Multiview layout/tally CTest2개, SceneTree 저장·투영 CTest1개 통과.
- 실제 OBS33 PGM/PVW 멀티뷰·탈리 일치, ME1/ME2 MIX 중 TAKE 양쪽 기여·독립 완료·이전 카메라 OFF 및 정상 종료 통과.
- 120초 통합 반복: TAKE116회 / 인증 재접속5회, 정상 종료 exit=0.
- 빠른 CUT native 회귀: 정확한 CUT 목적지100회, 동시 CUT 중4000 snapshot 모두 프로그램 경로 유지.
- 실제 Multiview 설정·크기/드래그 배치·저장·전체화면·재열기·SceneTree 목록 초기/설정/재열기/장면모음 재로드 확인, 정상 종료 통과.
- Tally Hub Jest29 suites, 285개 통과·기존1개 skip. backend/frontend/트레이·서버 실행 파일 빌드 통과. packaged HTTP·버전·firmware targets·상태 late-client replay·MIX 양쪽/완료OFF·500 event 병합·장면모음 교체·단절/재접속·배정 재시작 복원 모두 통과.

한계: 전체 OBS 종료 로그의 잔여 할당1건을 관찰했으며 전체 누수0을 주장하지 않습니다. 실제 장치 수 시간 방송·음질과 모든 SceneTree 수동 편집 조작은 별도 시험입니다. Source Switcher 재빌드판의 전체 옵션도 이번 검증 범위 밖입니다.

보존: 현장 OBS/Hub·개인 설정·사용자 3D 작업·멀티뷰 .gitignore/LOCAL-RESUME은 변경하지 않았습니다. 리스너 firmware13파일과 UDP/vMix/ATEM 구현은 보존합니다. 기존 OBS32/RC 릴리스 파일은 삭제/교체하지 않으며 원격 asset digest를 게시 전후 대조합니다. Tally GitHub는 비공개를 유지합니다.
