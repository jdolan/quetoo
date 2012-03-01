SOURCE=http://ftp.gnu.org/pub/gnu/gettext/gettext-0.18.1.1.tar.gz

wget -c $SOURCE 

tar xzf `ls gettext-*.tar.gz`


cd gettext-*
./configure --prefix=/mingw
make -j 4
make install
