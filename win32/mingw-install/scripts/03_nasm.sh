SOURCE=http://fossies.org/linux/misc/nasm-2.09.10.tar.gz
wget -c $SOURCE

tar xzf nasm-2.09.10.tar.gz

cd nasm-2.09.10

./configure --prefix=/mingw
make
make install