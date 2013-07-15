#!/bin/bash -x

source ./common.sh

init_chroot

echo
echo "MINGW_HOST: ${MINGW_HOST}"
echo "MINGW_ARCH: ${MINGW_ARCH}"
echo

MAKE_OPTIONS="MINGW_HOST=${MINGW_HOST} MINGW_ARCH=${MINGW_ARCH} ${MAKE_OPTIONS}" 

#
# Copy the SSH config into the chroot in case this is a release build
#
/usr/bin/mock -r ${CHROOT} --copyin ~/.ssh "/root/.ssh"

#
# Now do the build
#
/usr/bin/mock -r ${CHROOT} --shell "
	set -x
	export PATH=/usr/${MINGW_HOST}/sys-root/mingw/bin:${PATH}
	cd /tmp/quake2world
	autoreconf -i --force
	./configure --host=${MINGW_HOST} --prefix=/
	make
	make DESTDIR=/tmp/quake2world install
	cd mingw-cross
	make ${MAKE_OPTIONS} bundle release image release-image
"

archive_workspace
destroy_chroot
