PKGNAME="gettext"
PKGVER="0.18.1.1"

SOURCE=http://http://ftp.gnu.org/pub/gnu/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz \
       http://mingw/local-w64-dgn.googlecode.com/svn/trunk/patch/gettext-0.18.x-w64.patch
 
wget -c $SOURCE 

tar xzf ${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

patch -p0 < ../gettext-0.18.x-w64.patch
./configure --prefix=/mingw/local
make -j 4
make install
