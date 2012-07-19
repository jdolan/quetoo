PKGNAME="libogg"
PKGVER="1.3.0"

SOURCE=http://downloads.xiph.org/releases/ogg/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE 

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

LDFLAGS='-mwindows' ./configure --prefix=/mingw/local
make -j 4
make install
