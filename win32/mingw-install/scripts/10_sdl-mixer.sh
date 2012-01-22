SOURCE=http://www.libsdl.org/projects/SDL_mixer/release/SDL_mixer-1.2.12.tar.gz

wget -c $SOURCE

tar xzf SDL_mixer-1.2.12.tar.gz
cd SDL_mixer-1.2.12
# workaround for libtool linking order fuckup, just dont build playwave and playmus
sed -i 's#$(objects)/playwave$(EXE) $(objects)/playmus$(EXE)##g' Makefile.in
./configure --prefix=/mingw
make -j 4
make install