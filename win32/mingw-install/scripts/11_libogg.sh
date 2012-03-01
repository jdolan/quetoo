#compile libogg (sdl_mixer dep)
SOURCE=http://downloads.xiph.org/releases/ogg/libogg-1.3.0.tar.gz
wget -c $SOURCE 

tar xzf `ls libogg-*.tar.gz`
cd libogg-*
LDFLAGS='-mwindows' ./configure --prefix=/mingw
make -j 4
make install
