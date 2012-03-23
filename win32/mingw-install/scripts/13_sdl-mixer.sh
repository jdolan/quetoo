SOURCE=http://www.libsdl.org/projects/SDL_mixer/release/SDL_mixer-1.2.12.tar.gz

wget -c $SOURCE

tar xzf `ls SDL_mixer-*.tar.gz`
cd SDL_mixer-*

./configure --prefix=/mingw/local
make -j 4
make install
