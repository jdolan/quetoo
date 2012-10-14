#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="SDL"
PKGVER="1.2.15"
SOURCE=http://www.libsdl.org/release/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

#fix DLL version number
sed -i 's@1,2,14,0@1,2,15,0@g' src/main/win32/version.rc
./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install
#evil workaround for compilation order if the .la file exists
rm /mingw/local/lib/libSDLmain.la
