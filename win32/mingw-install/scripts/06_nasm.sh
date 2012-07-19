PKGNAME="nasm"
PKGVER="2.10.01"

SOURCE=http://www.nasm.us/pub/${PKGNAME}/releasebuilds/${PKGVER}/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
