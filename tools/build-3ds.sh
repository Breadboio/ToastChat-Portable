#!/bin/sh
# Builds ToastChat.3dsx with devkitARM in Docker.
set -e
cd "$(dirname "$0")/.."
HOST="${TC_HOST:-192.168.1.167}"
PORT="${TC_PORT:-3401}"
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src -w /src -e HOST="$HOST" -e PORT="$PORT" \
  devkitpro/devkitarm:latest sh -c '
set -e
DKP=/opt/devkitpro
CC=$DKP/devkitARM/bin/arm-none-eabi-gcc
ARCH="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"
CFLAGS="-std=gnu99 -Wall -Wextra -O2 $ARCH -D__3DS__ -DARM11 -D_3DS -DTC_HOST=\"$HOST\" -DTC_PORT=$PORT -I$DKP/libctru/include"
mkdir -p build/3ds
for f in core/tc_sha1.c core/tc_base64.c core/tc_ws.c core/tc_draw.c core/tc_ui.c \
         core/tc_png.c core/tc_send.c core/tc_app.c platform/3ds/plat_3ds.c platform/3ds/main.c; do
  $CC $CFLAGS -c "$f" -o "build/3ds/$(basename $f .c).o"
done
$CC -specs=$DKP/devkitARM/arm-none-eabi/lib/3dsx.specs $ARCH build/3ds/*.o \
    -L$DKP/libctru/lib -lctru -lm -o build/3ds/ToastChat.elf
$DKP/tools/bin/3dsxtool build/3ds/ToastChat.elf build/3ds/ToastChat.3dsx
echo "built: $(ls -la build/3ds/ToastChat.3dsx)"
'
