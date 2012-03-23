SOURCE=http://zlib.net/zlib-1.2.6.tar.gz

wget -c $SOURCE 

tar xzf `ls zlib-*.tar.gz`


cd zlib-*


export BINARY_PATH="/mingw/local/bin"
export INCLUDE_PATH="/mingw/local/include"
export LIBRARY_PATH="/mingw/local/lib"
sed -i 's/SHARED_MODE=0/SHARED_MODE=1/g'  win32/Makefile.gcc

make -j 4 -f win32/Makefile.gcc
make -fwin32/Makefile.gcc install
