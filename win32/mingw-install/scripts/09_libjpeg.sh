PKGNAME="jpegsrc"
PKGVER="v8d"

SOURCE=http://www.ijg.org/files/${PKGNAME}.${PKGVER}.tar.gz
wget -c $SOURCE

tar xzf ${PKGNAME}.${PKGVER}.tar.gz
cd jpeg-*

./configure --prefix=/mingw/local --enable-shared --enable-static
make -j 4
make install
