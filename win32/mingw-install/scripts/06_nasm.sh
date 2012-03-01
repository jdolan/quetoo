SOURCE=http://fossies.org/linux/misc/nasm-2.10rc9.tar.gz
wget -c $SOURCE

tar xzf `ls nasm-*.tar.gz`

cd nasm-*

./configure --prefix=/mingw
make -j 4
make install
