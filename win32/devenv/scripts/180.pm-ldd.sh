#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="pm-ldd"
PKGVER="0.1"
#SOURCE=http://

#pushd ../source
#wget -c ${SOURCE} 
#popd 

#tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
#cd ${PKGNAME}*

cp ../${PKGNAME}.sh /mingw/local/bin
