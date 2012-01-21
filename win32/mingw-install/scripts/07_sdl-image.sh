
SOURCE=http://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.11.tar.gz
wget -c $SOURCE

tar xzf SDL_image-1.2.11.tar.gz
cd SDL_image-1.2.11

./configure --prefix=/mingw
make -j 4
make install