#!/bin/sh
# Builds boot.dol for the Homebrew Channel using devkitPPC in Docker.
set -e
cd "$(dirname "$0")/.."
HOST="${TC_HOST:-192.168.1.167}"
PORT="${TC_PORT:-3401}"
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src -w /src -e HOST="$HOST" -e PORT="$PORT" \
  devkitpro/devkitppc:latest sh -c '
set -e
DKP=/opt/devkitpro
CC=$DKP/devkitPPC/bin/powerpc-eabi-gcc
MACH="-DGEKKO -mrvl -mcpu=750 -meabi -mhard-float"
CFLAGS="-std=gnu99 -Wall -Wextra -O2 $MACH -DTC_HOST=\"$HOST\" -DTC_PORT=$PORT -I$DKP/libogc/include"
mkdir -p build/wii
for f in core/tc_sha1.c core/tc_base64.c core/tc_ws.c core/tc_draw.c core/tc_ui.c \
         core/tc_png.c core/tc_send.c core/tc_app.c platform/wii/plat_wii.c platform/wii/main.c; do
  $CC $CFLAGS -c "$f" -o "build/wii/$(basename $f .c).o"
done
$CC $MACH build/wii/*.o -L$DKP/libogc/lib/wii -lwiiuse -lbte -logc -lm -o build/wii/toastchat.elf
$DKP/tools/bin/elf2dol build/wii/toastchat.elf build/wii/boot.dol
echo "built: $(ls -la build/wii/boot.dol)"
'
