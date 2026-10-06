# AIMP HTTP Remote Control Server

[Русский](#русский) | [English](#english)

---

## Русский

Плагин для удалённого управления AIMP через HTTP API или WebSocket. Работает с версией плеера 5.40.2716 и старше. Доступен для Windows (`.dll`). Создан для управления плеером в Bitfocus Companion через соответствующий модуль. Работает с Companion 4.3.2 и старше. Модуль для Companion доступен в каталоге модулей.

### Установка

#### Через пакет `.aimppack` (рекомендуется)

Скачайте `AimpHttpControl.aimppack` из releases и дважды кликните по файлу — AIMP установит плагин автоматически, выбрав нужную разрядность (x64 или x32).

#### Вручную

Скопируйте нужный `.dll` файл напрямую в папку плагинов AIMP и переименуйте в `AimpHttpControl.dll`:

```
%APPDATA%\AIMP\Plugins\AimpHttpControl.dll
```

Используйте `AimpHttpControl64.dll` для 64-бит AIMP, `AimpHttpControl32.dll` для 32-бит.

### Сборка

Готовые файлы для релизов собираются автоматически в GitHub Actions: пуш тега запускает сборку x64 + x32 и публикует `.aimppack` и обе `.dll` в GitHub Release (см. `.github/workflows/build-release.yml`).

#### Быстрая сборка (x64 + x32 + пакет)

```bash
bash build_package.sh
```

Создаёт `AimpHttpControl64.dll`, `AimpHttpControl32.dll` и `AimpHttpControl.aimppack`.

#### Windows (x64)

```bash
x86_64-w64-mingw32-g++ \
    -std=c++17 -O2 -Wall \
    -Wno-missing-braces -Wno-delete-non-virtual-dtor \
    -D_WIN32_WINNT=0x0A00 \
    -Isrc -isystem sdk -isystem third_party \
    src/globals.cpp src/utils.cpp src/network.cpp src/focus_sync.cpp \
    src/settings.cpp src/player_api.cpp src/http_server.cpp src/ws_server.cpp \
    src/options_frame.cpp src/plugin.cpp \
    -shared -o AimpHttpControl64.dll \
    -lws2_32 -liphlpapi -luuid -lkernel32 -luser32 -lgdi32 \
    -static-libgcc -static-libstdc++ \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
```

#### Windows (x32)

```bash
i686-w64-mingw32-g++ \
    -std=c++17 -O2 -Wall \
    -Wno-missing-braces -Wno-delete-non-virtual-dtor \
    -D_WIN32_WINNT=0x0A00 \
    -Isrc -isystem sdk -isystem third_party \
    src/globals.cpp src/utils.cpp src/network.cpp src/focus_sync.cpp \
    src/settings.cpp src/player_api.cpp src/http_server.cpp src/ws_server.cpp \
    src/options_frame.cpp src/plugin.cpp \
    -shared -o AimpHttpControl32.dll \
    -lws2_32 -liphlpapi -luuid -lkernel32 -luser32 -lgdi32 \
    -static-libgcc -static-libstdc++ \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic \
    src/plugin.def
```

### Настройки

Открываются через меню AIMP: **Настройки → Плагины → HTTP Remote Control**. Все настройки сохраняются при перезапуске плеера.

#### Сервер

| Параметр | По умолчанию | Описание |
|----------|-------------|----------|
| Port | `19122` | Порт сервера (общий для HTTP и WebSocket) |
| Server mode | `HTTP` | `HTTP` — REST API; `WebSocket` — WS-сервер с теми же командами и push-событиями. Режимы взаимоисключающие: на порту поднимается только один из них, переключение перезапускает сервер на лету |
| Listen interface | `All interfaces (0.0.0.0)` | `Localhost (127.0.0.1)` — только локальные подключения; `All interfaces (0.0.0.0)` — все интерфейсы; далее — список реальных сетевых интерфейсов системы (имя адаптера + IP) |

#### Политика доступа

| Параметр | По умолчанию | Описание |
|----------|-------------|----------|
| Allow access only from | выключено | Если включено, принимаются подключения только с адресов из списка ниже |
| IP / CIDR list | — | Список разрешённых адресов через запятую. Поддерживаются точные адреса и CIDR-диапазоны, например: `192.168.1.5, 192.168.2.0/24, 10.0.0.0/8` |

Если галочка снята — подключения принимаются с любых адресов (whitelist выключен).

### API эндпоинты

#### Плеер

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/api/player/status` | Полный статус плеера (состояние, трек, громкость, позиция, плейлист, фокус) |
| POST | `/api/player/play` | Возобновить воспроизведение |
| POST | `/api/player/pause` | Пауза |
| POST | `/api/player/stop` | Стоп |
| POST | `/api/player/next` | Следующий трек |
| POST | `/api/player/prev` | Предыдущий трек |
| GET | `/api/player/volume` | Получить громкость |
| PUT | `/api/player/volume` | Установить громкость (тело: `{"volume": 75}` или `?volume=75`) |
| POST | `/api/player/mute` | Переключить mute |
| PUT | `/api/player/position` | Установить позицию в секундах (тело: `{"position": 30}`) |
| GET | `/api/player/shuffle` | Получить состояние shuffle |
| POST | `/api/player/shuffle` | Переключить shuffle |
| GET | `/api/player/repeat` | Получить состояние repeat |
| POST | `/api/player/repeat` | Переключить repeat |
| GET | `/api/player/auto-jump` | Получить состояние auto-jump |
| POST | `/api/player/auto-jump` | Переключить auto-jump |

#### Фокус (навигация по плейлистам без воспроизведения)

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/api/focus` | Текущий фокус (плейлист + трек) |
| POST | `/api/focus/playlist/next` | Фокус на следующий плейлист |
| POST | `/api/focus/playlist/prev` | Фокус на предыдущий плейлист |
| POST | `/api/focus/track/next` | Фокус на следующий трек |
| POST | `/api/focus/track/prev` | Фокус на предыдущий трек |
| POST | `/api/focus/play` | Воспроизвести трек в фокусе |

#### Плейлисты

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/api/playlists` | Список всех плейлистов |
| GET | `/api/playlists/{id}` | Информация о плейлисте |
| GET | `/api/playlists/{id}/tracks` | Треки плейлиста (`?limit=50&offset=0`) |
| GET | `/api/playlists/{id}/tracks/{tid}` | Информация о треке |
| POST | `/api/playlists/{id}/play` | Воспроизвести плейлист с первого трека |
| POST | `/api/playlists/{id}/resume` | Воспроизвести плейлист с последней позиции (трек в фокусе) |
| POST | `/api/playlists/{id}/select` | Сделать плейлист активным |
| POST | `/api/playlists/{id}/tracks/{tid}/play` | Воспроизвести трек |
| POST | `/api/playlists/{id}/tracks/{tid}/select` | Установить фокус на трек |

### WebSocket

Кроме HTTP-сервера плагин умеет работать как WebSocket-сервер. Режим переключается в настройках: **Режим сервера → WebSocket**. В этом режиме вместо polling клиент получает push-события от плеера.

Режимы взаимоисключающие: на выбранном порту поднимается либо HTTP, либо WebSocket. Переключение режима перезапускает сервер на лету, без перезапуска плеера.

#### Подключение

```
ws://<host>:<port>/
```

Порт и интерфейс прослушивания те же, что и в HTTP-режиме (по умолчанию `19122`), политика доступа (whitelist по IP/CIDR) действует и здесь. Путь в URL не проверяется — принимается любой. Поддерживается только `ws://`, без TLS.

#### Формат команд

Клиент отправляет текстовый JSON-фрейм с полем `action`:

```json
{"action": "player/play"}
```

Ответ приходит только отправителю, с эхом `action` для сопоставления:

```json
{"action": "player/play", "state": "playing"}
```

При ошибке возвращается объект `error`:

```json
{"action": "player/volume/set", "error": {"code": "INVALID_VOLUME"}}
```

Коды ошибок: `UNKNOWN_ACTION`, `INVALID_VOLUME`, `INVALID_POSITION`, `INVALID_PLAYLIST_ID`, `INVALID_ID`.

#### Команды

| Action | Параметры | Описание |
|--------|-----------|----------|
| `player/status` | — | Полный статус плеера |
| `player/play` | — | Возобновить воспроизведение |
| `player/pause` | — | Пауза |
| `player/stop` | — | Стоп |
| `player/next` | — | Следующий трек |
| `player/prev` | — | Предыдущий трек |
| `player/volume/get` | — | Получить громкость и состояние mute |
| `player/volume/set` | `volume` (0–100) | Установить громкость |
| `player/mute` | — | Переключить mute |
| `player/position/set` | `position` (секунды) | Установить позицию |
| `player/shuffle/get` | — | Получить состояние shuffle |
| `player/shuffle/toggle` | — | Переключить shuffle |
| `player/repeat/get` | — | Получить состояние repeat |
| `player/repeat/toggle` | — | Переключить repeat |
| `player/auto-jump/get` | — | Получить состояние auto-jump |
| `player/auto-jump/toggle` | — | Переключить auto-jump |
| `focus/get` | — | Текущий фокус (плейлист + трек) |
| `focus/playlist/next` | — | Фокус на следующий плейлист |
| `focus/playlist/prev` | — | Фокус на предыдущий плейлист |
| `focus/track/next` | — | Фокус на следующий трек |
| `focus/track/prev` | — | Фокус на предыдущий трек |
| `focus/play` | — | Воспроизвести трек в фокусе |
| `playlists/get` | — | Список всех плейлистов |
| `playlists/get/one` | `playlist_id` | Информация о плейлисте |
| `playlists/tracks/get` | `playlist_id`, `limit`, `offset` | Треки плейлиста |
| `playlists/track/get` | `playlist_id`, `track_id` | Информация о треке |
| `playlists/play` | `playlist_id` | Воспроизвести плейлист с первого трека |
| `playlists/resume` | `playlist_id` | Воспроизвести плейлист с последней позиции |
| `playlists/select` | `playlist_id` | Сделать плейлист активным |
| `playlists/track/play` | `playlist_id`, `track_id` | Воспроизвести трек |
| `playlists/track/select` | `playlist_id`, `track_id` | Установить фокус на трек |

#### События (push)

Сразу после установки соединения клиент получает текущий статус с `event: "connected"`. Далее сервер рассылает всем подключённым клиентам:

| Event | Когда приходит | Полезная нагрузка |
|-------|----------------|-------------------|
| `connected` | сразу после подключения | полный статус плеера |
| `player_state_changed` | смена состояния (play/pause/stop) | полный статус плеера |
| `track_changed` | начало воспроизведения трека или потока | полный статус плеера |
| `playback_ended` | завершение воспроизведения | полный статус плеера |
| `property_changed` | изменение свойств (shuffle, repeat, auto-jump, mute, громкость) | полный статус плеера |
| `focus_changed` | смена активного плейлиста или трека в фокусе | полный статус плеера |
| `position` | обновление позиции воспроизведения | `position`, `duration`, `remaining` |

Событие `position` лёгкое — только позиция, без плейлистов и треков, поэтому подходит для прогресс-бара.

#### Пример (JavaScript)

```js
const ws = new WebSocket('ws://127.0.0.1:19122/');

ws.onopen = () => ws.send(JSON.stringify({ action: 'player/status' }));

ws.onmessage = (e) => {
  const msg = JSON.parse(e.data);
  if (msg.event === 'position') {
    console.log(`${msg.position} / ${msg.duration}`);
  } else if (msg.event) {
    console.log('event:', msg.event, msg);
  } else if (msg.action) {
    console.log('reply to', msg.action, msg);
  }
};
```

#### Ограничения

- Только `ws://`, без TLS (`wss://` не поддерживается) и без согласования subprotocol.
- Фрагментированные сообщения не поддерживаются: команда должна прийти одним фреймом, размер входящего фрейма — до 64 КиБ.
- Обрабатываются только текстовые фреймы (плюс служебные close/ping), бинарные data-фреймы игнорируются.
- Заголовок апгрейда должен быть в стандартном виде `Upgrade: websocket` — заголовок с другим написанием значения будет отклонён с `426 Upgrade Required`.
- В WebSocket-режиме HTTP-эндпоинты из раздела выше недоступны, и наоборот.

---

## English

A plugin for remote control of AIMP via HTTP API or WebSocket. Compatible with AIMP version 5.40.2716 and above. Available for Windows (`.dll`). Designed for controlling the player in Bitfocus Companion via the corresponding module. Works with Companion 4.3.2 and above. The Companion module is available in the module catalog.

### Installation

#### Via `.aimppack` package (recommended)

Download `AimpHttpControl.aimppack` from releases and double-click it — AIMP will install the plugin automatically, selecting the correct architecture (x64 or x32).

#### Manual

Copy the appropriate `.dll` directly to the AIMP plugins folder and rename it to `AimpHttpControl.dll`:

```
%APPDATA%\AIMP\Plugins\AimpHttpControl.dll
```

Use `AimpHttpControl64.dll` for 64-bit AIMP, `AimpHttpControl32.dll` for 32-bit.

### Build

Release files are built automatically in GitHub Actions: pushing a tag builds x64 + x32 and publishes the `.aimppack` plus both `.dll` files to a GitHub Release (see `.github/workflows/build-release.yml`).

#### Quick build (x64 + x32 + package)

```bash
bash build_package.sh
```

Produces `AimpHttpControl64.dll`, `AimpHttpControl32.dll` and `AimpHttpControl.aimppack`.

#### Windows (x64)

```bash
x86_64-w64-mingw32-g++ \
    -std=c++17 -O2 -Wall \
    -Wno-missing-braces -Wno-delete-non-virtual-dtor \
    -D_WIN32_WINNT=0x0A00 \
    -Isrc -isystem sdk -isystem third_party \
    src/globals.cpp src/utils.cpp src/network.cpp src/focus_sync.cpp \
    src/settings.cpp src/player_api.cpp src/http_server.cpp src/ws_server.cpp \
    src/options_frame.cpp src/plugin.cpp \
    -shared -o AimpHttpControl64.dll \
    -lws2_32 -liphlpapi -luuid -lkernel32 -luser32 -lgdi32 \
    -static-libgcc -static-libstdc++ \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
```

#### Windows (x32)

```bash
i686-w64-mingw32-g++ \
    -std=c++17 -O2 -Wall \
    -Wno-missing-braces -Wno-delete-non-virtual-dtor \
    -D_WIN32_WINNT=0x0A00 \
    -Isrc -isystem sdk -isystem third_party \
    src/globals.cpp src/utils.cpp src/network.cpp src/focus_sync.cpp \
    src/settings.cpp src/player_api.cpp src/http_server.cpp src/ws_server.cpp \
    src/options_frame.cpp src/plugin.cpp \
    -shared -o AimpHttpControl32.dll \
    -lws2_32 -liphlpapi -luuid -lkernel32 -luser32 -lgdi32 \
    -static-libgcc -static-libstdc++ \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic \
    src/plugin.def
```

### Settings

Accessible via the AIMP menu: **Settings → Plugins → HTTP Remote Control**. All settings are preserved across player restarts.

#### Server

| Parameter | Default | Description |
|-----------|---------|-------------|
| Port | `19122` | Server port (shared by HTTP and WebSocket) |
| Server mode | `HTTP` | `HTTP` — REST API; `WebSocket` — WS server with the same commands plus push events. The modes are mutually exclusive: only one runs on the port, and switching restarts the server on the fly |
| Listen interface | `All interfaces (0.0.0.0)` | `Localhost (127.0.0.1)` — local connections only; `All interfaces (0.0.0.0)` — all interfaces; followed by a list of real network interfaces on the system (adapter name + IP) |

#### Access policy

| Parameter | Default | Description |
|-----------|---------|-------------|
| Allow access only from | disabled | When enabled, only connections from addresses in the list below are accepted |
| IP / CIDR list | — | Comma-separated list of allowed addresses. Supports exact addresses and CIDR ranges, e.g.: `192.168.1.5, 192.168.2.0/24, 10.0.0.0/8` |

When the checkbox is unchecked, connections are accepted from any address (whitelist disabled).

### API Endpoints

#### Player

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/player/status` | Full player status (state, track, volume, position, playlist, focus) |
| POST | `/api/player/play` | Resume playback |
| POST | `/api/player/pause` | Pause |
| POST | `/api/player/stop` | Stop |
| POST | `/api/player/next` | Next track |
| POST | `/api/player/prev` | Previous track |
| GET | `/api/player/volume` | Get volume |
| PUT | `/api/player/volume` | Set volume (body: `{"volume": 75}` or `?volume=75`) |
| POST | `/api/player/mute` | Toggle mute |
| PUT | `/api/player/position` | Set position in seconds (body: `{"position": 30}`) |
| GET | `/api/player/shuffle` | Get shuffle state |
| POST | `/api/player/shuffle` | Toggle shuffle |
| GET | `/api/player/repeat` | Get repeat state |
| POST | `/api/player/repeat` | Toggle repeat |
| GET | `/api/player/auto-jump` | Get auto-jump state |
| POST | `/api/player/auto-jump` | Toggle auto-jump |

#### Focus (playlist navigation without playback)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/focus` | Current focus (playlist + track) |
| POST | `/api/focus/playlist/next` | Focus next playlist |
| POST | `/api/focus/playlist/prev` | Focus previous playlist |
| POST | `/api/focus/track/next` | Focus next track |
| POST | `/api/focus/track/prev` | Focus previous track |
| POST | `/api/focus/play` | Play focused track |

#### Playlists

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/playlists` | List all playlists |
| GET | `/api/playlists/{id}` | Playlist info |
| GET | `/api/playlists/{id}/tracks` | Playlist tracks (`?limit=50&offset=0`) |
| GET | `/api/playlists/{id}/tracks/{tid}` | Track info |
| POST | `/api/playlists/{id}/play` | Play playlist from the first track |
| POST | `/api/playlists/{id}/resume` | Play playlist from the last position (focused track) |
| POST | `/api/playlists/{id}/select` | Set playlist as active |
| POST | `/api/playlists/{id}/tracks/{tid}/play` | Play track |
| POST | `/api/playlists/{id}/tracks/{tid}/select` | Focus track |

### WebSocket

Besides the HTTP server, the plugin can run as a WebSocket server. The mode is switched in the settings: **Server mode → WebSocket**. In this mode the client receives push events from the player instead of polling.

The modes are mutually exclusive: only one of them runs on the selected port. Switching the mode restarts the server on the fly, without restarting the player.

#### Connecting

```
ws://<host>:<port>/
```

The port and listen interface are the same as in HTTP mode (default `19122`), and the access policy (IP/CIDR whitelist) applies here as well. The path is not checked — any path is accepted. Only `ws://` is supported, without TLS.

#### Command format

The client sends a text JSON frame with an `action` field:

```json
{"action": "player/play"}
```

The reply goes only to the sender, with the `action` echoed back for matching:

```json
{"action": "player/play", "state": "playing"}
```

On failure an `error` object is returned:

```json
{"action": "player/volume/set", "error": {"code": "INVALID_VOLUME"}}
```

Error codes: `UNKNOWN_ACTION`, `INVALID_VOLUME`, `INVALID_POSITION`, `INVALID_PLAYLIST_ID`, `INVALID_ID`.

#### Commands

| Action | Parameters | Description |
|--------|------------|-------------|
| `player/status` | — | Full player status |
| `player/play` | — | Resume playback |
| `player/pause` | — | Pause |
| `player/stop` | — | Stop |
| `player/next` | — | Next track |
| `player/prev` | — | Previous track |
| `player/volume/get` | — | Get volume and mute state |
| `player/volume/set` | `volume` (0–100) | Set volume |
| `player/mute` | — | Toggle mute |
| `player/position/set` | `position` (seconds) | Set position |
| `player/shuffle/get` | — | Get shuffle state |
| `player/shuffle/toggle` | — | Toggle shuffle |
| `player/repeat/get` | — | Get repeat state |
| `player/repeat/toggle` | — | Toggle repeat |
| `player/auto-jump/get` | — | Get auto-jump state |
| `player/auto-jump/toggle` | — | Toggle auto-jump |
| `focus/get` | — | Current focus (playlist + track) |
| `focus/playlist/next` | — | Focus next playlist |
| `focus/playlist/prev` | — | Focus previous playlist |
| `focus/track/next` | — | Focus next track |
| `focus/track/prev` | — | Focus previous track |
| `focus/play` | — | Play focused track |
| `playlists/get` | — | List all playlists |
| `playlists/get/one` | `playlist_id` | Playlist info |
| `playlists/tracks/get` | `playlist_id`, `limit`, `offset` | Playlist tracks |
| `playlists/track/get` | `playlist_id`, `track_id` | Track info |
| `playlists/play` | `playlist_id` | Play playlist from the first track |
| `playlists/resume` | `playlist_id` | Play playlist from the last position |
| `playlists/select` | `playlist_id` | Set playlist as active |
| `playlists/track/play` | `playlist_id`, `track_id` | Play track |
| `playlists/track/select` | `playlist_id`, `track_id` | Focus track |

#### Events (push)

Right after the connection is established the client receives the current status with `event: "connected"`. After that the server broadcasts to all connected clients:

| Event | When it fires | Payload |
|-------|---------------|---------|
| `connected` | right after connecting | full player status |
| `player_state_changed` | state change (play/pause/stop) | full player status |
| `track_changed` | a track or stream starts playing | full player status |
| `playback_ended` | playback finished | full player status |
| `property_changed` | a property changed (shuffle, repeat, auto-jump, mute, volume) | full player status |
| `focus_changed` | active playlist or focused track changed | full player status |
| `position` | playback position updated | `position`, `duration`, `remaining` |

The `position` event is lightweight — position only, without playlists or tracks — so it fits a progress bar.

#### Example (JavaScript)

```js
const ws = new WebSocket('ws://127.0.0.1:19122/');

ws.onopen = () => ws.send(JSON.stringify({ action: 'player/status' }));

ws.onmessage = (e) => {
  const msg = JSON.parse(e.data);
  if (msg.event === 'position') {
    console.log(`${msg.position} / ${msg.duration}`);
  } else if (msg.event) {
    console.log('event:', msg.event, msg);
  } else if (msg.action) {
    console.log('reply to', msg.action, msg);
  }
};
```

#### Limitations

- `ws://` only, no TLS (`wss://` is not supported) and no subprotocol negotiation.
- Fragmented messages are not supported: a command must arrive in a single frame, and the incoming frame size is limited to 64 KiB.
- Only text frames are handled (plus the close/ping control frames); binary data frames are ignored.
- The upgrade header must use the standard form `Upgrade: websocket` — a header with different value casing is rejected with `426 Upgrade Required`.
- In WebSocket mode the HTTP endpoints from the section above are unavailable, and vice versa.
