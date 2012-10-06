PKGNAME="libiconv"
PKGVER="1.14"

#SOURCE=http://http://ftp.gnu.org/pub/gnu/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz
#wget -c $SOURCE 

#tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

make distclean
./configure --prefix=/mingw/local
make -j 4
make install
