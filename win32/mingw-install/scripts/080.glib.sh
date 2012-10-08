PKGNAME="glib"
PKGVER="2.34.0"

SOURCE=http://ftp.gtk.org/pub/${PKGNAME}/2.34/${PKGNAME}-${PKGVER}.tar.xz

pushd ../source
wget -c $SOURCE
popd 

tar xf ../source/${PKGNAME}-${PKGVER}.tar.xz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local
make -j 4
make install
