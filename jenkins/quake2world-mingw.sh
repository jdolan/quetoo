#!/bin/bash
set -e
source ./_common.sh

if [ ${CHROOT} == "mingw64" ]
then
	MINGW_TARGET="x86_64"
	MINGW_ARCH="mingw64"
else if [ ${CHROOT} == "mingw32" ]
	MINGW_TARGET="i686"
	MINGW_ARCH="mingw32"
fi

CHROOT="fedora-18-x86_64"

init_chroot
install_deps

/usr/bin/mock -r ${CHROOT} --shell "
export PATH=/usr/${MINGW_TARGET}-w64-mingw32/sys-root/mingw/bin:${PATH}
cd ~/quake2world
libtoolize --force
autoreconf -i --force
./configure --host=${MINGW_TARGET}-w64-mingw32 --prefix=/tmp/quake2world-${MINGW_ARCH}
make
make install
"

archive_workspace 
destroy_chroot
