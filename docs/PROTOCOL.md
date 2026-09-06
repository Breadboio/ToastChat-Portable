# ToastChat wire protocol — as verified against a live server

Everything here was measured against a real ToastChat server (the `toastchat-app`
image, dev instance on :3401) on 2026-09-06, not read off the source. Where the
source and the observed behaviour disagreed, the observation wins.

## 0. Transport: WebSocket, not HTTP polling

An HTTP-polling client **cannot work**. `server.js` sweeps every 30s and wipes
any room where `clients.size === 0`, and only WebSocket connections are ever
added to `clients`. Measured: a drawing POSTed to an empty room was gone within
10 seconds, and stayed gone.

    t=0s   posted. log length = 1
    t=10s  log length = 0  people=0
    t=45s  log length = 0  people=0

Holding a WebSocket open fixes it — same room, socket held, log survived 40s
with `people=1`. So the socket is not an optimisation, it is what keeps the room
alive. Everything below assumes a held socket.

## 1. Handshake

Standard RFC 6455. The only crypto the client needs is SHA-1 + base64:

    accept = base64( SHA1( client_key + GUID ) )
    GUID   = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

**Check the GUID character by character.** The commonly mistyped form moves the
`C` from the front of the last group to the end (`...95CA-5AB0DC85B11C`), which
produces a plausible-looking accept that never matches. Verified both against
the RFC 6455 §1.3 vector (`dGhlIHNhbXBsZSBub25jZQ==` →
`s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`) and against a live handshake.

Request that works, byte for byte:

    GET /ws HTTP/1.1\r\n
    Host: 192.168.1.167:3401\r\n
    Upgrade: websocket\r\n
    Connection: Upgrade\r\n
    Sec-WebSocket-Key: <base64 of 16 random bytes>\r\n
    Sec-WebSocket-Version: 13\r\n
    \r\n

Server answers `101 Switching Protocols` with `Sec-WebSocket-Accept`.

Path note: on prod, nginx proxies `/toastchat/` with a **trailing slash**, so it
strips the prefix — the browser asks for `/toastchat/ws` and the server sees
`/ws`. Direct to the container (LAN :3400 / dev :3401) the path is plain `/ws`.

## 2. Framing

- Client→server frames **must be masked** (4-byte key, XOR over payload).
- Server→client frames are **never masked**.
- Only opcode 0x1 (text) carries messages; 0x8 is close, 0x9/0xA ping/pong.
- Length: <126 inline, 126 → next 2 bytes BE, 127 → next 8 bytes BE.

The server pings every 30s and drops sockets that miss a pong
(`heartbeat` in server.js). A port must answer pings or it gets disconnected.

## 3. Messages the client sends

    {"t":"identify","nick":"3DS","color":"#2fa89a"}
    {"t":"join","room":"C"}
    {"t":"msg","png":"data:image/png;base64,...","w":320,"h":240}
    {"t":"leave"}
    {"t":"watch","room":"C"}     // read-only, invisible; see the security note

Constraints measured/read: nick is trimmed to 10 chars and stripped of control
characters; `color` must be one of the 16 in `DS_COLORS` or the server silently
assigns a random one; sends are rate-limited to one per 900ms **per socket**.

`ws` `maxPayload` is `MAX_MESSAGE_BYTES + 64KB` ≈ 464KB. A frame above that is
not rejected politely — the socket is closed. Keep a send under ~400KB.

## 4. Messages the client receives

    {"t":"hello","id":...,"rooms":{...},"max":16,"colors":[16 hex],"ttlMinutes":20}
    {"t":"identified","nick":...,"color":...}
    {"t":"joined","room":"C","log":[entry...],"users":[...],"rooms":{...}}
    {"t":"entry","entry":{...}}
    {"t":"roster","room":"C","users":[{id,nick,color}...]}
    {"t":"counts","rooms":{"A":0,...}}
    {"t":"prune","ids":[...]}    // TTL swept these
    {"t":"wiped"}                // whole room cleared
    {"t":"error","text":"..."}

An `entry` of kind `msg` carries the PNG **inline as a base64 data URL** in
`png` (and `frames[]` for animations). So the socket path needs base64 *decode*.
A full 40-entry log arrives in one `joined` message — that is the largest
allocation a port will face. Budget for it, or join and immediately trim.

## 4b. TLS, and reaching the public server

`wss://breadtoasting.com/toastchat/ws` works from a console client, verified.
The trick is to **bring your own TLS**: the client links mbedtls (a devkitPro
portlib for 3DS, Switch and Wii U - there is none for Wii) and embeds its own
trust anchors in `core/tc_cacert.h`. That makes the earlier blocker moot - the
console's 2011-vintage root store and its limited cipher support never come into
it, because we never ask the console to do TLS. Regenerate the anchors with
`tools/mkca.py <host>` if the server changes CA.

**Header casing bites here.** Node's `ws` replies `Sec-WebSocket-Accept`, but
nginx forwards it as `Sec-Websocket-Accept`. A case-sensitive match works
perfectly against the dev container and fails against prod. HTTP header names
are case-insensitive; compare them that way.

Path also differs: straight to the container it is `/ws`; through unified-nginx
the browser and the client must ask for `/toastchat/ws`.

## 5. The HTTP side (useful, but not sufficient)

    GET /healthz                 -> {"ok":true,"rooms":{...}}
    GET /api/rooms               -> counts + max
    GET /api/rooms/:room/log     -> metadata + image URLs, NO inline base64
    GET /api/messages/:id.png    -> a real binary PNG, Content-Length set

Two things a tiny C client should know:

- **Never send `Accept-Encoding`.** Measured: without it, responses are plain
  with `Content-Length` and no chunking (~80 lines of C to parse). With
  `Accept-Encoding: gzip` the same response comes back **gzipped AND chunked
  with no Content-Length** — that would mean shipping an inflate on a Dreamcast.
- The `image` field is built from the `Host` header and the (empty) BASE_PATH,
  so it comes back as `http://<whatever-you-sent>/api/messages/<id>.png` — wrong
  scheme, wrong prefix. **Ignore it and build the path from `id` yourself.**

`POST /api/rooms/:room/messages` requires `INGEST_TOKEN`, which is empty on prod
(returns 503) and cannot be baked into a public homebrew binary anyway. It is
for bots, not for ports.

## 6. Palette

The 16 colours are the DS system "favourite colour" set, sent in `hello.colors`.
Read them from the wire rather than hardcoding, so a port never drifts:

    #8c8f94 #7a4c2a #c8352b #e77aa6 #e0762a #e0b52a #a8cc37 #4faa3c
    #2f7d34 #2fa89a #3fbbd6 #2f6fd0 #2a3f9e #7a49c4 #b04ec4 #d6459b

## 7. Security notes that affect ports

- There is **no origin check and no auth** on the socket. Anything can connect.
- `watch` gives a silent read-only feed: full log, all future drawings, and the
  watcher is not in the roster or the head count. Do not build a port that
  relies on this staying open — it is the first thing that should be gated.
- Nothing is encrypted on the LAN path. Fine here (the server holds no
  credentials and the log is world-readable anyway), but do not carry this
  assumption to a public deployment without revisiting it.
