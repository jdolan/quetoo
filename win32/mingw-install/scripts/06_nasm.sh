SOURCE=http://www.nasm.us/pub/nasm/releasebuilds/2.10/nasm-2.10.tar.gz
wget -c $SOURCE

tar xzf `ls nasm-*.tar.gz`

cd nasm-*

./configure --prefix=/mingw
make -j 4
make install
