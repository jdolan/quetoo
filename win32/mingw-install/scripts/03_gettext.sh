SOURCE=http://ftp.gnu.org/pub/gnu/gettext/gettext-0.18.1.1.tar.gz

wget -c $SOURCE 

tar xzf `ls gettext-*.tar.gz`


cd gettext-*

wget http://mingw-w64-dgn.googlecode.com/svn/trunk/patch/gettext-0.18.x-w64.patch
patch -p0 < gettext-0.18.x-w64.patch

./configure --prefix=/mingw
make -j 4
make install
