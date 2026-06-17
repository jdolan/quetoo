#!/bin/bash
# Engine compile-probe: syntax-check engine TUs against qglib (no real glib on
# the include path). Reports any glib symbols qglib is still missing. Dev-only.
QG=/root/quetoo-android/android/qglib
SRC=/root/quetoo-build/src
INC="-I/root/qg-probe/stub -I/root/qg-probe/compat -I$QG -I$SRC -I$SRC/shared -I$SRC/common -I$SRC/collision -I$SRC/game -I$SRC/game/default"
CF=$(pkg-config --cflags sdl3 Objectively 2>/dev/null)
for f in "$@"; do
  echo "=== $f ==="
  gcc -fsyntax-only -std=gnu11 $INC $CF "$SRC/$f" 2>/tmp/qgerr
  ec=$?
  grep -iE "error:|implicit declaration" /tmp/qgerr | grep -iE "g_[a-z]|G[A-Z][a-z]" | head -12
  echo "gcc_exit=$ec"
done
