SOURCE=http://www.ijg.org/files/jpegsrc.v8d.tar.gz

wget -c $SOURCE 
tar xzf `ls jpegsrc.*.tar.gz`
cd jpeg-*
./configure --prefix=/mingw/local --enable-shared --enable-static
make -j 4
make install
