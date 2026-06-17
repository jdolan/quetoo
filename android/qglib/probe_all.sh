#!/bin/bash
# Broad engine compile-probe: syntax-check EVERY .c in the heavy-glib subsystems
# against qglib (compat <glib.h>, no real glib on the path). Reports only files
# that fail, with the missing glib symbol(s). Dev-only validation harness.
QG=/root/quetoo-android/android/qglib
SRC=/root/quetoo-build/src
INC="-I/root/qg-probe/stub -I/root/qg-probe/compat -I$QG -I$SRC -I$SRC/shared -I$SRC/common -I$SRC/collision -I$SRC/net -I$SRC/server -I$SRC/game -I$SRC/game/default"
CF=$(pkg-config --cflags sdl3 Objectively 2>/dev/null)
pass=0; fail=0; fails=""
for f in $(cd "$SRC" && ls common/*.c collision/*.c net/*.c server/*.c game/default/*.c shared/*.c 2>/dev/null); do
  if gcc -fsyntax-only -std=gnu11 $INC $CF "$SRC/$f" 2>/tmp/qge; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); fails="$fails $f"
    echo "--- FAIL: $f ---"
    grep -iE "error:|implicit declaration of function .g_|unknown type name .G" /tmp/qge | grep -iE "g_[a-z]|G[A-Z][a-z]" | head -5
  fi
done
echo "==== pass=$pass fail=$fail ===="
[ -n "$fails" ] && echo "failed:$fails"
exit 0
