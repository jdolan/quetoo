SOURCE=http://zlib.net/zlib-1.2.6.tar.gz

wget -c $SOURCE 

tar xzf `ls zlib-*.tar.gz`


cd zlib-*


export SHARED_MODE=1
export BINARY_PATH="/mingw/bin"
export INCLUDE_PATH="/mingw/include"
export LIBRARY_PATH="/mingw/lib"
make -j 4 -fwin32/Makefile.gcc
make -fwin32/Makefile.gcc install
