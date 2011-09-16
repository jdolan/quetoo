SOURCE=http://www.ijg.org/files/jpegsrc.v8c.tar.gz

wget -c $SOURCE 
tar xzf jpegsrc.v8c.tar.gz
cd jpeg-8c
./configure --prefix=/mingw --enable-shared --enable-static
make
make install