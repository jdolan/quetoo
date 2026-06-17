#!/bin/bash
# Cross-compile the qglib shim with the Android NDK (Bionic/clang, arm64-v8a) to
# prove it is NDK-clean on the real target libc. Compiles each source to a .o and
# archives a static lib. Cannot RUN arm64 binaries here (no emulator) — a clean
# compile + archive is the validation. Run on the build container.
#
#   NDK=/opt/android-ndk-r27c ABI_API=24 ./ndk_check.sh
set -e
NDK=${NDK:-$(ls -d /opt/android-ndk-* 2>/dev/null | head -1)}
API=${API:-24}
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then echo "ERROR: NDK not found (set NDK=...)"; exit 1; fi
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang"
if [ ! -x "$CC" ]; then echo "ERROR: clang not found at $CC"; exit 1; fi
cd "$(dirname "$0")"
CFLAGS="-std=gnu11 -Wall -Wextra -O2 -fPIC"

echo "NDK: $NDK"
"$CC" --version | head -1
rm -f *.o libqglib.a
fail=0
for s in qglib_*.c; do
  if "$CC" $CFLAGS -c "$s" -o "${s%.c}.o" 2>/tmp/ndkerr; then
    echo "  ok   $s"
  else
    echo "  FAIL $s"; head -10 /tmp/ndkerr; fail=1
  fi
done
if [ $fail -eq 0 ]; then
  ar rcs libqglib.a qglib_*.o
  echo "=== libqglib.a: $(stat -c%s libqglib.a) bytes, $(ar t libqglib.a | wc -l) objects (arm64-v8a, Bionic) ==="
  echo "ALL QGLIB NDK-CLEAN"
else
  echo "NDK BUILD HAD FAILURES"; exit 1
fi
