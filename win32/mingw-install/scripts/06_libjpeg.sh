SOURCE=http://www.ijg.org/files/jpegsrc.v8d.tar.gz

wget -c $SOURCE 
tar xzf jpegsrc.v8d.tar.gz
cd jpeg-8d
./configure --prefix=/mingw --enable-shared --enable-static
make
make install