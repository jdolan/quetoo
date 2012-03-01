
SOURCE=http://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.12.tar.gz
wget -c $SOURCE

tar xzf `ls SDL_image-*.tar.gz`
cd SDL_image-*

export CPPFLAGS="-I/mingw/include"
export LDFLAGS="-L/mingw/lib"

./configure --prefix=/mingw
make -j 4
make install
