#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="expat"
PKGVER="2.0.1"
SOURCE=http://downloads.sourceforge.net/project/${PKGNAME}/${PKGNAME}/${PKGVER}/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install
