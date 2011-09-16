
SOURCE=http://downloads.sourceforge.net/pdcurses/PDCurses-3.3.tar.gz
wget -c $SOURCE
tar xzf PDCurses-3.3.tar.gz
cd PDCurses-3.3/win32/
make -f gccwin32.mak DLL=Y

cp pdcurses.dll /mingw/bin/.
cp pdcurses.a /mingw/lib/libpdcurses.a
cd ..
cp *.h /mingw/include/.