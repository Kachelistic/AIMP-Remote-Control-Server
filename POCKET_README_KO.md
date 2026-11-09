# AIMP Pocket

SLV Tech AIMP HTTP Remote Control Server의 모바일 웹 UI 통합본입니다.
원본: https://github.com/slv-tech/AIMP-Remote-Control-Server
기준 커밋: b42fccd65c964535fcfbdcc760967ccb27b48bf4

## 설치와 사용

1. 기존 AIMP HTTP Control 플러그인을 백업하고 AIMP를 종료합니다.
2. AimpHttpControl.aimppack을 AIMP로 열어 설치합니다. 기존 플러그인과 중복 설치하지 않습니다.
3. 기존 HTTP 서버 설정을 그대로 사용합니다. HTTP 모드에서 휴대폰이 접근할 수 있는 인터페이스를 선택합니다.
4. 휴대폰에서 http://PC주소:기존포트/ 에 접속합니다. 기본 포트는 19122입니다.
5. Safari 공유 메뉴의 '홈 화면에 추가'로 바로가기를 만들 수 있습니다.

재생·일시정지·이전/다음 곡·정지·셔플·음소거·볼륨·재생 위치 조절,
플레이리스트 선택과 곡 재생을 지원합니다. 검색은 불러온 곡 범위에 적용되며
'더 불러오기'로 목록을 추가할 수 있습니다. 별도 웹서버와 CDN은 필요 없습니다.

## HTML 수정

설치된 AimpHttpControl.dll과 같은 폴더 안의 `web/pocket.html`을 UTF-8로
수정·저장한 뒤 휴대폰 브라우저를 새로고침하면 바로 반영됩니다.
AIMP 재시작이나 DLL 재빌드는 필요 없습니다. CSS와 JavaScript도 이 HTML 안에 있습니다.
예: `AIMP/Plugins/AimpHttpControl/web/pocket.html`.
DLL이 x64 하위 폴더에서 로드되는 설치 형태라면 그 폴더의 `web/pocket.html`을 사용합니다.
파일이 없거나 읽을 수 없거나 비어 있거나 4 MiB를 넘으면 HTTP 404 오류가 표시됩니다.
`.aimppack`에는 두 아키텍처용 폴더에 HTML을 각각 포함합니다.
플러그인을 재설치·업데이트하기 전에는 수정한 HTML을 별도로 백업하세요.
추가 파일을 제공하는 일반 웹서버 기능은 없으므로 CSS/JS는 HTML 안에 유지합니다.

## API 호환성

추가 경로는 GET /, GET /index.html, GET /pocket입니다.
기존 HTTP GET/POST/PUT/OPTIONS, JSON 응답, CORS, IP 허용 목록,
WebSocket 모드 및 플레이어 구현은 원본을 유지합니다.
웹 UI도 기존 IP 허용 목록 검사 이후에 제공됩니다.
웹 UI의 명령은 현재 페이지와 같은 주소/포트를 사용합니다.
HTTP와 WebSocket 모드는 원본과 같이 동시에 활성화되지 않습니다.

## 빌드 및 검증

GitHub Actions의 AIMP Pocket Windows는 windows-2022에서 MSYS2 MinGW로
x86/x64 DLL을 컴파일하고 AIMP 패키지 구조로 압축합니다.
API 소스 보존 검사, 원본/통합 HTTP 서버 실행 비교, PE 아키텍처,
플러그인 진입점, 런타임 DLL 의존성, 패키지 구조 및 SHA-256을 검사합니다.
HTTP 서버 비교는 AIMP 코어가 없는 테스트 호스트에서 수행합니다.
이 검사는 실제 AIMP에서 재생 및 설치 검증을 대신하지 않습니다.

HTML은 독립 파일이므로 변경 시 DLL을 다시 빌드할 필요가 없습니다.
DLL 빌드는 `bash tools/build-windows.sh`, 패키지는 `./tools/package.ps1`입니다.
