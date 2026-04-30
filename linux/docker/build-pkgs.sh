#!/bin/bash
# See linux/docker/README.md for usage.
set -e

export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Pre-place the cached linuxdeploy where the Makefile expects it.
ARCH=$(uname -m)
mkdir -p /src/linux/target
cp /usr/local/bin/linuxdeploy-${ARCH}.AppImage /src/linux/target/linuxdeploy-${ARCH}.AppImage

cd /src/linux
make configure
make compile
make deb rpm

if [[ -d /out ]]; then
  cp target/quetoo_*.deb target/quetoo-*.rpm /out/
  echo ""
  echo "Packages written to /out/:"
  ls -lh /out/quetoo_*.deb /out/quetoo-*.rpm
fi
