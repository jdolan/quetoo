#!/bin/bash
#############################################################################
# Copyright(c) 2007 Quake2World.
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

mkdir /mingw
cd /mingw
rm * -Rf

mkdir -p $TMP
cp $START/7za.exe $TMP

cd $TMP
../wget -c http://downloads.sourceforge.net/mingw/binutils-2.18.50-20080109-2.tar.gz
../wget -c http://downloads.sourceforge.net/mingw/mingwrt-3.15.1-mingw32-dev.tar.gz
../wget -c http://downloads.sourceforge.net/mingw/w32api-3.12-mingw32-dev.tar.gz
../wget -c http://downloads.sourceforge.net/mingw/mingw32-make-3.81-20080326-3.tar.gz
../wget -c http://downloads.sourceforge.net/mingw/gdb-6.8-mingw-3.tar.bz2

../wget -c http://downloads.sourceforge.net/tdm-gcc/gcc-4.3.2-tdm-1-core.tar.gz
../wget -c http://downloads.sourceforge.net/tdm-gcc/gcc-4.3.2-tdm-1-g++.tar.gz


../wget -c http://downloads.sourceforge.net/sourceforge/gnuwin32/zlib-1.2.3-lib.zip
../wget -c http://downloads.sourceforge.net/sourceforge/gnuwin32/jpeg-6b-4-lib.zip
../wget -c http://downloads.sourceforge.net/sourceforge/gnuwin32/libpng-1.2.33-lib.zip
../wget -c http://curl.de-mirror.de/download/libcurl-7.16.4-win32-nossl.zip
../wget -c http://www.libsdl.org/release/SDL-devel-1.2.13-mingw32.tar.gz
../wget -c http://www.libsdl.org/projects/SDL_image/release/SDL_image-devel-1.2.6-VC8.zip

echo "extract base"
for n in *.tar.gz;do
tar xzvf $n -C /mingw
done

echo "extract debugger"
for n in *.tar.bz2;do
tar xjvf $n -C /mingw
done

echo "extract compiler"
for n in *.7z;do
cd /mingw
$TMP/7za.exe x -aoa $TMP/$n
done

cd $TMP

echo "extract dependencies"
for n in *.zip;do
cd /mingw
$TMP/7za.exe x -aoa $TMP/$n
done

cd /mingw/SDL-1.2.13
make install-sdl prefix=/mingw
cd ..
rm -Rf SDL-1.2.13

cp /mingw/libcurl-7.16.4/* -R /mingw
rm -Rf /mingw/libcurl-7.16.4

cp /mingw/SDL_image-1.2.6/include/* /mingw/include/SDL
cp /mingw/SDL_image-1.2.6/lib/SDL* /mingw/lib
rm -rf /mingw/SDL_image-1.2.6

