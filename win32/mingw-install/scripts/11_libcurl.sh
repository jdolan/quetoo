#compile libcurl
SOURCE=http://curl.haxx.se/download/curl-7.22.0.tar.gz

wget -c $SOURCE 

tar xzf curl-7.22.0.tar.gz
cd curl-7.22.0
./configure --prefix=/mingw --without-ssl
make -j 4
make install