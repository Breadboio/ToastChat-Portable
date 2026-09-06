# ToastChat Portable

A second, independent build of ToastChat: a native client in portable C99 that
talks to the **existing, unmodified** ToastChat server. Targets 3DS first, then
Wii U, Wii, Dreamcast and Android from the same core.

This tree does not touch `~/breadtoasting/toastchat` (prod). It speaks the
server's real wire protocol, documented and verified in `docs/PROTOCOL.md`.

## Status — 2026-09-06

Switch is target #1 (user has an unpatched Erista — software-only RCM entry).

| Piece | State |
|---|---|
| Wire protocol | **verified against a live server** (docs/PROTOCOL.md) |
| SHA-1 / base64 / RFC 6455 client | compiled, unit-tested (FIPS + RFC vectors), live handshake |
| Stroke model + AA rasteriser | **rendered to PNG and visually reviewed** (build/draw_test.png) |
| 8x16 bitmap font | generated from Lat15-VGA16 by `tools/mkfont.py` |
| UI compositor | **rendered at 1280x720 and visually reviewed** (build/ui_switch.png) |
| PNG encode + base64 + send | **end-to-end round trip** against a live server; wire bytes reviewed (build/wire.png) |
| Switch framebuffer + touchscreen | written against libnx (`framebufferCreate`, `hidGetTouchScreenStates`) |
| Switch `.nro` | **builds** — 249KB valid NRO0, zero warnings |
| JSON parsing (jsmn) | not wired — status is substring-matched for now |
| Receiving/decoding others' drawings | not written (thumbnails render a placeholder) |
| **Run on real hardware** | **not done — the next milestone** |

No host toolchain and no sudo needed: both builds run in Docker via
`devkitpro/devkita64` and `gcc:13`.

    tools/build-posix.sh 127.0.0.1 3401 C   # compile + unit + live test
    tools/build-switch.sh                   # -> build/ToastChat.nro

The `.nro` **has never been run.** It compiles and the identical core passes
live protocol tests on x86, which is not the same thing — see
[[feedback_build_success_is_not_feature_verification]]. Copy it to
`/switch/` on the SD card and launch from hbmenu to find out.

## Why WebSocket and not HTTP polling

Because polling cannot work. The server wipes any room with zero **WebSocket**
clients every 30 seconds, and HTTP pollers never count as present. Measured, not
assumed — `docs/PROTOCOL.md` §0 has the numbers. Holding a socket is what keeps
a room alive, so the socket is mandatory, not an optimisation.

The upside: a WebSocket client needs **no server change at all**. Prod works as-is.

## Porting contract

A new console is done when `platform/platform.h` compiles and behaves — five
groups: sockets, video blit, one pointer, time, allocation. Everything else
(handshake, framing, JSON, PNG, drawing, state machine) is shared C99 in `core/`,
so a port cannot get the protocol subtly wrong.

    platform/sdl2/       desktop dev target — build and iterate here first
    platform/3ds/        devkitARM + libctru + citro2d
    platform/switch/     devkitA64 + libnx (touchscreen; easiest target - see its NOTES.md)
    platform/wiiu/       devkitPPC + wut
    platform/wii/        devkitPPC + libogc (IR pointer)
    platform/dreamcast/  KallistiOS, SH-4 (mouse or stick cursor)
    platform/android/    NDK

Build the SDL2 target first, always. It is the only one that can be iterated on
without hardware, and a bug found there is a bug not shipped to five consoles.

## Constraints the core must respect

- **C99**, no threads, no C11, no exceptions. KallistiOS and libogc are the floor.
- **Dreamcast sets the memory budget**: 16MB main RAM. The largest single
  allocation is a 40-entry `joined` log of inline base64 PNGs; `TC_LOG_KEEP`
  exists to cap what a port actually decodes.
- **No TLS.** The LAN build is plain `ws://`. Defensible here (the server holds
  no credentials and its log is world-readable), but revisit before any port
  points at the public internet — see docs/PROTOCOL.md §7 and the TLS finding:
  breadtoasting.com serves an **ECDSA-only** cert chaining to GTS Root R4, which
  no 3DS-era console can validate, and RSA-only clients fail the handshake outright.

## Setting up the toolchain (needs sudo — run these yourself)

Prefix with `!` in the Claude Code prompt so the output lands in the session.

    # build essentials + headless GL for the SDL2 target
    sudo apt update
    sudo apt install -y build-essential pkg-config libsdl2-dev xvfb \
                        libgl1-mesa-dri mesa-utils

    # devkitPro (3DS, Wii U, Wii all come from here)
    wget https://apt.devkitpro.org/install-devkitpro-pacman
    chmod +x install-devkitpro-pacman
    sudo ./install-devkitpro-pacman
    sudo dkp-pacman -Syu
    sudo dkp-pacman -S --noconfirm 3ds-dev wii-dev wiiu-dev switch-dev

    # Azahar (3DS emulator) - AppImage, no sudo needed
    mkdir -p ~/tools && cd ~/tools
    # grab the latest linux AppImage from https://github.com/azahar-emu/azahar/releases
    chmod +x Azahar-*.AppImage

Dreamcast (KallistiOS) is a separate toolchain build and can wait until the
core is proven on 3DS.

## Dev server

Prod is `:3400` and must not be used as a test target. An isolated dev instance
runs on **:3401** from the same image, with ingest enabled:

    docker run -d --name toastchat-dev -p 3401:3000 \
      -e INGEST_TOKEN=devtoken -e MESSAGE_TTL_MIN=600 \
      --memory 192m toastchat-app:latest

Point ports at `192.168.1.167:3401`.
