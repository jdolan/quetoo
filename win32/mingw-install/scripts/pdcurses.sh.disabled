SOURCE=http://downloads.sourceforge.net/pdcurses/PDCurses-3.4.tar.gz
wget -c $SOURCE
tar xzf `ls PDCurses-*.tar.gz`
cd PDCurses-*
make -f gccwin32.mak DLL=Y

cp -iv pdcurses.dll /mingw/bin
cp -iv pdcurses.a /mingw/lib/libpdcurses.a
cd ..
cp -iv curses.h panel.h /mingw/include
