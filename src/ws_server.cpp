#include "ws_server.h"
#include "http_server.h"
#include "globals.h"
#include "network.h"
#include "player_api.h"
#include "focus_sync.h"

// ==========================================
// SHA-1 (RFC 3174) — нужен для WS handshake
// ==========================================
static void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    auto rotl = [](uint32_t v, int n){ return (v<<n)|(v>>(32-n)); };

    // Padding
    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    uint64_t bitLen = (uint64_t)len * 8;
    for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)(bitLen >> (i*8)));

    for (size_t i = 0; i < msg.size(); i += 64) {
        uint32_t w[80];
        for (int j = 0; j < 16; j++)
            w[j] = ((uint32_t)msg[i+j*4]<<24)|((uint32_t)msg[i+j*4+1]<<16)|
                   ((uint32_t)msg[i+j*4+2]<<8)|(uint32_t)msg[i+j*4+3];
        for (int j = 16; j < 80; j++)
            w[j] = rotl(w[j-3]^w[j-8]^w[j-14]^w[j-16], 1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for (int j = 0; j < 80; j++) {
            uint32_t f,k;
            if      (j<20){f=(b&c)|((~b)&d);k=0x5A827999;}
            else if (j<40){f=b^c^d;         k=0x6ED9EBA1;}
            else if (j<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else          {f=b^c^d;         k=0xCA62C1D6;}
            uint32_t tmp=rotl(a,5)+f+e+k+w[j];
            e=d; d=c; c=rotl(b,30); b=a; a=tmp;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
    }
    for (int i = 0; i < 5; i++)
        for (int j = 3; j >= 0; j--)
            out[i*4+(3-j)] = (uint8_t)(h[i]>>(j*8));
}

// Base64 encode
static std::string base64Encode(const uint8_t* data, size_t len) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v  = (uint32_t)data[i] << 16;
        if (i+1 < len) v |= (uint32_t)data[i+1] << 8;
        if (i+2 < len) v |= (uint32_t)data[i+2];
        out += t[(v>>18)&63]; out += t[(v>>12)&63];
        out += (i+1 < len) ? t[(v>>6)&63] : '=';
        out += (i+2 < len) ? t[v&63]      : '=';
    }
    return out;
}

// ==========================================
// Список подключённых клиентов
// ==========================================
static std::mutex             s_wsMutex;
static std::vector<SOCKET>    s_wsClients;

static void wsAddClient(SOCKET s) {
    std::lock_guard<std::mutex> lk(s_wsMutex);
    s_wsClients.push_back(s);
}

static void wsRemoveClient(SOCKET s) {
    std::lock_guard<std::mutex> lk(s_wsMutex);
    s_wsClients.erase(std::remove(s_wsClients.begin(), s_wsClients.end(), s), s_wsClients.end());
}

// ==========================================
// Encode WebSocket text frame (RFC 6455)
// ==========================================
static std::string wsMakeFrame(const std::string& payload) {
    std::string frame;
    frame.push_back((char)0x81); // FIN + opcode text
    size_t len = payload.size();
    if (len <= 125) {
        frame.push_back((char)len);
    } else if (len <= 65535) {
        frame.push_back((char)126);
        frame.push_back((char)((len >> 8) & 0xFF));
        frame.push_back((char)(len & 0xFF));
    } else {
        frame.push_back((char)127);
        for (int i = 7; i >= 0; i--)
            frame.push_back((char)((len >> (i*8)) & 0xFF));
    }
    frame.append(payload);
    return frame;
}

// ==========================================
// WsBroadcast — рассылка всем клиентам
// ==========================================
void WsBroadcast(const std::string& jsonStr) {
    std::string frame = wsMakeFrame(jsonStr);
    std::vector<SOCKET> clients;
    {
        std::lock_guard<std::mutex> lk(s_wsMutex);
        clients = s_wsClients;
    }
    std::vector<SOCKET> dead;
    for (SOCKET s : clients) {
        int r = send(s, frame.c_str(), (int)frame.size(), 0);
        if (r == SOCKET_ERROR) dead.push_back(s);
    }
    if (!dead.empty()) {
        std::lock_guard<std::mutex> lk(s_wsMutex);
        for (SOCKET s : dead) {
            closesocket(s);
            s_wsClients.erase(std::remove(s_wsClients.begin(), s_wsClients.end(), s), s_wsClients.end());
        }
    }
}

