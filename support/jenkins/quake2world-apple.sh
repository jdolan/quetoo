#!/bin/bash -e

#
# Build entry point for Apple OS X.
#
function build() {
	source ~/.profile
	autoreconf -i
	./configure ${CONFIGURE_FLAGS}
	make -C apple ${MAKE_OPTIONS} ${MAKE_TARGETS}
}

source $(dirname $0)/common.sh
