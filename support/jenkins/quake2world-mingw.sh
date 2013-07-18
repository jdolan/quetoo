#!/bin/bash -e

#
# Build entry point for MinGW cross-compile via chroot.
#
function build() {
	/usr/bin/mock -r ${CHROOT} --cwd /tmp/quake2world --chroot "
		set -e
		export PATH=/usr/${HOST}/sys-root/mingw/bin:${PATH}
		autoreconf -i
		./configure ${CONFIGURE_FLAGS}
		make -C mingw-cross ${MAKE_OPTIONS} ${MAKE_TARGETS}
	"
}

source ./common.sh
