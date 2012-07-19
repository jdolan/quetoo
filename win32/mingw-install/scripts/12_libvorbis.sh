PKGNAME="libvorbis"
PKGVER="1.3.3"

SOURCE=http://downloads.xiph.org/releases/vorbis/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE 

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

LDFLAGS='-mwindows' ./configure --prefix=/mingw/local
make LIBS='-logg' -j 4
make install
