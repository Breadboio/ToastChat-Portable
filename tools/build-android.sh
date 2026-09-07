#!/bin/sh
# Builds ToastChat.apk with the Android NDK. Unlike the console targets this
# one does NOT run in Docker - the SDK/NDK are installed on the host, since
# there is no devkitPro-style official image to borrow.
#
#   tools/build-android.sh                 # release-shaped debug APK, wss://443
#   TC_TLS=OFF TC_HOST=192.168.1.167 TC_PORT=3401 tools/build-android.sh
#
# Requires: JDK 21 at ~/Android/jdk, SDK at ~/Android/Sdk with ndk;27.2.12479018
# and cmake;3.22.1. Install those with
#   ~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager \
#       "ndk;27.2.12479018" "cmake;3.22.1" "platforms;android-35"
set -e
cd "$(dirname "$0")/.."
ROOT="$PWD"

HOST="${TC_HOST:-breadtoasting.com}"
PORT="${TC_PORT:-443}"
TLS="${TC_TLS:-ON}"
AUTONAME="${TC_AUTONAME:-OFF}"
TASK="${TC_TASK:-assembleDebug}"

export JAVA_HOME="${JAVA_HOME:-$HOME/Android/jdk}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
export PATH="$JAVA_HOME/bin:$PATH"

# mbedtls is not vendored - it is 27MB of upstream source - so fetch it the
# first time a TLS build is asked for. Same 2.28.x series devkitPro ships for
# the 3DS and Switch, so core/tc_tls.c compiles unchanged everywhere.
if [ "$TLS" = "ON" ] && [ ! -f third_party/mbedtls/CMakeLists.txt ]; then
    echo "fetching mbedtls 2.28.8 ..."
    mkdir -p third_party
    curl -4 -sSL -o /tmp/mbedtls-tc.tar.gz \
      https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/mbedtls-2.28.8.tar.gz
    tar xzf /tmp/mbedtls-tc.tar.gz -C third_party
    mv third_party/mbedtls-mbedtls-2.28.8 third_party/mbedtls
    rm -f /tmp/mbedtls-tc.tar.gz
fi

cd platform/android
[ -f local.properties ] || echo "sdk.dir=$ANDROID_HOME" > local.properties

./gradlew "$TASK" \
    -PtcHost="$HOST" -PtcPort="$PORT" -PtcTls="$TLS" -PtcAutoname="$AUTONAME" \
    "$@"

mkdir -p "$ROOT/build"
for apk in app/build/outputs/apk/*/*.apk; do
    [ -f "$apk" ] || continue
    cp "$apk" "$ROOT/build/ToastChat-Android-$(basename "$(dirname "$apk")").apk"
done
echo "built: $(ls -la "$ROOT"/build/ToastChat-Android-*.apk)"
echo "install with: adb install -r $ROOT/build/ToastChat-Android-debug.apk"
