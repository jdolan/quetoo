#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="SDL_mixer"
PKGVER="1.2.12"
SOURCE=http://www.libsdl.org/projects/${PKGNAME}/release/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz --exclude=Xcode
cd ${PKGNAME}-${PKGVER}

./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install
