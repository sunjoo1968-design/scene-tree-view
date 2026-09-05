# Scene Tree View

장면 트리-Made by Sunjoo

OBS 라이브커머스 방송 운영을 위한 장면 폴더 트리 플러그인입니다.

## 기능

- 폴더와 장면의 트리 배치. OBS 기본 장면 목록 순서는 변경하지 않습니다.
- 송출 장면은 붉은색, 미리보기 장면은 파란색으로 표시합니다.
- 하단 배치 잠금은 드래그와 폴더 편집을 차단하며 재시작 후에도 유지됩니다.
- 폴더 더블클릭으로 펼치고 접습니다. 장면 더블클릭에는 별도 전환 동작이 없습니다.
- 검색과 단일 클릭은 일반 장면 선택입니다. 스튜디오 모드에서는 미리보기에 적용됩니다.
- 장면 생성과 삭제는 OBS 기본 장면 목록에서 수행합니다.

## 설치

Windows x64, OBS 32 계열용입니다. OBS를 종료하고 배포 ZIP의 `data`, `obs-plugins` 폴더를 `C:\Program Files\obs-studio\`에 병합합니다.

기존 설치와 폴더 배치 호환성을 위해 DLL 이름 `scene-anchor.dll`과 내부 저장 키는 유지합니다. 중복 설치하지 마세요.

## 빌드

Visual Studio 2022 C++ Build Tools, Windows SDK, CMake가 필요합니다. 기본 프리셋은 SDK 10.0.22621을 사용합니다. SDK 10.0.26100.0 환경에서는 다음을 실행합니다.

```powershell
cmake -S . -B build_x64_local -G "Visual Studio 17 2022" -A "x64,version=10.0.26100.0" -DENABLE_FRONTEND_API=ON -DENABLE_QT=ON
cmake --build build_x64_local --config RelWithDebInfo --parallel --target scene-anchor
```

의존성은 최초 구성 시 내려받습니다. `tests`에는 저장소·투영 회귀 테스트가 있습니다. 실제 OBS 조작과 장시간 방송 안정성은 별도로 검증해야 합니다.

## 저작권

Sunjoo가 방송 운영 용도로 수정한 SceneAnchor 파생 프로젝트입니다.
원본: https://github.com/rockbenben/scene-anchor (rockbenben).
원저작권 고지와 GPL-2.0-or-later 라이선스를 유지합니다. 자세한 내용은 [LICENSE](LICENSE)를 참고하세요.
