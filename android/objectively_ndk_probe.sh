#!/bin/bash
# Scope the Objectively NDK port: cross-compile each Objectively source with the
# Android NDK and report which compile clean vs. which block on a missing
# dependency (iconv / libcurl). Run on the build container. Read-only probe.
NDK=${NDK:-/opt/android-ndk-r27c}
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
SRC=${OBJ:-/root/Objectively}
cd "$SRC" || exit 1
ok=0; fail=0
for f in Sources/Objectively/*.c; do
  if "$CC" -std=gnu11 -fPIC -ISources -c "$f" -o /tmp/obj.o 2>/tmp/obj.err; then
    ok=$((ok+1))
  else
    fail=$((fail+1))
    h=$(grep -oE "fatal error: '?[A-Za-z0-9_/.]+'? file not found|fatal error: [A-Za-z0-9_/.]+: No such file" /tmp/obj.err | head -1)
    [ -z "$h" ] && h=$(grep -E "error:" /tmp/obj.err | head -1)
    echo "  BLOCK $(basename "$f")  --  $h"
  fi
done
echo "==== objectively: $ok compiled clean, $fail blocked ===="
