#pragma once
#include "pch.h"

// Запустить WebSocket сервер (блокирующий, вызывается в отдельном потоке)
void RunWsServer();

// Перезапустить сервер (HTTP или WS, в зависимости от g_serverMode)
void RestartServer();

// Разослать JSON-сообщение всем подключённым WS-клиентам
void WsBroadcast(const std::string& jsonStr);
