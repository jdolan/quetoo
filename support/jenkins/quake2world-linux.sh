#!/bin/bash -ex

source ./common.sh

init_chroot

CONFIGURE_OPTIONS="--prefix=/ ${CONFIGURE_OPTIONS}"
MAKE_OPTIONS="${MAKE_OPTIONS}"

echo
echo "CONFIGURE_OPTIONS: ${CONFIGURE_OPTIONS}"
echo "MAKE_OPTIONS: ${MAKE_OPTIONS}"
echo

/usr/bin/mock -r ${CHROOT} --cwd /tmp/quake2world --chroot "
	set -e
	set -x
	autoreconf -i
	./configure ${CONFIGURE_OPTIONS}
	make
	make -C linux ${MAKE_OPTIONS} install image
"

destroy_chroot
