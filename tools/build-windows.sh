#!/usr/bin/env bash
set -euo pipefail
mkdir -p build
sources=(src/globals.cpp src/utils.cpp src/network.cpp src/focus_sync.cpp
 src/settings.cpp src/player_api.cpp src/http_server.cpp src/ws_server.cpp
 src/options_frame.cpp src/plugin.cpp)
flags=(-std=c++17 -O2 -Wall -Wno-missing-braces -Wno-delete-non-virtual-dtor
 -D_WIN32_WINNT=0x0A00 -Isrc -isystem sdk -isystem third_party)
libs=(-lws2_32 -liphlpapi -luuid -lkernel32 -luser32 -lgdi32 -static -static-libgcc -static-libstdc++)
/mingw64/bin/g++ "${flags[@]}" "${sources[@]}" -shared -o build/AimpHttpControl64.dll "${libs[@]}"
/mingw32/bin/g++ "${flags[@]}" "${sources[@]}" src/plugin.def -shared -o build/AimpHttpControl32.dll "${libs[@]}"
/mingw64/bin/g++ "${flags[@]}" "${sources[@]}" tests/http-harness.cpp -o build/http-pocket.exe "${libs[@]}"
git show b42fccd65c964535fcfbdcc760967ccb27b48bf4:src/http_server.cpp > build/http-original.cpp
original=()
for source in "${sources[@]}"; do
 if [[ "$source" == src/http_server.cpp ]]; then original+=(build/http-original.cpp); else original+=("$source"); fi
done
/mingw64/bin/g++ "${flags[@]}" "${original[@]}" tests/http-harness.cpp -o build/http-original.exe "${libs[@]}"
/mingw64/bin/objdump -p build/AimpHttpControl64.dll > build/pe64.txt
/mingw32/bin/objdump -p build/AimpHttpControl32.dll > build/pe32.txt
