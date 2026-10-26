# Sunjoo OBS Link Scene Tree View 1.1.1 — OBS 33.0.0

제작자 SunjooAn · 2026-10-26 · Windows x64 · OBS 33.0.0 정식용

기존 OBS32 릴리스·태그와 RC1 시험 릴리스는 보존합니다. 이번 버전은 공식 OBS33.0.0 SDK와 정식 DLL import libraries로 별도 재빌드했습니다. RC 플래그·표시·manifest·새 plugins 배치가 정식33에 맞춰졌습니다. 기존 내부 source/vendor/dock/settings ID는 유지합니다.

## 설치
방송이 없는 시간에 사용자가 OBS/허브를 종료한 후 교체합니다. 설치형 OBS에서는 ZIP의 plugins 안 모듈 폴더를 C:/ProgramData/obs-studio/plugins/ 아래에 복사합니다. portable OBS에서는 plugins 폴더를 OBS 루트에 합칩니다. 새 구조는 plugins/<module>/<module>.dll 및 data/manifest.json입니다. core 폴더에 넣지 않습니다. 같은 DLL을 예전 위치와 새 위치에 중복 설치하지 마세요. 기존 DLL·설정은 백업하고 보관합니다.

씬앵커(Scene Tree View)는 Source Switcher 없이 독립 동작합니다. 기존 scene-anchor.dll, scene_anchor 저장 키, scene_anchor_dock ID를 유지하므로 기존 장면 폴더 배치를 이어 사용합니다. 원작 rockbenben 저작권/GPL과 SunjooAn 재배포 표기를 유지합니다.

## 소스 빌드
전체 source ZIP의 scripts/build-obs33.ps1 -ObsPath <정식33폴더> -ObsSourceDir <공식33.0.0소스> -QtPath <Qt6.11SDK>를 사용합니다. Tally bridge에는 Qt SDK가 필요하지 않습니다. Tally 허브 빌드는 Node 의존성 설치 후 -BuildHub를 추가합니다. SceneTree의 OBS33 CMake 엔트리는 cmake/obs33입니다. 하단 예전 README의 OBS32 빌드법은 Legacy 자료입니다.

공식 소스 SHA256: c1ae3c98c1bb70ebdced33e894a526fcbbbda9bfd95806970a3fc31f74b08f6c
공식 릴리스: https://github.com/obsproject/obs-studio/releases/tag/33.0.0
외부 OBS/Source Switcher/wifi-tally/SceneAnchor의 원저작권과 라이선스를 유지합니다.
