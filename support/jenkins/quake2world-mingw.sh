#!/bin/bash -ex

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
/usr/bin/mock -r ${CHROOT} --cwd /tmp/quake2world --chroot "
	export PATH=/usr/${MINGW_HOST}/sys-root/mingw/bin:${PATH}
	autoreconf -i
	./configure --host=${MINGW_HOST} --prefix=/
	make
	cd mingw-cross
	make ${MAKE_OPTIONS} bundle release image release-image
"

destroy_chroot
