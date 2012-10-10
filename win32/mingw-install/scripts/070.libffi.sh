PKGNAME="libffi"
PKGVER="3.0.11"

SOURCE=ftp://sourceware.org/pub/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c $SOURCE
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local --build=`gcc -v 2>&1|grep Target|cut -d\  -f2`
make -j 4
make install
