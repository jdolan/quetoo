#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="libffi"
PKGVER="3.0.11"
SOURCE=ftp://sourceware.org/pub/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install
