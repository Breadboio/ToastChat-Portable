#!/bin/sh
# Builds and runs the core self-test inside gcc:13 - no host toolchain needed.
set -e
cd "$(dirname "$0")/.."
docker run --rm --network host -v "$PWD":/src -w /src gcc:13 sh -c '
  gcc -std=c99 -Wall -Wextra -Wpedantic -O1 -g \
      core/tc_sha1.c core/tc_base64.c core/tc_ws.c \
      platform/posix/plat_posix.c tools/test_core.c \
      -o /tmp/test_core && exec /tmp/test_core "$@"' -- "$@"
