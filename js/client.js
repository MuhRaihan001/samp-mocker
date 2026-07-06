const net = require('net');

class SampMockClient {
  constructor(host = '127.0.0.1', port = 7777, opts = {}) {
    this.host = host;
    this.port = port;
    this.socket = null;
    this.buffer = '';
    this.pending = new Map();
    this.nextId = 1;
    this.timeoutMs = opts.timeoutMs ?? 5000;
    this.verbose = opts.verbose ?? false;
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection(this.port, this.host, () => resolve());
      this.socket.setEncoding('utf8');

      this.socket.on('error', (err) => {
        reject(err);
        this._rejectAllPending(err);
      });

      this.socket.on('data', (chunk) => this._onData(chunk));

      this.socket.on('close', () => {
        this._rejectAllPending(new Error('koneksi ke mock_bridge terputus'));
      });
    });
  }

  _rejectAllPending(err) {
    for (const [, req] of this.pending) {
      clearTimeout(req.timer);
      req.reject(err);
    }
    this.pending.clear();
  }

  _onData(chunk) {
    this.buffer += chunk;
    let idx;

    while ((idx = this.buffer.indexOf('\n')) >= 0) {
      const line = this.buffer.slice(0, idx);
      this.buffer = this.buffer.slice(idx + 1);

      if (!line.trim()) continue;

      let resp;
      try {
        resp = JSON.parse(line);
        if (this.verbose) console.log('RESP', resp);
      } catch {
        console.error('respon plugin bukan JSON valid, diabaikan:', line);
        continue;
      }

      const id = resp.id;

      if (id === undefined || !this.pending.has(id)) {
        if (this.verbose) {
          console.warn('respon tanpa id yang cocok dengan pending request:', resp);
        }
        continue;
      }

      const req = this.pending.get(id);
      this.pending.delete(id);
      clearTimeout(req.timer);

      if (resp.error) req.reject(new Error(resp.error));
      else req.resolve(resp);
    }
  }

  _send(payload) {
    if (!this.socket) {
      return Promise.reject(new Error('belum connect, panggil .connect() dulu'));
    }

    const id = this.nextId++;
    const fullPayload = { ...payload, id };

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`timeout (${this.timeoutMs}ms) nunggu respon untuk action "${payload.action}"`));
      }, this.timeoutMs);

      this.pending.set(id, { resolve, reject, timer });
      this.socket.write(JSON.stringify(fullPayload) + '\n');
    });
  }

  invoke(name, params = []) {
    return this._send({ action: 'invoke', name, params });
  }

  trigger(name, params = []) {
    return this.invoke(name, params);
  }

  mockCallback(name, params = []) {
    return this.invoke(name, params);
  }

  getVar(name, type = 'i') {
    return this._send({ action: 'getvar', name, type });
  }

  setVar(name, value, type = 'i') {
    return this._send({ action: 'setvar', name, value, type });
  }

  mockNative(name, options = {}) {
    return this._send({
      action: 'mocknative',
      name,
      returnValue: options.returnValue,
      writes: options.writes,
      // Tipe tiap parameter native ini, index 0 = param pertama.
      // Contoh mysql_tquery(handle, query, callback, format, ...vararg):
      // ['i', 's', 's', 's'] -> param ke-3 (callback) & ke-4 (format) didecode
      // sebagai string di getNativeCalls(), bukan raw AMX address.
      paramTypes: options.paramTypes,
    });
  }

  unmockNative(name) {
    return this._send({ action: 'unmocknative', name });
  }

  async getNativeCalls(name) {
    const resp = await this._send({ action: 'getnativecalls', name });
    return resp.calls || [];
  }

  resetMocks() {
    return this._send({ action: 'resetmocks' });
  }

  ping() {
    return this._send({ action: 'ping' });
  }

  close() {
    if (this.socket) this.socket.end();
  }
}

const P = {
  int: (value) => ({ type: 'i', value }),
  float: (value) => ({ type: 'f', value }),
  str: (value) => ({ type: 's', value }),
};

module.exports = { SampMockClient, P };