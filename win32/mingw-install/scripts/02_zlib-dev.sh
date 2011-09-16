SOURCE=http://sourceforge.net/projects/mingw/files/MinGW/zlib/zlib-1.2.5-1/libz-1.2.5-1-mingw32-dev.tar.lzma

wget -c $SOURCE

tar --lzma -xf libz-1.2.5-1-mingw32-dev.tar.lzma -C /mingw

SOURCE=http://sourceforge.net/projects/mingw/files/MinGW/zlib/zlib-1.2.5-1/libz-1.2.5-1-mingw32-dll-1.tar.lzma
wget -c $SOURCE
tar --lzma -xf libz-1.2.5-1-mingw32-dll-1.tar.lzma -C /mingw
