#!/bin/bash -ex

source ./common.sh

init_chroot

/usr/bin/mock -r ${CHROOT} --shell "
	set -x
	set -e
	cd /tmp/quake2world
	autoreconf -i
	./configure --prefix=/ --without-client
	make
	#cd linux
	#make ${MAKE_OPTIONS} install image
"

destroy_chroot
