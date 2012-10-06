PKGNAME="libiconv"
PKGVER="1.14"

SOURCE=http://http://ftp.gnu.org/pub/gnu/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c $SOURCE
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
