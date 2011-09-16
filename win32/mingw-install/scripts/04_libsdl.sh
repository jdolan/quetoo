SOURCE=http://www.libsdl.org/release/SDL-1.2.14.tar.gz
wget -c $SOURCE


tar xzf SDL-1.2.14.tar.gz
cd SDL-1.2.14

./configure --prefix=/mingw
make -j 4
make install