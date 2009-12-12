#!/bin/sh
#############################################################################
# Copyright(c) 2007-2010 Quake2World.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or(at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#############################################################################

#exit on error
set -e
set -o errexit

START=`pwd`
PATH=$PATH:$START
export PATH
TMP=$START/tmp

rm -Rf /mingw
mkdir /mingw

mkdir -p $TMP/core
mkdir -p $TMP/deps

cd $TMP/core
wget -c http://downloads.sourceforge.net/mingw/binutils-2.19.1-mingw32-bin.tar.gz
wget -c http://downloads.sourceforge.net/mingw/mingwrt-3.17-mingw32-dev.tar.gz
wget -c http://downloads.sourceforge.net/mingw/w32api-3.14-mingw32-dev.tar.gz
wget -c http://downloads.sourceforge.net/mingw/gdb-6.8-mingw-3.tar.bz2
wget -c http://www.libsdl.org/extras/win32/common/directx-devel.tar.gz

wget -c http://downloads.sourceforge.net/project/tdm-gcc/TDM-GCC%204.4%20series/4.4.1-tdm-2%20SJLJ/gcc-4.4.1-tdm-2-core.tar.gz
wget -c http://downloads.sourceforge.net/project/tdm-gcc/TDM-GCC%204.4%20series/4.4.1-tdm-2%20SJLJ/gcc-4.4.1-tdm-2-g%2B%2B.tar.gz

wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20crypt/crypt-1.1_1-2/libcrypt-1.1_1-2-msys-1.0.11-dll-0.tar.lzma?use_mirror
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20perl/perl-5.6.1_2-1/perl-5.6.1_2-1-msys-1.0.11-bin.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20autoconf/autoconf-2.63-1/autoconf-2.63-1-msys-1.0.11-bin.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20automake/automake-1.11-1/automake-1.11-1-msys-1.0.11-bin.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20libtool/libtool-2.2.7a-1/libtool-2.2.7a-1-msys-1.0.11-bin.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20autogen/autogen-5.9.2-2/autogen-5.9.2-2-msys-1.0.11-bin.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20openssl/openssl-0.9.8k-1/libopenssl-0.9.8k-1-msys-1.0.11-dll-098.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20minires/minires-1.02_1-1/libminires-1.02_1-1-msys-1.0.11-dll.tar.lzma
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20openssh/openssh-4.7p1-2/openssh-4.7p1-2-msys-1.0.11-bin.tar.lzma

echo "extract core"
for n in *.tar.gz;do
tar xzvf $n -C /mingw
done

#install msys zlib
rm -f zlib-1.2.3-1-msys-1.0.11-dll.tar.gz
wget -c http://downloads.sourceforge.net/project/mingw/MSYS%20zlib/zlib-1.2.3-1/zlib-1.2.3-1-msys-1.0.11-dll.tar.gz
tar xzf zlib-1.2.3-1-msys-1.0.11-dll.tar.gz -C /

echo "extract debugger"
for n in *.tar.bz2;do
tar xjvf $n -C /mingw
done

echo "extract msys addons"
for i in *.tar.lzma;do
tar xf $i --lzma -C /
done


echo "fetch and compile dependencies"
#compile zlib
cd $TMP/deps
wget -c http://www.zlib.net/zlib-1.2.3.tar.gz
tar xzf zlib-1.2.3.tar.gz
cd zlib-1.2.3
sed 's/dllwrap/dllwrap -mwindows/' win32/Makefile.gcc >Makefile.gcc
make IMPLIB='libz.dll.a' -fMakefile.gcc
cp -fp *.a /mingw/lib
cp -fp zlib.h /mingw/include
cp -fp zconf.h /mingw/include
cp -fp zlib1.dll /mingw/bin
#./configure --prefix=/mingw --shared
make
make install

#compile nasm
cd $TMP/deps
wget -c http://downloads.sourceforge.net/sourceforge/nasm/nasm-2.07.tar.gz
tar xzf nasm-2.07.tar.gz
cd nasm-2.07
./configure --prefix=/mingw
make
make install

