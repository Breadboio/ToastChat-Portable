#!/bin/sh
# Builds ToastChat.nro with devkitA64 in Docker - no host toolchain needed.
set -e
cd "$(dirname "$0")/.."
HOST="${TC_HOST:-192.168.1.167}"
PORT="${TC_PORT:-3401}"
docker run --rm -v "$PWD":/src -w /src -e HOST="$HOST" -e PORT="$PORT" \
  devkitpro/devkita64:latest sh -c '
set -e
DKP=/opt/devkitpro
CC=$DKP/devkitA64/bin/aarch64-none-elf-gcc
ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
CFLAGS="-std=gnu99 -Wall -Wextra -O2 $ARCH -D__SWITCH__ -DTC_HOST=\"$HOST\" -DTC_PORT=$PORT -I$DKP/libnx/include"
mkdir -p build
for f in core/tc_sha1.c core/tc_base64.c core/tc_ws.c platform/switch/plat_switch.c platform/switch/main.c; do
  $CC $CFLAGS -c "$f" -o "build/$(basename $f .c).o"
done
$CC -specs=$DKP/libnx/switch.specs $ARCH build/*.o -L$DKP/libnx/lib -lnx -o build/ToastChat.elf
$DKP/tools/bin/elf2nro build/ToastChat.elf build/ToastChat.nro
echo "built: $(ls -la build/ToastChat.nro)"
'
