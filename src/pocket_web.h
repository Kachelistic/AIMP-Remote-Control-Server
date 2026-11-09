#pragma once
#include "globals.h"

// Resolve relative to the loaded DLL, never to AIMP's working directory.
// Only this fixed file is exposed; URL paths are not mapped to filesystem paths.
inline bool LoadPocketPage(std::string& html) {
    wchar_t modulePath[32768];
    const DWORD count = GetModuleFileNameW(g_hInstance, modulePath, 32768);
    if (count == 0 || count >= 32768) return false;
    const std::wstring module(modulePath, count);
    const size_t separator = module.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return false;
    const std::wstring path = module.substr(0, separator + 1) + L"web\\pocket.html";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    // Bound allocation and transfer time; empty/incomplete saves use the fallback.
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    html.resize(static_cast<size_t>(size.QuadPart));
    size_t offset = 0;
    while (offset < html.size()) {
        DWORD received = 0;
        if (!ReadFile(file, &html[offset], static_cast<DWORD>(html.size() - offset), &received, nullptr) || received == 0) {
            CloseHandle(file);
            return false;
        }
        offset += received;
    }
    CloseHandle(file);
    return true;
}

// The caller has already applied the original IP allow-list.
inline void SendPocketPage(SOCKET client) {
    std::string html;
    if (!LoadPocketPage(html)) {
        const char error[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 35\r\nConnection: close\r\n\r\nMissing or unreadable pocket.html\r\n";
        send(client, error, sizeof(error)-1, 0);
        return;
    }
    const size_t length = html.size();
    const std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "Content-Security-Policy: default-src 'self' data:; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'\r\n"
        "Content-Length: " + std::to_string(length) + "\r\n"
        "Connection: close\r\n\r\n";
    auto sendAll = [client](const char* data, size_t remaining) {
        while (remaining) {
            const int chunk = static_cast<int>(std::min(remaining, size_t(16384)));
            const int sent = send(client, data, chunk, 0);
            if (sent <= 0) return false;
            data += sent;
            remaining -= static_cast<size_t>(sent);
        }
        return true;
    };
    if (sendAll(headers.data(), headers.size())) sendAll(html.data(), length);
}
