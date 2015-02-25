#!/bin/bash -e

#
# Build entry point for GNU / Linux via chroot.
#
function build() {
	/usr/bin/mock -r ${CHROOT} --cwd /tmp/quetoo --chroot "
		set -e
		autoreconf -i
		./configure ${CONFIGURE_FLAGS}
		make -C linux ${MAKE_OPTIONS} ${MAKE_TARGETS}
	"
}

source $(dirname $0)/common.sh
