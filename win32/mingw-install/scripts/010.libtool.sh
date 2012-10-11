#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="libtool"
PKGVER="2.4.2"
SOURCE=http://ftpmirror.gnu.org/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install
