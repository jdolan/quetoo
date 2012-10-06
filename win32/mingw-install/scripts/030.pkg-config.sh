PKGNAME="pkg-config"
PKGVER="0.27.1"

SOURCE=http://pkgconfig.freedesktop.org/releases/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE 

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local --with-internal-glib
make -j 4
make install
