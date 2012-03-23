#compile libpng (sdl_image dep)
SOURCE=http://downloads.sourceforge.net/libpng/libpng-1.5.9.tar.gz

wget -c $SOURCE 

tar xzf `ls libpng-*.tar.gz`


cd libpng-*

./configure --prefix=/mingw/local
make -j 4
make install
