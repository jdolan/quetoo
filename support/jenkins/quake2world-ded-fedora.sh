#!/bin/bash -ex

source ./common.sh

init_chroot

/usr/bin/mock -r ${CHROOT} --shell "
	set -x
	cd /tmp/quake2world
	autoreconf -i --force
	./configure --prefix=/ --with-tests --with-master --without-client
	make
	./support/jenkins/create_fixtures.sh
	make check
	make DESTDIR=/tmp/quake2world install
	cd linux
	make ${MAKE_OPTIONS} install image
"

archive_workspace
destroy_chroot
