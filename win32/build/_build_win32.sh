#!/bin/sh
#############################################################################
# Copyright(c) 2007-2012 Quake2World.
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

CURRENT_ARCH=`gcc -v 2>&1|grep Target|cut -d\  -f2|cut -d\- -f1`

if [ -z $CURRENT_ARCH ]; then
  echo "/mingw is not mounted or gcc not installed"
fi

if [ ! -d $CURRENT_ARCH ];then
	mkdir $CURRENT_ARCH
fi

PREFIX=/tmp/quake2world-$CURRENT_ARCH


cd $CURRENT_ARCH
START=`pwd`
svn co svn://jdolan.dyndns.org/quake2world/trunk quake2world
CURRENT_REVISION=` svn info quake2world/ |grep evision|cut -d\  -f 2`
echo checked out CURRENT_REVISION $CURRENT_REVISION


cd $START/quake2world/win32/build/icons
windres.exe -i quake2world-icon.rc -o quake2world-icon.o

cd $START/quake2world
autoreconf -i --force
./configure --prefix=$PREFIX
#sed -i 's:-O2:-O0:g' $(find . -name Makefile)

make

# start icon 
cd $START/quake2world/src/main
make clean
BUILDLINE=`make 2>&1 |grep "/libtool"`
$BUILDLINE $START/quake2world/win32/build/icons/quake2world-icon.o
cd $START/quake2world
# end icon

make install
cd $START/quake2world/src/game/default
gcc -shared -o game.dll *.o ../../.libs/libshared.a
cd $START/quake2world/src/cgame/default
gcc -shared -o cgame.dll *.o ../../.libs/libshared.a -lopengl32


cd $START
rm -Rf *.zip dist
mkdir -p dist/quake2world/default
cd dist/quake2world
cp ../../../updater/bin/Update.exe .
echo "[Update.exe]" > update.cfg
echo "arch=$CURRENT_ARCH" >> update.cfg
echo "keep_local_data=true" >> update.cfg
echo "keep_update_config=false" >> update.cfg
echo "version=4" >> update.cfg


cp $PREFIX/bin/pak.exe .
cp $PREFIX/bin/q2wmap.exe .
cp $PREFIX/bin/quake2world.exe .
rm -Rf $PREFIX
cp $START/quake2world/src/game/default/game.dll ./default
cp $START/quake2world/src/cgame/default/cgame.dll ./default

LIBS=`ldd.exe -R quake2world.exe |grep mingw|cut -d\: -f 2|cut -d\  -f1|cut -d\\\ -f4-0|sed 's@\\\@/@g' | sed 's/.*mingw[0-9][0-9]\/\(.*\)/\1/'`

cd /mingw
cp $LIBS $START/dist/quake2world


cd $START/dist
zip -9 -r ../quake2world-"$CURRENT_ARCH"-svn"$CURRENT_REVISION".zip quake2world

cd $START

../_rsync_retry.sh -vrzhP --timeout=120 --chmod="u=rwx,go=rx" -p --delete --inplace --rsh='ssh' dist/quake2world/* maci@jdolan.dyndns.org:/opt/rsync/quake2world-win32/"$CURRENT_ARCH"
../_rsync_retry.sh -vrzhP --timeout=120 --chmod="u=rwx,go=rx" -p --delete --inplace --rsh='ssh' dist/quake2world/* web@satgnu.net:www/satgnu.net/files/quake2world/"$CURRENT_ARCH"

../_rsync_retry.sh -vrzhP --timeout=120 --chmod="u=rwx,go=rx" -p --delete --inplace --rsh='ssh' quake2world-"$CURRENT_ARCH"-svn"$CURRENT_REVISION".zip web@satgnu.net:www/satgnu.net/files/quake2world-"$CURRENT_ARCH"-svn"$CURRENT_REVISION".zip
ssh web@satgnu.net ln -f /home/web/www/satgnu.net/files/quake2world-"$CURRENT_ARCH"-svn"$CURRENT_REVISION".zip /home/web/www/satgnu.net/files/quake2world-"$CURRENT_ARCH"-BETA.zip
