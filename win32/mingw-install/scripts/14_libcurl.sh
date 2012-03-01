#compile libcurl
SOURCE=http://curl.haxx.se/download/curl-7.24.0.tar.gz

wget -c $SOURCE 

tar xzf `ls curl-*.tar.gz`
cd curl-*
./configure --prefix=/mingw --without-ssl
make -j 4
make install
