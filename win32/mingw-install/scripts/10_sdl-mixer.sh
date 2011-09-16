#compile sdl_mixer #TODO: newer versions fail to build bcs of Makefile bug
SOURCE=http://www.libsdl.org/projects/SDL_mixer/release/SDL_mixer-1.2.11.tar.gz

wget -c $SOURCE

tar xzf SDL_mixer-1.2.11.tar.gz
cd SDL_mixer-1.2.11
./configure --prefix=/mingw
make -j 4
make install