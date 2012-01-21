#compile libpng (sdl_image dep)
SOURCE=http://downloads.sourceforge.net/libpng/libpng-1.5.7.tar.gz

wget -c $SOURCE 

tar xzf libpng-1.5.7.tar.gz


cd libpng-1.5.7
./configure --prefix=/mingw
make -j 4
make install