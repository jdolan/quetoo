#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="jpegsrc"
PKGVER="v8d"
SOURCE=http://www.ijg.org/files/${PKGNAME}.${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}.${PKGVER}.tar.gz
cd jpeg-*

./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install
