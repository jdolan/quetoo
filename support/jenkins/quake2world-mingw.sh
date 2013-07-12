#!/bin/bash -x

source ./common.sh

init_chroot

echo
echo "MINGW_TARGET: ${MINGW_TARGET}"
echo "MINGW_ARCH: ${MINGW_ARCH}"
echo

# Copy the SSH config into the chroot in case this is a release build
/usr/bin/mock -r ${CHROOT} --copyin ~/.ssh "/root/.ssh"

# Now do the build
/usr/bin/mock -r ${CHROOT} --shell "
	set -x
	export PATH=/usr/${MINGW_ARCH}-w64-mingw32/sys-root/mingw/bin:${PATH}
	cd /tmp/quake2world
	autoreconf -i --force
	./configure --host=${MINGW_ARCH}-w64-mingw32
	make
	make DESTDIR=/tmp/quake2world-${MINGW_TARGET} install
	
	if [[ \"$JOB_NAME\" == \"*release*\" ]]; then
		cd mingw-cross
		./release.sh
	fi
"

archive_workspace 
destroy_chroot
