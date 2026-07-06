#include <cstdio>
#include <cstdlib>

#include "sdk/amx/amx.h"
#include "sdk/plugincommon.h"

#include "src/globals.h"
#include "src/mock_native.h"
#include "src/socket_server.h"

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    logprintf = (void (*)(const char *, ...))ppData[PLUGIN_DATA_LOGPRINTF];

    if (const char *envVal = std::getenv("PAWN_MOCKER_MAX_RECV_BUFFER")) {
        try {
            size_t parsed = (size_t)std::stoul(envVal);
            if (parsed > 0) {
                g_maxRecvBuffer = parsed;
            }
        } catch (...) {
            logprintf("[PAWN-MOCKER] PAWN_MOCKER_MAX_RECV_BUFFER invalid, pakai default");
        }
    }

    InitListenSocket(PAWN_MOCKER_PORT);

    logprintf("[PAWN-MOCKER] listening on 127.0.0.1:%d (maxRecvBuffer=%zu bytes)", PAWN_MOCKER_PORT, g_maxRecvBuffer);
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    ResetAllMocks(g_amx);
    CloseSockets();
}

PLUGIN_EXPORT void PLUGIN_CALL AmxLoad(AMX *amx) {
    if (!g_amx) {
        g_amx = amx;
        logprintf("[PAWN-MOCKER] AMX terdaftar sebagai target callback");
    }
}

PLUGIN_EXPORT void PLUGIN_CALL AmxUnload(AMX *amx) {
    if (g_amx == amx) {
        ResetAllMocks(amx);
        g_amx = nullptr;
    }
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() {
    PollSocket();
}

// --- Implementasi modul ---
// Di-include langsung (bukan link object terpisah) supaya build tetap
// cukup arahkan satu file: index.cpp. Kalau mau, ini bisa diganti jadi
// compile terpisah tiap .cpp lalu di-link, tinggal hapus include di bawah
// dan tambahkan src/*.cpp ke daftar source file compiler/CMake-mu.
#include "src/globals.cpp"
#include "src/amx_native_utils.cpp"
#include "src/mock_native.cpp"
#include "src/handlers.cpp"
#include "src/socket_server.cpp"
