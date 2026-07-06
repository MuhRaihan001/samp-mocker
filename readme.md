# pawn-mocker

A SA-MP server plugin that exposes a local TCP/JSON bridge for triggering
Pawn callbacks, reading/writing pubvars, and mocking natives directly from
Node.js. Built for writing gamemode unit tests without a real client.

## How it works

The plugin opens a non-blocking TCP listener on `127.0.0.1:7777` and speaks
newline-delimited JSON. Each request is a single JSON object per line; each
response is a single JSON object per line, matched back by an `id` field.

From Node you connect with the bundled client (`client.js`) and can:

- invoke a `public` callback in the gamemode
- get/set a pubvar
- mock a native (override its return value and/or write to its
  out-parameters) and inspect how it was called
- reset all mocks back to their original native addresses

## Installation

1. Build the plugin (`index.cpp` pulls in all the other `.cpp` files via
   `#include`, so compiling just `index.cpp` is enough) into your platform's
   plugin binary (`.dll` on Windows, `.so` on Linux).
2. Copy the compiled binary straight into your SA-MP server's `plugins/`
   folder.
3. Add it to `server.cfg`:

   ```
   plugins pawn-mocker.so
   ```

   (or `pawn-mocker.dll` on Windows — keep any existing plugins already
   listed there, just append this one.)

4. Copy `client.js` into your test project (or `npm link` it as a local
   package) and install `jest` as a dev dependency:

   ```
   npm install --save-dev jest
   ```

5. Start the server. You should see in the console:

   ```
   [PAWN-MOCKER] listening on 127.0.0.1:7777 (maxRecvBuffer=4194304 bytes)
   ```

That's it — no gamemode-side code changes are required, the plugin hooks in
purely at the AMX/native level.

### Optional config

- Env var `PAWN_MOCKER_MAX_RECV_BUFFER` sets the max receive buffer size
  (bytes) at startup, overriding the 4MB default.
- The same limit can be changed at runtime via the `setconfig` action from
  the client.

## JavaScript client (`client.js`)

```js
const { SampMockClient, P } = require('./client');

const client = new SampMockClient(); // defaults to 127.0.0.1:7777

await client.connect();

// Trigger a public callback in the gamemode
await client.invoke('OnPlayerConnect', [P.int(0)]);

// Read / write a pubvar
const { value } = await client.getVar('gGameState', 'i');
await client.setVar('gGameState', 1, 'i');

// Mock a native so it doesn't hit the real implementation
await client.mockNative('GetPlayerMoney', { returnValue: 5000 });

// Inspect how the mocked native was called
const calls = await client.getNativeCalls('GetPlayerMoney');

// Restore the native
await client.unmockNative('GetPlayerMoney');

client.close();
```

`P.int`, `P.float`, and `P.str` build tagged parameters for `invoke()`, since
the plugin needs to know each argument's type (`i`, `f`, `s`) to push it onto
the AMX stack correctly.

## Example unit test

```js
const { SampMockClient, P } = require('../client');

describe('OnPlayerConnect', () => {
  let client;

  beforeAll(async () => {
    client = new SampMockClient();
    await client.connect();
  });

  afterAll(() => {
    client.close();
  });

  afterEach(async () => {
    await client.resetMocks();
  });

  test('gives a new player starting money', async () => {
    await client.mockNative('GetPlayerMoney', { returnValue: 0 });
    await client.mockNative('GivePlayerMoney');

    await client.invoke('OnPlayerConnect', [P.int(0)]);

    const calls = await client.getNativeCalls('GivePlayerMoney');
    expect(calls).toHaveLength(1);
    expect(calls[0]).toEqual([0, 500]); // playerid, amount
  });

  test('rejects underage nickname format', async () => {
    const result = await client.invoke('OnPlayerConnect', [P.int(1)]);
    expect(result.return).toBe(1);
  });
});
```

Run it with:

```
npx jest
```

## Notes

- The plugin only tracks a single AMX instance at a time (the first
  gamemode loaded); restarting the gamemode clears mocks automatically via
  `AmxUnload`.
- Up to 64 natives can be mocked simultaneously (`MAX_MOCKED_NATIVES`).
- Some log messages from the plugin and error responses returned over the
  socket are in Indonesian.