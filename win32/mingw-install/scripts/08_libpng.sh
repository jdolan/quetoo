PKGNAME="libpng"
PKGVER="1.5.12"

SOURCE=http://downloads.sourceforge.net/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE 

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
