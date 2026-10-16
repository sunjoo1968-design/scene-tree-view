# OBS 33.0.0-rc1 검증 보고

2026-10-16 · 제작자 SunjooAn · Windows x64

조합: Controller 0.2.0 / Multiview 0.6.0 / Tally 1.7.0-obs33.rc1 / Scene Tree View 1.1.0.

공식 OBS33 RC1 소스 SHA256: 1a73e95b5ad8addd79aeb5b48e5292b982dd2ffa69744294930a7c474c958fe0. 사용자가 제공한 RC1 bin/core/data를 별도 portable 시험본으로 복사했다. 현장 설치본 및 제공 원본 폴더는 변경하지 않았다.

- 네 native DLL의 OBS33 SDK 재빌드 및 신형 plugins/module/module.dll 경로 로딩 통과. Qt runtime 6.11.1.
- 실제 OBS ME1/ME2 MIX 중 TAKE: 두 카메라 기여, 독립적 남은 MIX 완료, 이전 카메라 OFF, PGM/PVW 실제 Multiview/Tally 일치 및 연동 규격 2 통과.
- 120초 반복: TAKE 117회 / 인증 재접속 5회, 정상 종료 exit=0. 이후 명시적 main canvas Lua API와 RC 플래그를 반영한 최종 DLL 조합을 별도로 재검증하여 통과했다.
- 최종 Controller Lua 최초 생성 CREATED / 재시작 RESTORED 2회 및 정상 종료 exit=0 통과.
- Multiview layout/tally CTest 2개, Scene Tree 저장/투영 CTest 1개 통과. 씬 트리 locale hard issue 0. Qt 실제 dock 생성 및 장면 목록 재구성 로그 확인.
- Tally backend/frontend/Windows 실행 파일 빌드와 Jest 28개 suite, 277개 통과·기존 1개 skip. 새 실행 파일 runtime: HTTP/frontend 버전·firmware targets, 상태 UI late-client replay, MIX 양쪽 기여/이전 카메라 OFF, 500 events 병합, 장면 모음 교체, 갑작스러운 단절·재접속, 배정 재시작 복원 모두 통과.

한계: RC1 시험판이며 실제 방송 장비의 수 시간 운용·오디오 품질·모든 씬 트리 편집 조작을 검증한 결과가 아니다. OBS 전체 종료 로그에 잔여 할당 1건을 관찰했으므로 전체 누수 0건을 주장하지 않는다. Scene 복제 내부에서 main canvas fallback 경고가 발생할 수 있으나 시험의 배치·판정·종료는 통과했다. Source Switcher는 기존 설치 0.4.4로 시험했으며 외부 의존성의 모든 기능을 보증하지 않는다.

배포: 네 모듈 모두 OBS33 RC1 시험판으로 배포하며 이전 Legacy Releases와 소스를 보존한다. 리스너 firmware·UDP·vMix/ATEM 및 현장 설정·사용자 3D 작업은 보존한다. Tally 저장소는 비공개 유지. 배포 ZIP의 SHA256SUMS.txt와 GitHub asset digest를 확인한다.
