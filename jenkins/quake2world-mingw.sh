#!/bin/bash
set -e

source ./_common.sh


init_chroot

MINGW_TARGET=`echo $JOB_NAME|cut -d\-  -f3-10`

if [ "${MINGW_TARGET}" == "mingw64" ]
then
	MINGW_ARCH="x86_64"
elif [ "${MINGW_TARGET}" == "mingw32" ]
then
	MINGW_ARCH="i686"
fi



/usr/bin/mock -r ${CHROOT} --shell "
export PATH=/usr/${MINGW_ARCH}-w64-mingw32/sys-root/mingw/bin:${PATH}
cd /tmp/quake2world
autoreconf -i --force
./configure --host=${MINGW_ARCH}-w64-mingw32 --prefix=/tmp/quake2world-${MINGW_TARGET}
make
make install
"

archive_workspace 
destroy_chroot
