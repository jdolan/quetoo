SOURCE=http://www.buraphalinux.org/download/bls2.3/dvd_source/source1/development/nasm/nasm-2.09.10.tar.bz2
wget -c $SOURCE

tar xjf nasm-2.09.10.tar.bz2

cd nasm-2.09.10

./configure --prefix=/mingw
make
make install