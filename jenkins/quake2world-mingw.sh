#!/bin/bash
set -e

source ./_common.sh


init_chroot

MINGW_ARCH=`echo $JOB_NAME|cut -d\-  -f3-10`

if [ "${MINGW_ARCH}" == "mingw64" ]
then
	MINGW_TARGET="x86_64"
elif [ "${MINGW_ARCH}" == "mingw32" ]
then
	MINGW_TARGET="i686"
fi



/usr/bin/mock -r ${CHROOT} --shell "
export PATH=/usr/${MINGW_TARGET}-w64-mingw32/sys-root/mingw/bin:${PATH}
cd /tmp/quake2world
autoreconf -i --force
./configure --host=${MINGW_TARGET}-w64-mingw32 --prefix=/tmp/quake2world-${MINGW_ARCH}
make
make install
"

archive_workspace 
destroy_chroot
