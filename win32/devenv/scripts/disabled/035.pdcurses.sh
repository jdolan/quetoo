#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="PDCurses"
PKGVER="3.4"
SOURCE=http://downloads.sourceforge.net/pdcurses/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c ${SOURCE}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --build=${TARGET} --prefix=/mingw/local
make -j 4
make install

make -f gccwin32.mak DLL=Y

cp -iv pdcurses.dll /mingw/local/bin
cp -iv pdcurses.a /mingw/local/lib/libpdcurses.a
cd ..
cp -iv curses.h panel.h /mingw/local/include
