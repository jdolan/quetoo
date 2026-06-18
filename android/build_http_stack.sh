#!/bin/bash
# #856 path B: cross-build the HTTPS stack the v78 engine needs through
# Objectively's RESTClient -> URLSession -> libcurl -> mbedTLS (HTTPS endpoints:
# giblets.quetoo.org, *.s3.amazonaws.com). Produces, for one ABI, into PREFIX:
#   libmbedtls/libmbedx509/libmbedcrypto.so, libcurl.so, and a libObjectively.so
#   that now INCLUDES the URLSession + RESTClient + JSON layer (libcurl-linked).
#
# Usage: build_http_stack.sh <abi> <prefix>   e.g. build_http_stack.sh x86_64 /root/android-x86_64
#   ABIs: arm64-v8a | x86_64
set -e
ABI="${1:-x86_64}"
PREFIX="${2:-/root/android-$ABI}"
NDK=/opt/android-ndk-r27c
API=24
TC="$NDK/build/cmake/android.toolchain.cmake"
case "$ABI" in
  arm64-v8a) TRIPLE=aarch64-linux-android ;;
  x86_64)    TRIPLE=x86_64-linux-android ;;
  *) echo "unknown ABI $ABI"; exit 1 ;;
esac
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/${TRIPLE}${API}-clang"
COMPAT=/root/obj-compat
OBJ=/root/Objectively
MBEDTLS_VER=3.6.2
CURL_VER=8.11.1
# -Wl,-z,max-page-size=16384: 16 KB ELF segment alignment for arm64 Android (#856).
CM="-DCMAKE_TOOLCHAIN_FILE=$TC -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384"
mkdir -p "$PREFIX" /root/httpsrc
cd /root/httpsrc

# --- 1. mbedTLS (TLS backend for libcurl) -------------------------------------
echo "=== mbedTLS $MBEDTLS_VER ($ABI) ==="
if [ ! -d "mbedtls-$MBEDTLS_VER" ]; then
  curl -fsSL -o "mbedtls-$MBEDTLS_VER.tar.bz2" \
    "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VER/mbedtls-$MBEDTLS_VER.tar.bz2"
  tar xjf "mbedtls-$MBEDTLS_VER.tar.bz2"
fi
cd "mbedtls-$MBEDTLS_VER"; rm -rf "ba-$ABI"; mkdir "ba-$ABI"; cd "ba-$ABI"
if cmake $CM -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF \
     -DUSE_SHARED_MBEDTLS_LIBRARY=ON -DUSE_STATIC_MBEDTLS_LIBRARY=OFF .. >cm.log 2>&1 \
   && cmake --build . -j6 >b.log 2>&1 && cmake --install . >/dev/null 2>&1; then
  echo "mbedTLS OK"
else
  echo "mbedTLS FAIL"; tail -25 cm.log b.log 2>/dev/null; exit 1
fi
cd /root/httpsrc

# --- 2. libcurl (HTTP/HTTPS only, mbedTLS backend) ----------------------------
echo "=== libcurl $CURL_VER ($ABI) ==="
if [ ! -d "curl-$CURL_VER" ]; then
  curl -fsSL -o "curl-$CURL_VER.tar.gz" "https://curl.se/download/curl-$CURL_VER.tar.gz"
  tar xzf "curl-$CURL_VER.tar.gz"
fi
cd "curl-$CURL_VER"; rm -rf "ba-$ABI"; mkdir "ba-$ABI"; cd "ba-$ABI"
# HTTP_ONLY keeps http:// and https:// (TLS is a backend, not a protocol toggle),
# drops ftp/smtp/etc. CA path resolved at runtime (app ships cacert.pem).
if cmake $CM -DBUILD_CURL_EXE=OFF -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF \
     -DCURL_USE_MBEDTLS=ON -DCURL_USE_OPENSSL=OFF -DCURL_USE_LIBPSL=OFF \
     -DUSE_LIBIDN2=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF \
     -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF -DHTTP_ONLY=ON \
     -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none -DCURL_CA_FALLBACK=ON .. >cm.log 2>&1 \
   && cmake --build . -j6 >b.log 2>&1 && cmake --install . >/dev/null 2>&1; then
  echo "libcurl OK"
else
  echo "libcurl FAIL"; tail -30 cm.log b.log 2>/dev/null; exit 1
fi
cd /root/httpsrc

# --- 3. libObjectively.so WITH the HTTP layer ---------------------------------
# Compile EVERY Objectively source (no longer skipping URLSession*.c) plus the
# compat shims, then link a shared lib against libcurl. The RESTClient ->
# URLSession -> libcurl chain is now fully present.
echo "=== Objectively+HTTP ($ABI) ==="
CURL_INC="$PREFIX/include"
CFLAGS="-std=gnu11 -fPIC -O2 -I$OBJ/Sources -I$COMPAT -I$CURL_INC -include $COMPAT/android_compat.h"
B=/root/obj-http-$ABI; rm -rf "$B"; mkdir -p "$B"; cd "$B"
objs=""
"$CC" $CFLAGS -c "$COMPAT/android_compat.c" -o android_compat.o; objs="$objs android_compat.o"
"$CC" $CFLAGS -c "$COMPAT/iconv.c" -o iconv_compat.o;            objs="$objs iconv_compat.o"
ok=0; fail=0
for f in "$OBJ"/Sources/Objectively/*.c; do
  b=$(basename "$f")
  if "$CC" $CFLAGS -c "$f" -o "${b%.c}.o" 2>/tmp/oe; then
    objs="$objs ${b%.c}.o"; ok=$((ok+1))
  else
    fail=$((fail+1)); echo "  FAIL $b: $(grep -m1 -E 'error:|file not found' /tmp/oe | sed -E 's#.*error: ##')"
  fi
done
echo "  objectively sources: ok=$ok fail=$fail"
[ $fail -eq 0 ] || { echo "Objectively compile FAILED"; exit 1; }
# Static archive (link-time convenience) + the shared lib the engine/APK ship.
ar rcs "$PREFIX/lib/libObjectively.a" $objs
"$CC" -shared -Wl,-z,max-page-size=16384 -o "$PREFIX/lib/libObjectively.so" $objs \
  -L"$PREFIX/lib" -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -llog
cp -rf "$OBJ"/Sources/Objectively "$PREFIX/include/"
echo "Objectively.so: $(stat -c%s "$PREFIX/lib/libObjectively.so") bytes"

echo "==== HTTPS stack ($ABI) staged in $PREFIX ===="
ls "$PREFIX"/lib/lib{curl,mbedtls,mbedx509,mbedcrypto,Objectively}.so 2>/dev/null | xargs -n1 basename
