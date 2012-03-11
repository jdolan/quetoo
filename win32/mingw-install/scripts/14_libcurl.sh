#compile libcurl
SOURCE=http://curl.haxx.se/download/curl-7.24.0.tar.gz

wget -c $SOURCE 

tar xzf `ls curl-*.tar.gz`
cd curl-*
./configure --prefix=/mingw --enable-shared --enable-static --disable-ldap \
--disable-rtsp --disable-dict --disable-telnet --disable-pop3 \
--disable-imap --disable-smtp --disable-gopher --without-ssl

make -j 4 
make install

