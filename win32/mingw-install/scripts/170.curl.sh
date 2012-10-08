PKGNAME="curl"
PKGVER="7.26.0"

SOURCE=http://curl.haxx.se/download/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c $SOURCE
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz

cd ${PKGNAME}-${PKGVER}

./configure --prefix=/mingw/local --enable-shared --enable-static --disable-ldap \
            --disable-rtsp --disable-dict --disable-telnet --disable-pop3 \
            --disable-imap --disable-smtp --disable-gopher --without-ssl

make -j 4 
make install
