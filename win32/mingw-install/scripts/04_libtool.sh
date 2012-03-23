SOURCE=http://ftpmirror.gnu.org/libtool/libtool-2.4.2.tar.gz

wget -c $SOURCE 

tar xzf `ls libtool-*.tar.gz`


cd libtool-*
./configure --prefix=/mingw/local
make -j 4
make install
