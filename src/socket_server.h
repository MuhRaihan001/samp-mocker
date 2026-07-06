#pragma once

// Buka listening socket di 127.0.0.1:port. Return false kalau gagal.
bool InitListenSocket(int port);

// Tutup client & listen socket (dipanggil pas Unload).
void CloseSockets();

// Non-blocking accept + baca 1 baris JSON, dispatch, kirim balik response.
// Dipanggil tiap ProcessTick().
void PollSocket();
