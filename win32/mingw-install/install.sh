#!/bin/bash
#############################################################################
# Copyright(c) 2007-2009 Quake2World.
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

START=`pwd`
TMP=$START/tmp

rm -Rf /mingw
mkdir /mingw

mkdir -p $TMP/core
mkdir -p $TMP/deps

cd $TMP/core
wget -c http://downloads.sourceforge.net/mingw/binutils-2.19.1-mingw32-bin.tar.gz
wget -c http://downloads.sourceforge.net/mingw/mingwrt-3.15.2-mingw32-dev.tar.gz
wget -c http://downloads.sourceforge.net/mingw/w32api-3.13-mingw32-dev.tar.gz
wget -c http://downloads.sourceforge.net/mingw/gdb-6.8-mingw-3.tar.bz2
wget -c http://www.libsdl.org/extras/win32/common/directx-devel.tar.gz

wget -c http://downloads.sourceforge.net/tdm-gcc/gcc-4.4.0-tdm-1-core-2.tar.gz
wget -c http://downloads.sourceforge.net/tdm-gcc/gcc-4.4.0-tdm-1-g%2B%2B-2.tar.gz


echo "extract core"
for n in *.tar.gz;do
tar xzvf $n -C /mingw
done

echo "extract debugger"
for n in *.tar.bz2;do
tar xjvf $n -C /mingw
done


echo "fetch and compile dependencies"
#compile zlib
cd $TMP/deps
wget -c http://www.zlib.net/zlib-1.2.3.tar.gz
tar xzf zlib-1.2.3.tar.gz
cd zlib-1.2.3
sed 's/dllwrap/dllwrap -mwindows/' win32/Makefile.gcc >Makefile.gcc
make IMPLIB='libz.dll.a' -fMakefile.gcc || return 1
cp -fp *.a /mingw/lib
cp -fp zlib.h /mingw/include
cp -fp zconf.h /mingw/include
cp -fp zlib1.dll /mingw/bin
#./configure --prefix=/mingw --shared
make || return 1
make install || return 1

#compile nasm
cd $TMP/deps
wget -c http://downloads.sourceforge.net/sourceforge/nasm/nasm-2.06.tar.gz
tar xzf nasm-2.06.tar.gz
cd nasm-2.06
./configure --prefix=/mingw
make || return 1
make install || return 1

#compile libsdl
cd $TMP/deps
wget -c http://www.libsdl.org/release/SDL-1.2.13.tar.gz
tar xzf SDL-1.2.13.tar.gz
cd SDL-1.2.13
./configure --prefix=/mingw
make || return 1
make install || return 1

#compile libpng (sdl_image dep)
cd $TMP/deps
wget -c http://downloads.sourceforge.net/libpng/libpng-1.2.37.tar.gz
tar xzf libpng-1.2.37.tar.gz
cd libpng-1.2.37
awk 'NR==12478{$0="  return \"1.2.3\";";}1' configure > configure.new
mv configure.new configure
./configure --prefix=/mingw
make || return 1
make install || return 1

#compile jpeg6b (sdl_image dep)
cd $TMP/deps
wget -c http://www.ijg.org/files/jpegsrc.v7.tar.gz
tar xzf jpegsrc.v7.tar.gz
cd jpeg-7
./configure --prefix=/mingw --enable-shared --enable-static
make || return 1
make install || return 1

#compile sdl_image
cd $TMP/deps
wget -c http://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.7.tar.gz
tar xzf SDL_image-1.2.7.tar.gz
cd SDL_image-1.2.7
./configure --prefix=/mingw
make || return 1
make install || return 1

#compile smpeg (sdl_mixer dep)
cd $TMP/deps
svn co -r 374 svn://svn.icculus.org/smpeg/trunk smpeg
cd smpeg
./autogen.sh
./configure --prefix=/mingw --disable-gtk-player --disable-opengl-player --disable-gtktest --enable-mmx
make CXXLD='$(CXX) -no-undefined' || return 1
make install || return 1

#compile libogg (sdl_mixer dep)
cd $TMP/deps
wget -c http://downloads.xiph.org/releases/ogg/libogg-1.1.4.tar.gz
tar xzf libogg-1.1.4.tar.gz
cd libogg-1.1.4
LDFLAGS='-mwindows' ./configure --prefix=/mingw
make || return 1
make install || return 1

#compile libvorbis (sdl_mixer dep)
cd $TMP/deps
wget -c http://downloads.xiph.org/releases/vorbis/libvorbis-1.2.2.tar.gz
tar xzf libvorbis-1.2.2.tar.gz
cd libvorbis-1.2.2
LDFLAGS='-mwindows' ./configure --prefix=/mingw
make LIBS='-logg' || return 1
make install || return 1

#compile sdl_mixer
cd $TMP/deps
wget -c http://www.libsdl.org/projects/SDL_mixer/release/SDL_mixer-1.2.8.tar.gz
tar xzf SDL_mixer-1.2.8.tar.gz
cd SDL_mixer-1.2.8
./configure --prefix=/mingw
make || return 1
make install || return 1

#compile libcurl
cd $TMP/deps
wget -c http://curl.haxx.se/download/curl-7.19.5.tar.gz
tar xzf curl-7.19.5.tar.gz
cd curl-7.19.5
./configure --prefix=/mingw --without-ssl
make || return 1
make install || return 1

#cleanups
rm /mingw/*.txt
rm /mingw/*README
rm /mingw/.DS_Store
rm /mingw/._.DS_Store

echo "DONE!"
