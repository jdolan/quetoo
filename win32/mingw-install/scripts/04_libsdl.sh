SOURCE=http://www.libsdl.org/release/SDL-1.2.15.tar.gz
wget -c $SOURCE


tar xzf SDL-1.2.15.tar.gz
cd SDL-1.2.15

./configure --prefix=/mingw
make -j 4
make install
#evil workaround
rm /mingw/lib/libSDLmain.la