#!/bin/bash
set -e
source ./_common.sh

TARGET="i686"
if [ ${ENV} == "mingw64" ]
then
	TARGET="x86_64"
fi

MINGW_ENV="fedora-18-x86_64"

setup_mingw

/usr/bin/mock -r ${MINGW_ENV} --shell "
export PATH=/usr/${TARGET}-w64-mingw32/sys-root/mingw/bin:${PATH}
cd /tmp/quake2world
libtoolize --force
autoreconf -i --force
./configure --host=${TARGET}-w64-mingw32 --prefix=/tmp/quake2world-${MINGW_ARCH}

make
make install

"

archive_workspace_mingw

destroy_mingw
