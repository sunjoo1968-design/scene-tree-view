# Sunjoo OBS Link Scene Tree View 1.1.0 — OBS 33 RC1

제작자 SunjooAn · Windows x64 · 대상 OBS 33.0.0-rc1. 정식판 보증 버전이 아닌 시험 릴리스입니다.

기존 버전은 **OBS 32 Legacy**로 구분합니다. 이전 GitHub Releases와 `legacy-obs32-20261016` 태그를 보존합니다. 새 시험판은 `codex/obs33-rc1` 브랜치에서 개발합니다. 내부 DLL/source ID/설정 키는 유지하므로 이름을 바꾼 DLL을 중복 설치하지 마세요.

## 설치
방송이 없는 시간에 사용자가 OBS와 허브를 종료한 뒤 설치합니다. 설치형 OBS에서는 ZIP의 plugins 폴더 내부 각 모듈 폴더를 `C:/ProgramData/obs-studio/plugins/`에 복사합니다. portable OBS에서는 ZIP의 plugins 폴더를 portable OBS 루트에 합칩니다. 새 구조는 `plugins/<module>/<module>.dll`과 `plugins/<module>/data/manifest.json`입니다. 기존 같은 모듈의 Legacy DLL은 다른 경로에 함께 남기지 말고 보관 장소로 이동하세요. 자동 삭제·교체는 하지 않습니다.

Controller는 동봉 scripts/camera-mix-hybrid.lua를 OBS 스크립트에 등록하고 이전 동일 스크립트 등록은 해제합니다. 외부 Source Switcher가 필요합니다. 기존 카메라 배정/폴더 설정은 백업 후 사용하세요. Tally listener firmware·UDP·vMix/ATEM은 변경하지 않았습니다. Tally GitHub는 비공개입니다.

## OBS33 변경
공식 33.0.0-rc1 헤더와 제공된 RC1 DLL에서 만든 import libraries로 빌드합니다. Qt 6.11.x를 사용합니다. OBS33 core 폴더와 새 plugin manager 레이아웃, manifest metadata를 반영합니다. Controller의 영구 장면 생성은 명시적 main canvas API를 사용합니다. 그룹/PGM 복제·MIX 진행의 기존 의미는 보존합니다.

공식 참고: https://github.com/obsproject/obs-studio/releases/tag/33.0.0-rc1
외부 Source Switcher, OBS 및 SceneAnchor(rockbenben)의 저작권과 라이선스는 각 배포 파일에 보존합니다.

## 소스 빌드
동일 릴리스의 전체 source ZIP을 풀고 `scripts/build-obs33.ps1 -ObsPath <OBS33폴더> -ObsSourceDir <공식33.0.0-rc1소스폴더> -QtPath <Qt6.11SDK폴더>`를 실행합니다. Controller/Tally bridge에는 Qt SDK가 필요하지 않습니다. Tally 허브 전체 빌드는 Node 의존성을 먼저 설치한 뒤 `-BuildHub`를 추가합니다. Windows 실행 파일은 별도 `scripts/build-obs-compat.ps1` 경로를 사용합니다. Scene Tree View의 OBS33 전용 CMake 엔트리는 `cmake/obs33`입니다. 기존 README 하단의 OBS32 빌드·설치법은 Legacy 자료입니다.
