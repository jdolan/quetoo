#!/bin/bash
set -e

source ./common.sh

init_chroot

export MINGW_TARGET=`echo $JOB_NAME|cut -d\- -f3-10`

if [ "${MINGW_TARGET}" == "mingw64" ]; then
	export MINGW_ARCH="x86_64"
elif [ "${MINGW_TARGET}" == "mingw32" ]
then
	export MINGW_ARCH="i686"
fi

# Copy the SSH config into the chroot in case this is a release build
/usr/bin/mock -r ${CHROOT} --copyin ~/.ssh "/root/.ssh"

# Now do the build
/usr/bin/mock -r ${CHROOT} --shell "
	export PATH=/usr/${MINGW_ARCH}-w64-mingw32/sys-root/mingw/bin:${PATH}
	cd /tmp/quake2world
	autoreconf -i --force
	./configure --host=${MINGW_ARCH}-w64-mingw32
	make
	make DESTDIR=/tmp/quake2world-${MINGW_TARGET} install
	
	test x${BUILDRELEASE} = xtrue && {
		cd mingw-cross
		./buildrelease.sh
	}
"

archive_workspace 
destroy_chroot
