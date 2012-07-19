PKGNAME="SDL_image"
PKGVER="1.2.12"

SOURCE=http://www.libsdl.org/projects/${PKGNAME}/release/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
