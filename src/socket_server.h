#pragma once

// Opens a listening socket on 127.0.0.1:port. Returns false on failure.
bool InitListenSocket(int port);

// Closes the client & listen sockets (called during Unload).
void CloseSockets();

// Non-blocking accept + reads one line of JSON, dispatches it, sends back the response.
// Called on every ProcessTick().
void PollSocket();