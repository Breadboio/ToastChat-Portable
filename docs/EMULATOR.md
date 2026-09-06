# Running the 3DS build in an emulator

Two ways. Headless is a smoke test you can run on breadweb; interactive is the
one where you can actually draw.

## A. Headless, on breadweb (no display needed)

One-time setup:

    mkdir -p ~/tools && cd ~/tools
    curl -L -o azahar.AppImage \
      https://github.com/azahar-emu/azahar/releases/download/2126.0/azahar.AppImage
    chmod +x azahar.AppImage
    ./azahar.AppImage --appimage-extract          # no FUSE needed
    cd ~/breadtoasting/toastchat-portable
    docker build -t azahar-headless:v2 tools/emulator

Then, any time:

    ./tools/build-3ds.sh            # -> build/3ds/ToastChat.3dsx
    ./tools/run-3ds-emulator.sh     # -> build/emu/screen.png

It boots the .3dsx under Xvfb + Mesa llvmpipe, waits 35s, and screenshots the
X root window. `SECS=60 ./tools/run-3ds-emulator.sh` waits longer.

You cannot touch the screen this way, so it proves boot + render + connect and
nothing else. To confirm it really reached the server, look from the other side
while it is running:

    curl -s http://127.0.0.1:3401/api/rooms/C/log

A working run shows `"3DS entered Room C."` and `"people": 1`.

## B. Interactive, on a desktop machine (this is the one you want)

The headless run cannot draw. To actually use it, run Azahar on a machine with
a display that is on the same LAN as the server:

    scp breadweb:~/breadtoasting/toastchat-portable/build/3ds/ToastChat.3dsx .
    ./azahar.AppImage ToastChat.3dsx

The mouse acts as the stylus on the bottom screen: click and drag to draw, click
a swatch or pen size to change them, and UN / CL / GO are undo / clear / send.

## Pointing it somewhere else

The server address is compiled in, defaulting to 192.168.1.167:3401 (the dev
instance, NOT prod on :3400):

    TC_HOST=192.168.1.50 TC_PORT=3401 ./tools/build-3ds.sh

## What this does and does not prove

Proves: the .3dsx boots, the column-major BGR8 framebuffer blit is right, both
screens lay out correctly, sockets come up, and the RFC 6455 handshake and room
join work.

Does NOT prove anything about real hardware: the emulator uses the **host**
network stack, so 3DS wifi and the TLS problem in docs/PROTOCOL.md are still
completely untested. Nor does it say anything about speed on a real console.
