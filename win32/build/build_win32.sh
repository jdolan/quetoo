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

rev=`svn co svn://jdolan.dyndns.org/quake2world/trunk quake2world |grep "evision"|cut -d\  -f 3`
echo checked out $rev

cd $START/quake2world
autoreconf -i --force
./configure
make
cd src/game/default
gcc -shared -o game.dll *.o ../../.libs/libshared.a

cd $START
rm -Rf quake2world_rev* dist
mkdir -p dist/quake2world/default
cd dist/quake2world
cp ../../updater/* .

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

cp $START/quake2world/src/tools/pak/.libs/pak.exe .
cp $START/quake2world/src/tools/q2wmap/.libs/q2wmap.exe .
cp $START/quake2world/src/game/default/game.dll ./default
cp $START/quake2world/src/.libs/quake2world.exe .
cd $START/dist
$START/7za a -tzip -mx=9 ../quake2world_rev"$rev"zip quake2world

cd $START
scp -r dist/quake2world/* maci@jdolan.dyndns.org:/opt/rsync/quake2world-win32

scp quake2world_rev"$rev"zip satgnu.net@satgnu.net:/var/www/satgnu.net/files
ssh satgnu.net@satgnu.net ln -f /var/www/satgnu.net/files/quake2world_rev"$rev"zip  /var/www/satgnu.net/files/quake2world-win32-snapshot.zip