// ==========================================
// Выполнить WebSocket handshake
// ==========================================
static bool wsDoHandshake(SOCKET cl, const std::string& raw) {
    // Найти Sec-WebSocket-Key
    const std::string keyHeader = "Sec-WebSocket-Key:";
    size_t pos = raw.find(keyHeader);
    if (pos == std::string::npos) return false;
    pos += keyHeader.size();
    while (pos < raw.size() && (raw[pos]==' '||raw[pos]=='\t')) pos++;
    size_t end = raw.find('\r', pos);
    if (end == std::string::npos) end = raw.find('\n', pos);
    if (end == std::string::npos) return false;
    std::string key = raw.substr(pos, end - pos);
    // Trim trailing whitespace
    while (!key.empty() && (key.back()==' '||key.back()=='\r'||key.back()=='\n'))
        key.pop_back();

    // Compute accept key: SHA1(key + GUID), then base64
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    uint8_t hash[20];
    sha1((const uint8_t*)combined.c_str(), combined.size(), hash);
    std::string accept = base64Encode(hash, 20);

    std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    send(cl, resp.c_str(), (int)resp.size(), 0);
    return true;
}

// MSG_WAITALL на Winsock ненадёжен — используем цикл
static bool recvAll(SOCKET cl, char* buf, int needed) {
    int got = 0;
    while (got < needed) {
        int r = recv(cl, buf + got, needed - got, 0);
        if (r <= 0) return false;
        got += r;
    }
    return true;
}

// ==========================================
// Чтение одного WS фрейма от клиента
// Возвращает false при ошибке или закрытии
// ==========================================
static bool wsReadFrame(SOCKET cl, std::string& payload) {
    uint8_t hdr[2] = {};
    if (!recvAll(cl, (char*)hdr, 2)) return false;

    bool fin       = (hdr[0] & 0x80) != 0;
    uint8_t opcode =  hdr[0] & 0x0F;
    bool masked    = (hdr[1] & 0x80) != 0;
    uint64_t payLen = hdr[1] & 0x7F;

    if (payLen == 126) {
        uint8_t ext[2];
        if (!recvAll(cl, (char*)ext, 2)) return false;
        payLen = ((uint64_t)ext[0]<<8) | ext[1];
    } else if (payLen == 127) {
        uint8_t ext[8];
        if (!recvAll(cl, (char*)ext, 8)) return false;
        payLen = 0;
        for (int i = 0; i < 8; i++) payLen = (payLen << 8) | ext[i];
    }

    uint8_t mask[4] = {};
    if (masked) {
        if (!recvAll(cl, (char*)mask, 4)) return false;
    }

    if (payLen > 65536) return false;

    payload.resize((size_t)payLen);
    if (payLen > 0) {
        if (!recvAll(cl, &payload[0], (int)payLen)) return false;
        if (masked) {
            for (size_t i = 0; i < payLen; i++)
                payload[i] ^= mask[i % 4];
        }
    }

    if (opcode == 0x08) return false; // close
    if (opcode == 0x09) {             // ping → pong
        std::string pong = wsMakeFrame(payload);
        pong[0] = (char)0x8A;
        send(cl, pong.c_str(), (int)pong.size(), 0);
    }
    (void)fin;
    return true;
}

