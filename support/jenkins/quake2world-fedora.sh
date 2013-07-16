#!/bin/bash -ex

source ./common.sh

init_chroot

/usr/bin/mock -r ${CHROOT} --cwd /tmp/quake2world --chroot "
	autoreconf -i
	./configure --prefix=/ --with-tests --with-master
	make
	./support/jenkins/create_fixtures.sh
	make check
	cd linux
	make ${MAKE_OPTIONS} install image
"

destroy_chroot
