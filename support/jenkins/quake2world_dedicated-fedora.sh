#!/bin/bash
set -e

source ./common.sh

init_chroot

/usr/bin/mock -r ${CHROOT} --shell "

cd /tmp/quake2world
autoreconf -i --force
./configure --with-tests --with-master --without-client
make
./support/jenkins/create_fixtures.sh
make check
make DESTDIR=/tmp/quake2world-${CHROOT} install
" 

archive_workspace
destroy_chroot
