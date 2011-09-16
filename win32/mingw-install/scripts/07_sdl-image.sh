
SOURCE=http://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.10.tar.gz
wget -c $SOURCE

tar xzf SDL_image-1.2.10.tar.gz
cd SDL_image-1.2.10

#workaround for silly bug
cp ../libpng*/pnginfo.h .
cp ../libpng*/pngstruct.h .
patch -p0 < ../../scripts/sdl_image.diff


./configure --prefix=/mingw
make -j 4
make install