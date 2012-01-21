#compile libcurl
SOURCE=http://curl.haxx.se/download/curl-7.23.1.tar.gz

wget -c $SOURCE 

tar xzf curl-7.23.1.tar.gz
cd curl-7.23.1
./configure --prefix=/mingw --without-ssl
make -j 4
make install