// ==========================================
// Обслуживание одного WS-клиента в потоке
// ==========================================
static void wsClientThread(SOCKET cl) {
    // Отправить начальный статус при подключении
    {
        json status = GetPlayerStatus();
        status["event"] = "connected";
        WsBroadcast(""); // dummy, клиент ещё не в списке
        // Шлём напрямую клиенту
        std::string frame = wsMakeFrame(status.dump());
        send(cl, frame.c_str(), (int)frame.size(), 0);
    }

    wsAddClient(cl);

    while (g_running) {
        // Проверяем есть ли данные (с таймаутом 5с для keep-alive)
        fd_set rs; FD_ZERO(&rs); FD_SET(cl, &rs);
        timeval to{5, 0};
        int sel = select(0, &rs, nullptr, nullptr, &to);
        if (sel < 0) break;
        if (sel == 0) continue; // timeout, продолжаем

        std::string payload;
        if (!wsReadFrame(cl, payload)) break;
        if (payload.empty()) continue;

        // Обработка входящей команды.
        // Протокол: {"action": "...", ...параметры...}
        // Ответ шлём только этому клиенту (broadcast придёт через IAIMPMessageHook автоматически).
        try {
            json cmd = json::parse(payload);
            std::string action = cmd.value("action", "");
            json rsp;
            bool sendReply = true; // отправить ответ клиенту

            if (action == "player/status") {
                rsp = GetPlayerStatus();
                rsp["event"] = "status";

            } else if (action == "player/play") {
                PlayerPlay(); rsp["state"] = "playing";

            } else if (action == "player/pause") {
                IAIMPServicePlayer* p = nullptr;
                if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) { p->Pause(); rsp["state"]="paused"; p->Release(); }

            } else if (action == "player/stop") {
                IAIMPServicePlayer* p = nullptr;
                if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) { p->Stop(); rsp["state"]="stopped"; p->Release(); }

            } else if (action == "player/next") {
                IAIMPServicePlayer* p = nullptr;
                if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) { p->GoToNext(); p->Release(); rsp["ok"]=true; }

            } else if (action == "player/prev") {
                IAIMPServicePlayer* p = nullptr;
                if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) { p->GoToPrev(); p->Release(); rsp["ok"]=true; }

            } else if (action == "player/volume/get") {
                IAIMPServicePlayer* p = nullptr;
                if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) {
                    float v=0; BOOL m=FALSE; p->GetVolume(&v); p->GetMute(&m);
                    rsp["volume"]=(int)(v*100); rsp["muted"]=(m!=FALSE); p->Release();
                }

            } else if (action == "player/volume/set") {
                float v = -1;
                if (cmd.contains("volume")) try { v = cmd["volume"].get<float>() / 100.f; } catch(...) {}
                if (v >= 0.f && v <= 1.f) {
                    IAIMPServicePlayer* p = nullptr;
                    if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) { p->SetVolume(v); rsp["volume"]=(int)(v*100); p->Release(); }
                } else { rsp["error"]["code"]="INVALID_VOLUME"; }

            } else if (action == "player/mute") {
                IAIMPServicePlayer* p = nullptr;
                if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) {
                    BOOL m=FALSE; p->GetMute(&m); p->SetMute(!m); rsp["muted"]=(m==FALSE); p->Release();
                }

            } else if (action == "player/position/set") {
                double pos = -1;
                if (cmd.contains("position")) try { pos = cmd["position"].get<double>(); } catch(...) {}
                if (pos >= 0) {
                    IAIMPServicePlayer* p = nullptr;
                    if (g_core && g_core->QueryInterface(IID_IAIMPServicePlayer,(void**)&p)==S_OK && p) { p->SetPosition(pos); rsp["position"]=pos; p->Release(); }
                } else { rsp["error"]["code"]="INVALID_POSITION"; }

            } else if (action == "player/shuffle/get") {
                rsp["shuffle"] = MsgGetBool(AIMP_MSG_PROPERTY_SHUFFLE);

            } else if (action == "player/shuffle/toggle") {
                rsp["shuffle"] = MsgToggleBool(AIMP_MSG_PROPERTY_SHUFFLE);

            } else if (action == "player/repeat/get") {
                rsp["repeat"] = MsgGetBool(AIMP_MSG_PROPERTY_REPEAT);

            } else if (action == "player/repeat/toggle") {
                rsp["repeat"] = MsgToggleBool(AIMP_MSG_PROPERTY_REPEAT);

            } else if (action == "player/auto-jump/get") {
                rsp["auto_jump"] = MsgGetBool(AIMP_MSG_PROPERTY_AUTOJUMP_TO_NEXT_TRACK);

            } else if (action == "player/auto-jump/toggle") {
                rsp["auto_jump"] = MsgToggleBool(AIMP_MSG_PROPERTY_AUTOJUMP_TO_NEXT_TRACK);

            } else if (action == "focus/get") {
                int plIdx, trIdx;
                { std::lock_guard<std::mutex> lk(g_focusMutex); plIdx = g_focusPlaylistIdx; trIdx = g_focusTrackIdx; }
                RunInMainThread([&]() {
                    IAIMPServicePlaylistManager* mgr = nullptr;
                    if (g_core->QueryInterface(IID_IAIMPServicePlaylistManager, (void**)&mgr) != S_OK || !mgr) {
                        rsp["error"] = "playlist manager unavailable"; return;
                    }
                    json fp; FillPlaylistJson(mgr, plIdx, fp); rsp["focus_playlist"] = fp;
                    json ft; FillTrackJson(mgr, plIdx, trIdx, ft); rsp["focus_track"] = ft;
                    mgr->Release();
                });

            } else if (action == "focus/playlist/next") {
                rsp = FocusPlaylistShift(+1);

            } else if (action == "focus/playlist/prev") {
                rsp = FocusPlaylistShift(-1);

            } else if (action == "focus/track/next") {
                rsp = FocusTrackShift(+1);

            } else if (action == "focus/track/prev") {
                rsp = FocusTrackShift(-1);

            } else if (action == "focus/play") {
                int plIdx, trIdx;
                { std::lock_guard<std::mutex> lk(g_focusMutex); plIdx = g_focusPlaylistIdx; trIdx = g_focusTrackIdx; }
                int code = 200;
                ParsedPath pp; pp.playlistId = plIdx; pp.trackId = trIdx; pp.action = "play";
                DoPlaylistAction(pp, "POST", "", rsp, code);

            } else if (action == "playlists/get") {
                rsp = GetPlaylistsResponse();

            } else if (action == "playlists/get/one") {
                // {"action":"playlists/get/one","playlist_id":0}
                int plId = cmd.value("playlist_id", -1);
                if (plId >= 0) rsp = GetPlaylistResponse(plId);
                else rsp["error"]["code"] = "INVALID_PLAYLIST_ID";

            } else if (action == "playlists/tracks/get") {
                // {"action":"playlists/tracks/get","playlist_id":0,"limit":50,"offset":0}
                int plId  = cmd.value("playlist_id", -1);
                int limit = cmd.value("limit", 50);
                int off   = cmd.value("offset", 0);
                if (plId >= 0) rsp = GetTracksResponse(plId, limit, off);
                else rsp["error"]["code"] = "INVALID_PLAYLIST_ID";

            } else if (action == "playlists/track/get") {
                // {"action":"playlists/track/get","playlist_id":0,"track_id":5}
                int plId = cmd.value("playlist_id", -1);
                int trId = cmd.value("track_id", -1);
                if (plId >= 0 && trId >= 0) rsp = GetTrackResponse(plId, trId);
                else rsp["error"]["code"] = "INVALID_ID";

            } else if (action == "playlists/play") {
                // {"action":"playlists/play","playlist_id":0}
                int plId = cmd.value("playlist_id", -1);
                int code = 200;
                ParsedPath pp; pp.playlistId = plId; pp.trackId = -1; pp.action = "play";
                if (plId >= 0) DoPlaylistAction(pp, "POST", "", rsp, code);
                else rsp["error"]["code"] = "INVALID_PLAYLIST_ID";

            } else if (action == "playlists/resume") {
                // {"action":"playlists/resume","playlist_id":0}
                int plId = cmd.value("playlist_id", -1);
                int code = 200;
                ParsedPath pp; pp.playlistId = plId; pp.trackId = -1; pp.action = "resume";
                if (plId >= 0) DoPlaylistAction(pp, "POST", "", rsp, code);
                else rsp["error"]["code"] = "INVALID_PLAYLIST_ID";

            } else if (action == "playlists/select") {
                // {"action":"playlists/select","playlist_id":0}
                int plId = cmd.value("playlist_id", -1);
                int code = 200;
                ParsedPath pp; pp.playlistId = plId; pp.trackId = -1; pp.action = "select";
                if (plId >= 0) DoPlaylistAction(pp, "POST", "", rsp, code);
                else rsp["error"]["code"] = "INVALID_PLAYLIST_ID";

            } else if (action == "playlists/track/play") {
                // {"action":"playlists/track/play","playlist_id":0,"track_id":5}
                int plId = cmd.value("playlist_id", -1);
                int trId = cmd.value("track_id", -1);
                int code = 200;
                ParsedPath pp; pp.playlistId = plId; pp.trackId = trId; pp.action = "play";
                if (plId >= 0 && trId >= 0) DoPlaylistAction(pp, "POST", "", rsp, code);
                else rsp["error"]["code"] = "INVALID_ID";

            } else if (action == "playlists/track/select") {
                // {"action":"playlists/track/select","playlist_id":0,"track_id":5}
                int plId = cmd.value("playlist_id", -1);
                int trId = cmd.value("track_id", -1);
                int code = 200;
                ParsedPath pp; pp.playlistId = plId; pp.trackId = trId; pp.action = "select";
                if (plId >= 0 && trId >= 0) DoPlaylistAction(pp, "POST", "", rsp, code);
                else rsp["error"]["code"] = "INVALID_ID";

            } else {
                rsp["error"]["code"] = "UNKNOWN_ACTION";
                rsp["action"] = action;
                sendReply = true;
            }

            if (sendReply && !rsp.is_null() && !rsp.empty()) {
                rsp["action"] = action; // эхо action для идентификации ответа
                std::string frame = wsMakeFrame(rsp.dump());
                send(cl, frame.c_str(), (int)frame.size(), 0);
            }
        } catch (...) {
            // Невалидный JSON — игнорируем
        }
    }

    wsRemoveClient(cl);
    closesocket(cl);
}

