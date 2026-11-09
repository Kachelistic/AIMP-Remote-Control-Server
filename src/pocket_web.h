#pragma once
#include "pocket_embedded.h"

// The caller has already applied the original IP allow-list.
inline void SendPocketPage(SOCKET client) {
    const size_t length = sizeof(kAimpPocketHtml) - 1;
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
    if (sendAll(headers.data(), headers.size())) sendAll(kAimpPocketHtml, length);
}
