
SOURCE=http://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.12.tar.gz
wget -c $SOURCE

tar xzf `ls SDL_image-*.tar.gz`
cd SDL_image-*

./configure --prefix=/mingw/local
make -j 4
make install
