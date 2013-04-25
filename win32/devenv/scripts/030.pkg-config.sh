#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="pkg-config"
PKGVER="0.27.1"
SOURCE=http://pkgconfig.freedesktop.org/releases/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --build=${TARGET} --prefix=/mingw/local --with-internal-glib
make -j 4
make install
