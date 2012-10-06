PKGNAME="libffi"
PKGVER="3.0.11"

SOURCE=http://ftp.gtk.org/pub/${PKGNAME}/${PKGVER}/${PKGNAME}-${PKGVER}.tar.xz

pushd ../source
wget -c $SOURCE
popd 

tar xf ../source/${PKGNAME}-${PKGVER}.tar.xz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
