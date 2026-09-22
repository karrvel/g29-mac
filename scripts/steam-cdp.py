#!/usr/bin/env python3
"""Talk to Steam's CEF over the DevTools protocol, using nothing but the stdlib.

Steam's window renders black under Wine (see
_knowledge/gotchas/steam-webhelper-restart-loop-in-wine.md), so the client has to
be driven programmatically instead of clicked. Steam opens a DevTools endpoint on
port 8080 when a marker file exists next to it:

    touch "$STEAM_DIR/.cef-enable-remote-debugging"      # then restart Steam

Usage:
    steam-cdp.py <target-title> '<json-command>' [outfile-for-base64-data]

`target-title` is matched against the titles in http://localhost:8080/json/list —
"SharedJSContext" is where the SteamClient JS API lives, "Steam" is the main
window, "Steam Dialog" is any modal. Examples:

    steam-cdp.py SharedJSContext '{"id":1,"method":"Runtime.evaluate",
        "params":{"expression":"1+1","returnByValue":true}}'
    steam-cdp.py "Steam Dialog" '{"id":1,"method":"Page.captureScreenshot",
        "params":{"format":"png"}}' /tmp/dialog.png

Exits non-zero if no matching target exists, so callers can test for a dialog.
"""
import base64, json, os, socket, struct, sys, urllib.parse, urllib.request

CDP_HOST = os.environ.get("STEAM_CDP_HOST", "localhost")
CDP_PORT = int(os.environ.get("STEAM_CDP_PORT", "8080"))


def find_target(title):
    """Return the websocket URL of the first target whose title matches."""
    url = f"http://{CDP_HOST}:{CDP_PORT}/json/list"
    try:
        with urllib.request.urlopen(url, timeout=8) as r:
            targets = json.load(r)
    except Exception:
        return None
    for t in targets:
        if t.get("title") == title:
            return t.get("webSocketDebuggerUrl")
    return None


def connect(url):
    """Minimal RFC 6455 client handshake — enough for one request/response."""
    u = urllib.parse.urlparse(url)
    s = socket.create_connection((u.hostname, u.port or 80), timeout=30)
    key = base64.b64encode(os.urandom(16)).decode()
    path = u.path + (("?" + u.query) if u.query else "")
    s.sendall(
        f"GET {path} HTTP/1.1\r\nHost: {u.hostname}:{u.port}\r\n"
        f"Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n".encode()
    )
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = s.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed during handshake")
        buf += chunk
    head, rest = buf.split(b"\r\n\r\n", 1)
    if b"101" not in head.split(b"\r\n")[0]:
        raise RuntimeError("handshake rejected: " + head.decode(errors="replace")[:200])
    return s, rest


def send(sock, payload):
    data = payload.encode()
    mask = os.urandom(4)
    n = len(data)
    frame = bytearray([0x81])
    if n < 126:
        frame.append(0x80 | n)
    elif n < 65536:
        frame += bytes([0x80 | 126]) + struct.pack(">H", n)
    else:
        frame += bytes([0x80 | 127]) + struct.pack(">Q", n)
    frame += mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    sock.sendall(bytes(frame))


def recv(sock, buf):
    """Read one (possibly fragmented) text frame. Screenshots are large."""
    def need(n):
        nonlocal buf
        while len(buf) < n:
            chunk = sock.recv(65536)
            if not chunk:
                raise RuntimeError("connection closed")
            buf += chunk

    out = b""
    while True:
        need(2)
        fin, ln, off = buf[0] & 0x80, buf[1] & 0x7F, 2
        if ln == 126:
            need(4); ln = struct.unpack(">H", buf[2:4])[0]; off = 4
        elif ln == 127:
            need(10); ln = struct.unpack(">Q", buf[2:10])[0]; off = 10
        need(off + ln)
        out += buf[off:off + ln]
        buf = buf[off + ln:]
        if fin:
            return out, buf


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    title, command = sys.argv[1], sys.argv[2]
    outfile = sys.argv[3] if len(sys.argv) > 3 else None

    ws_url = find_target(title)
    if not ws_url:
        print(f"no CDP target titled {title!r} (is Steam running with remote debugging?)",
              file=sys.stderr)
        sys.exit(2)

    sock, leftover = connect(ws_url)
    send(sock, command)
    want = json.loads(command)["id"]
    while True:
        msg, leftover = recv(sock, leftover)
        reply = json.loads(msg)
        if reply.get("id") == want:
            break

    result = reply.get("result", reply)
    if outfile and "data" in result:
        with open(outfile, "wb") as f:
            f.write(base64.b64decode(result["data"]))
        print(f"wrote {outfile} ({os.path.getsize(outfile)} bytes)")
    else:
        print(json.dumps(result))


if __name__ == "__main__":
    main()
