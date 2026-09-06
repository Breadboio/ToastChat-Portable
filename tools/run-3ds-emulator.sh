#!/bin/sh
# Boots build/3ds/ToastChat.3dsx in Azahar, headless, and screenshots it.
#
# One-time setup:
#   mkdir -p ~/tools && cd ~/tools
#   curl -L -o azahar.AppImage \
#     https://github.com/azahar-emu/azahar/releases/download/2126.0/azahar.AppImage
#   chmod +x azahar.AppImage && ./azahar.AppImage --appimage-extract
#   docker build -t azahar-headless:v2 tools/emulator
#
# Caveat: the emulator uses the HOST network stack. This proves the framebuffer,
# the UI, and the protocol code - it proves nothing about real 3DS wifi or TLS.
set -e
cd "$(dirname "$0")/.."
AZ="${AZAHAR_ROOT:-$HOME/tools/squashfs-root}"
OUT="${OUT_DIR:-$PWD/build/emu}"
SECS="${SECS:-35}"
mkdir -p "$OUT"
docker run --rm --network host \
  -v "$AZ":/azahar:ro -v "$PWD/build/3ds":/rom:ro -v "$OUT":/out \
  azahar-headless:v2 bash -c "
export LD_LIBRARY_PATH=/azahar/usr/lib QT_PLUGIN_PATH=/azahar/usr/plugins HOME=/tmp/azhome
mkdir -p \$HOME
Xvfb :99 -screen 0 1400x1000x24 >/dev/null 2>&1 &
sleep 3
timeout $((SECS + 30)) /azahar/usr/bin/azahar /rom/ToastChat.3dsx > /out/azahar.log 2>&1 &
AZ=\$!
sleep $SECS
import -display :99 -window root /out/screen.png 2>/dev/null && echo 'screenshot -> build/emu/screen.png'
kill \$AZ 2>/dev/null || true
tail -5 /out/azahar.log
"
