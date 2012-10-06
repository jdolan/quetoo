PKGNAME="zlib"
PKGVER="1.2.7"

SOURCE=http://zlib.net/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c $SOURCE
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

export BINARY_PATH="/mingw/local/bin"
export INCLUDE_PATH="/mingw/local/include"
export LIBRARY_PATH="/mingw/local/lib"
sed -i 's/SHARED_MODE=0/SHARED_MODE=1/g'  win32/Makefile.gcc

make -j 4 -f win32/Makefile.gcc
make -fwin32/Makefile.gcc install
