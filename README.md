# HanSeek

한컴오피스 없이 HWP, HWPX 문서의 본문을 검색하는 C++ 로컬 검색 엔진입니다.

전남대학교 2026-2학기 C++프로그래밍 및 실습 개인 프로젝트 (고준서, 234460)

- `Sources/`: 소스 코드, 테스트, 빌드 파일
- `Reports/`: 제안서와 진척 보고서 (PDF)

## 빌드

### Windows (Visual Studio 2019 이상)

"x64 Native Tools Command Prompt"에서:

```
cd Sources
cmake -S . -B build
cmake --build build --config Release
build\Release\hanseek.exe help
```

### macOS, Linux

```
cd Sources
make          # bin/hanseek, bin/hanseek_tests 생성
make test     # 단위 테스트 실행
```

CMake로도 빌드할 수 있습니다: `cmake -S . -B build && cmake --build build`

## 사용법

```
hanseek index <폴더> [--db 파일]        폴더 안의 HWP, HWPX, TXT 문서를 색인
hanseek search <검색어> [--db 파일] [--limit N]
hanseek extract <파일>                  문서 한 개의 본문 출력
hanseek stats [--db 파일]               색인 정보 출력
```

색인 파일은 기본으로 현재 폴더의 `hanseek.idx`에 저장됩니다.

### 검색어

| 입력 | 뜻 |
|---|---|
| `hanseek search 예산 집행` | 두 단어가 모두 들어 있는 문서 |
| `hanseek search '"예산 집행"'` | 구문이 그대로 들어 있는 문서 |
| `hanseek search 예산 -추경` | "추경"이 들어 있는 문서는 제외 |

조사가 붙은 말("예산을", "예산은")도 "예산"으로 찾을 수 있습니다.

## 지금 되는 것과 앞으로 할 것

| 기능 | 상태 |
|---|---|
| HWP 5.x, HWPX, TXT 본문 추출 | 완료 |
| 한국어 bigram 역색인, 색인 파일 저장 | 완료 |
| AND, 구문, 제외어 검색과 문맥 미리보기 | 완료 |
| CLI (index, search, extract, stats) | 완료 |
| 단위 테스트 | 완료 (12개) |
| 멀티스레드 색인, 증분 색인, BM25 순위, PDF | 2차 중간 (12/6) |
| MCP 서버, GUI, 배포 | 최종 (12/21) |

## 사용한 라이브러리

- [miniz](https://github.com/richgel999/miniz) 3.0.2 (MIT): deflate 풀기와 zip 읽기
