#include "http_server.h"
#include "globals.h"
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    g_port = std::stoi(argv[1]);
    g_bindMode = 0;
    RunHttpServer();
    return 0;
}
