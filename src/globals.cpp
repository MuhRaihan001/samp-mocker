#include "globals.h"

void (*logprintf)(const char *format, ...) = nullptr;

AMX *g_amx = nullptr;

int g_listenSocket = -1;
int g_clientSocket = -1;
std::string g_recvBuffer;

int PAWN_MOCKER_PORT = 7777;
size_t g_maxRecvBuffer = 4 * 1024 * 1024; // 4MB default