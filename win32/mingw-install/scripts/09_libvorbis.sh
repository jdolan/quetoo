#compile libvorbis (sdl_mixer dep)
SOURCE=http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.2.tar.gz
wget -c $SOURCE

tar xzf libvorbis-1.3.2.tar.gz
cd libvorbis-1.3.2
LDFLAGS='-mwindows' ./configure --prefix=/mingw
make LIBS='-logg' -j 4
make install