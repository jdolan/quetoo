#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="gettext"
PKGVER="0.18.1.1"
SOURCE=http://ftp.gnu.org/pub/gnu/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz
#SOURCE2=http://mingw-w64-dgn.googlecode.com/svn/trunk/patch/gettext-0.18.x-w64.patch
SOURCE2=http://savannah.gnu.org/support/download.php?file_id=25639

pushd ../source
wget -c ${SOURCE}
wget -c -O gettext-0.18.x-w64.patch ${SOURCE2}
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

patch -p0 < ../../source/gettext-0.18.x-w64.patch
./configure --build=${TARGET} --enable-threads=win32 --prefix=/mingw/local
make -j 4
make install
