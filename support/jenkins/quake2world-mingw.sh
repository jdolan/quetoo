#!/bin/bash -ex

source ./common.sh

init_chroot

CONFIGURE_OPTIONS="--host=${MINGW_HOST} --prefix=/ ${CONFIGURE_OPTIONS}"
MAKE_OPTIONS="MINGW_ARCH=${MINGW_ARCH} ${MAKE_OPTIONS}" 

echo
echo "CONFIGURE_OPTIONS: ${CONFIGURE_OPTIONS}"
echo "MAKE_OPTIONS: ${MAKE_OPTIONS}"
echo

/usr/bin/mock -r ${CHROOT} --cwd /tmp/quake2world --chroot "
	set -e
	set -x
	export PATH=/usr/${MINGW_HOST}/sys-root/mingw/bin:${PATH}
	autoreconf -i
	./configure ${CONFIGURE_OPTIONS}
	make -C mingw-cross ${MAKE_OPTIONS} bundle release image release-image
"

destroy_chroot