#compile libsdl
cd $TMP/deps
wget -c http://www.libsdl.org/release/SDL-1.2.13.tar.gz
tar xzf SDL-1.2.13.tar.gz
cd SDL-1.2.13
./configure --prefix=/mingw
make
make install

#compile libpng (sdl_image dep)
cd $TMP/deps
wget -c http://downloads.sourceforge.net/libpng/libpng-1.2.41.tar.gz
tar xzf libpng-1.2.41.tar.gz
cd libpng-1.2.41
./configure --prefix=/mingw
make
make install

#compile jpeg6b (sdl_image dep)
cd $TMP/deps
wget -c http://www.ijg.org/files/jpegsrc.v7.tar.gz
tar xzf jpegsrc.v7.tar.gz
cd jpeg-7
./configure --prefix=/mingw --enable-shared --enable-static
make
make install

#compile sdl_image
cd $TMP/deps
wget -c http://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.10.tar.gz
tar xzf SDL_image-1.2.10.tar.gz
cd SDL_image-1.2.10
./configure --prefix=/mingw
make
make install

#compile libogg (sdl_mixer dep)
cd $TMP/deps
wget -c http://downloads.xiph.org/releases/ogg/libogg-1.1.4.tar.gz
tar xzf libogg-1.1.4.tar.gz
cd libogg-1.1.4
LDFLAGS='-mwindows' ./configure --prefix=/mingw
make
make install

#compile libvorbis (sdl_mixer dep)
cd $TMP/deps
wget -c http://downloads.xiph.org/releases/vorbis/libvorbis-1.2.3.tar.gz
tar xzf libvorbis-1.2.3.tar.gz
cd libvorbis-1.2.3
LDFLAGS='-mwindows' ./configure --prefix=/mingw
make LIBS='-logg'
make install

#compile sdl_mixer #TODO: newer versions fail to build bcs of Makefile bug
cd $TMP/deps
wget -c http://www.libsdl.org/projects/SDL_mixer/release/SDL_mixer-1.2.8.tar.gz
tar xzf SDL_mixer-1.2.8.tar.gz
cd SDL_mixer-1.2.8
./configure --prefix=/mingw
make
make install

#compile libcurl
cd $TMP/deps
wget -c http://curl.haxx.se/download/curl-7.19.7.tar.gz
tar xzf curl-7.19.7.tar.gz
cd curl-7.19.7
./configure --prefix=/mingw --without-ssl
make
make install

#compile pdcurses
cd $TMP/deps
wget -c http://downloads.sourceforge.net/project/pdcurses/pdcurses/3.4/PDCurses-3.4.tar.gz
tar xzf PDCurses-3.4.tar.gz
cd PDCurses-3.4/win32
make -f gccwin32.mak
cp ../curses.h /mingw/include
cp pdcurses.a /mingw/lib/libpdcurses.a

#cleanups
rm /mingw/*.txt
rm /mingw/*README
rm /mingw/.DS_Store
rm /mingw/._.DS_Store

#copy DLLs
cd $START
rm -Rf DLLs
mkdir DLLs
cd DLLs
cp `find /mingw | grep .dll | grep libcurl-` .
cp `find /mingw | grep .dll | grep libjpeg-` .
cp `find /mingw | grep .dll | grep libogg-` .
cp `find /mingw | grep .dll | grep libpng-` .
cp `find /mingw | grep .dll | grep libvorbis-` .
cp `find /mingw | grep .dll | grep libvorbisfile-` .
cp `find /mingw | grep -v .dll.a | grep SDL.dll` .
cp `find /mingw | grep -v .dll.a | grep SDL_image.dll` .
cp `find /mingw | grep -v .dll.a | grep SDL_mixer.dll` .
cp `find /mingw | grep .dll | grep zlib` .

echo "DONE!"