// ==========================================
// RunWsServer — основной цикл
// ==========================================
void RunWsServer() {
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { WSACleanup(); return; }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(g_port);
    if (g_bindMode == 0) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (g_bindMode == 2 && !g_bindIp.empty()) {
        if (InetPtonW(AF_INET, g_bindIp.c_str(), &addr.sin_addr) != 1)
            addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) { closesocket(srv); WSACleanup(); return; }
    listen(srv, SOMAXCONN);
    g_serverSocket = srv;
    g_running = true;

    while (g_running) {
        fd_set rs; FD_ZERO(&rs); FD_SET(srv, &rs);
        timeval to{1, 0};
        if (select(0, &rs, nullptr, nullptr, &to) <= 0) continue;
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET cl = accept(srv, (sockaddr*)&clientAddr, &clientLen);
        if (cl == INVALID_SOCKET) continue;

        if (!IsAllowedClient(clientAddr.sin_addr.s_addr)) {
            closesocket(cl); continue;
        }

        // Читаем HTTP Upgrade запрос
        char buf[4096];
        int n = recv(cl, buf, sizeof(buf)-1, 0);
        if (n <= 0) { closesocket(cl); continue; }
        buf[n] = 0;
        std::string raw(buf, n);

        // Проверяем, что это WebSocket upgrade
        if (raw.find("Upgrade: websocket") == std::string::npos &&
            raw.find("Upgrade: WebSocket") == std::string::npos) {
            // Не WS — отвечаем кратким HTTP ответом
            std::string notws =
                "HTTP/1.1 426 Upgrade Required\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n";
            send(cl, notws.c_str(), (int)notws.size(), 0);
            closesocket(cl); continue;
        }

        if (!wsDoHandshake(cl, raw)) { closesocket(cl); continue; }

        // Запускаем клиентский поток
        std::thread(wsClientThread, cl).detach();
    }

    // Закрываем все клиентские соединения
    {
        std::lock_guard<std::mutex> lk(s_wsMutex);
        for (SOCKET s : s_wsClients) closesocket(s);
        s_wsClients.clear();
    }

    g_serverSocket = INVALID_SOCKET;
    closesocket(srv);
    WSACleanup();
}

// ==========================================
// RestartServer — HTTP или WS в зависимости от режима
// ==========================================
void RestartServer() {
    g_running = false;
    if (g_serverSocket != INVALID_SOCKET) {
        closesocket(g_serverSocket);
        g_serverSocket = INVALID_SOCKET;
    }
    Sleep(300);
    if (g_serverMode == 1)
        g_serverThread = std::thread(RunWsServer);
    else
        g_serverThread = std::thread(RunHttpServer);
    g_serverThread.detach();
}
