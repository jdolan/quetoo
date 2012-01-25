#!/bin/sh
#############################################################################
# Copyright(c) 2007-2011 Quake2World.
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
svn co svn://jdolan.dyndns.org/quake2world/trunk quake2world
rev=` svn info quake2world/ |grep evision|cut -d\  -f 2`
echo checked out revision $rev

cd $START/quake2world
autoreconf -i --force
./configure --prefix=/tmp/quake2world
make
make install
cd $START/quake2world/src/game/default
gcc -shared -o game.dll *.o ../../.libs/libshared.a
cd $START/quake2world/src/cgame/default
gcc -shared -o cgame.dll *.o ../../.libs/libshared.a -lopengl32


cd $START
rm -Rf quake2world_rev* dist
mkdir -p dist/quake2world/default
cd dist/quake2world
cp ../../updater/* .

cp /tmp/quake2world/bin/pak.exe .
cp /tmp/quake2world/bin/q2wmap.exe .
cp /tmp/quake2world/bin/quake2world.exe .
rm -Rf /tmp/quake2world
cp $START/quake2world/src/game/default/game.dll ./default
cp $START/quake2world/src/cgame/default/cgame.dll ./default

cd /mingw/bin
cp AntTweakBar.dll libcurl-4.dll libz-1.dll libpng15-15.dll \
   libgcc_s_dw2-1.dll libjpeg-8.dll libpdcurses.dll  \
   SDL.dll libvorbis-0.dll SDL_image.dll \
   libvorbisfile-3.dll SDL_mixer.dll libogg-0.dll \
    $START/dist/quake2world

cd $START/dist
zip -9 -r ../quake2world_rev"$rev".zip quake2world

cd $START
#scp -r dist/quake2world/* maci@jdolan.dyndns.org:/opt/rsync/quake2world-win32
#scp quake2world_rev"$rev".zip web@satgnu.net:www/satgnu.net/files

_rsync_retry.sh -vrz --progress --inplace --rsh='ssh' dist/quake2world/* maci@jdolan.dyndns.org:/opt/rsync/quake2world-win32
_rsync_retry.sh -vrz --progress --delete --inplace --rsh='ssh' dist/quake2world/* web@satgnu.net:www/satgnu.net/files/quake2world
_rsync_retry.sh -vrz --progress --inplace --rsh='ssh' quake2world_rev"$rev".zip web@satgnu.net:www/satgnu.net/files

#ssh web@satgnu.net zip -9 -r /home/web/www/satgnu.net/files/quake2world_rev"$rev".zip  /home/web/www/satgnu.net/files/quake2world

ssh web@satgnu.net ln -f /home/web/www/satgnu.net/files/quake2world_rev"$rev".zip  /home/web/www/satgnu.net/files/quake2world-win32-snapshot.zip
