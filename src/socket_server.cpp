#include "socket_server.h"
#include "globals.h"
#include "handlers.h"
#include "../json.hpp"

#ifdef _WIN32
  #include <winsock2.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef int socklen_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #define closesocket close
#endif

using json = nlohmann::json;

static void SetNonBlocking(int sock) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool InitListenSocket(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    g_listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    bind(g_listenSocket, (sockaddr *)&addr, sizeof(addr));
    listen(g_listenSocket, 1);
    SetNonBlocking(g_listenSocket);

    return true;
}

void CloseSockets() {
    if (g_clientSocket != -1) closesocket(g_clientSocket);
    if (g_listenSocket != -1) closesocket(g_listenSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}

void PollSocket() {
    if (g_clientSocket == -1) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int s = accept(g_listenSocket, (sockaddr *)&clientAddr, &len);
        if (s >= 0) {
            SetNonBlocking(s);
            g_clientSocket = s;
            logprintf("[PAWN-MOCKER] Node client connected");
        }
        return;
    }

    char buf[4096];
    int n = recv(g_clientSocket, buf, sizeof(buf), 0);
    if (n > 0) {
        g_recvBuffer.append(buf, n);

        if (g_recvBuffer.size() > g_maxRecvBuffer) {
            logprintf("[PAWN-MOCKER] recv buffer kelewat gede (%zu bytes, limit %zu), reset paksa (kemungkinan garbage tanpa newline)",
                g_recvBuffer.size(), g_maxRecvBuffer);
            g_recvBuffer.clear();
            return;
        }

        size_t pos;
        while ((pos = g_recvBuffer.find('\n')) != std::string::npos) {
            std::string line = g_recvBuffer.substr(0, pos);
            g_recvBuffer.erase(0, pos + 1);
            if (line.empty()) continue;

            json response;
            json cmd;
            bool cmdParsedOk = true;

            try {
                cmd = json::parse(line);
            } catch (const std::exception &e) {
                cmdParsedOk = false;
                response["error"] = std::string("JSON parse error: ") + e.what();
            }

            if (cmdParsedOk) {
                try {
                    response = Dispatch(cmd);
                } catch (const std::exception &e) {
                    response = json::object();
                    response["error"] = std::string("dispatch error: ") + e.what();
                }
                if (cmd.contains("id")) {
                    response["id"] = cmd["id"];
                }
            }

            std::string out = response.dump() + "\n";
            size_t totalSent = 0;
            while (totalSent < out.size()) {
                int sent = send(g_clientSocket, out.c_str() + totalSent, (int)(out.size() - totalSent), 0);
                if (sent <= 0) {
                    logprintf("[PAWN-MOCKER] send() gagal/partial, sisa %zu bytes gak terkirim", out.size() - totalSent);
                    break;
                }
                totalSent += (size_t)sent;
            }
        }
    } else if (n == 0) {
        closesocket(g_clientSocket);
        g_clientSocket = -1;
        g_recvBuffer.clear();
        logprintf("[PAWN-MOCKER] Node client disconnected (clean)");
    } else {
        #ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
            closesocket(g_clientSocket);
            g_clientSocket = -1;
            g_recvBuffer.clear();
            logprintf("[PAWN-MOCKER] Node client disconnected (forced error: %d)", err);
        }
        #else
        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINPROGRESS) {
            closesocket(g_clientSocket);
            g_clientSocket = -1;
            g_recvBuffer.clear();
            logprintf("[PAWN-MOCKER] Node client disconnected (forced error: %d)", errno);
        }
        #endif
    }
}
