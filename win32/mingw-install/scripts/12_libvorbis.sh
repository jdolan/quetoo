#compile libvorbis (sdl_mixer dep)
SOURCE=http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.2.tar.gz
wget -c $SOURCE

tar xzf `ls libvorbis-*.tar.gz`
cd libvorbis-*
LDFLAGS='-mwindows' ./configure --prefix=/mingw/local
make LIBS='-logg' -j 4
make install
