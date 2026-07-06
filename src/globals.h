#pragma once

#include <string>
#include "../sdk/amx/amx.h"

// --- Plugin-wide globals (definisi ada di globals.cpp) ---

extern void *pAMXFunctions;
extern void (*logprintf)(const char *format, ...);

extern AMX *g_amx;

extern int g_listenSocket;
extern int g_clientSocket;
extern std::string g_recvBuffer;

extern int PAWN_MOCKER_PORT;
extern size_t g_maxRecvBuffer;
