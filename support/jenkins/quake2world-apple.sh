#!/bin/bash -ex

source ~/.profile

CONFIGURE_OPTIONS="--prefix=/ ${CONFIGURE_OPTIONS}"
MAKE_OPTIONS="${MAKE_OPTIONS}"

echo
echo "CONFIGURE_OPTIONS: ${CONFIGURE_OPTIONS}"
echo "MAKE_OPTIONS: ${MAKE_OPTIONS}"
echo

autoreconf -i
./configure ${CONFIGURE_OPTIONS}
make -C apple ${MAKE_OPTIONS} bundle release image release-image clean
