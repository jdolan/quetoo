PKGNAME="SDL"
PKGVER="1.2.15"

SOURCE=http://www.libsdl.org/release/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
#evil workaround
rm /mingw/local/lib/libSDLmain.la
