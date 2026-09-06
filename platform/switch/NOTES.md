# Switch port notes

Toolchain is the same devkitPro install as 3DS/Wii/Wii U — one more group:

    sudo dkp-pacman -S switch-dev        # devkitA64 + libnx

Builds a `.nro`, launched from nx-hbmenu. (Verified against switchbrew's
"Setting up Development Environment" wiki, 2026-09-06.)

## Technically this is the easiest target we have

All three hard parts of `platform.h` map 1:1 onto libnx with no adaptation:

| platform.h | libnx |
|---|---|
| `tc_sock_*` | `socketInitializeDefault()` then plain BSD sockets |
| `tc_video_blit` | `framebufferCreate` / `framebufferBegin` — raw RGBA, no GPU API |
| `tc_input_pointer` | `hidGetTouchScreenStates` — a real capacitive touchscreen |

No GPU pipeline to learn (deko3d/EGL exist but we don't need them), no tiled
texture format like the 3DS, and a native touchscreen instead of an IR pointer
or a stick-driven cursor. It is a **bigger** PictoChat canvas than the 3DS: 1280x720
handheld. Genuinely less work than the 3DS port.

Two wrinkles:

- **Docked mode has no touch** (1920x1080, no touchscreen). Needs a
  stick-as-cursor fallback, same code path the Dreamcast port will want.
- **Applet vs title takeover.** Launching hbmenu from the Album applet gives a
  restricted memory pool (~448MB); taking over a game gives the full pool. Our
  footprint is tiny, so applet mode is fine — which is also the low-friction
  path for users.

Single screen, so the port composites ToastChat's top/bottom screens itself.
`platform.h` already allows for that (Dreamcast/Android/desktop do the same).

## The catch is hardware, not code

Whether this is a weekend or a soldering project depends entirely on which unit
you own:

- **Unpatched Erista** (original model, roughly pre-mid-2018): vulnerable to the
  Tegra X1 RCM bootrom exploit. Jig + payload injector, software-only,
  unpatchable by Nintendo. This is the one you want.
- **Patched Erista / Mariko / OLED / Lite**: bootrom fixed. Requires a modchip
  (picofly/hwfly) — fine soldering on a live console, with real brick risk.

Check the serial prefix against a "is my Switch patched" list before planning
anything. Atmosphère is the standard CFW either way.

## Emulation is a worse option here than it was for 3DS

Both yuzu and Ryujinx were shut down by Nintendo in 2024; the surviving forks
are unstable and legally fraught. More practically: Switch emulators generally
need firmware and `prod.keys` dumped from a console you already own, so emulation
**does not remove the hardware requirement** the way Azahar did for 3DS homebrew.
And it would hit the same trap flagged in docs/PROTOCOL.md — an emulator uses the
*host's* network stack, so a green result proves nothing about the console.

Build and iterate on the SDL2 target; use real hardware to confirm.

## Online / ban note

A modded Switch that touches Nintendo's network can get the console banned.
ToastChat only ever talks to your own LAN server, but the usual practice on a
modded unit is to blackhole Nintendo's servers (90DNS or equivalent) if the
console still matters to you for anything official.
