PKGNAME="libtool"
PKGVER="2.4.2"

SOURCE=http://ftpmirror.gnu.org/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz
wget -c $SOURCE 

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
