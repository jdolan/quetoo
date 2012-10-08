PKGNAME="libpng"
PKGVER="1.5.13"

SOURCE=http://downloads.sourceforge.net/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c $SOURCE
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz

cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
