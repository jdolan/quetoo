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

if [ -z ${CURRENT_ARCH} ]; then
  echo "/mingw is not mounted or gcc not installed"
fi

if [ ! -d ${CURRENT_ARCH} ];then
	mkdir ${CURRENT_ARCH}
fi

PREFIX=/tmp/quake2world-${CURRENT_ARCH}


cd ${CURRENT_ARCH}
START=`pwd`
svn co svn://quake2world.net/quake2world/trunk quake2world
CURRENT_REVISION=` svn info quake2world/ |grep evision|cut -d\  -f 2`
echo checked out CURRENT_REVISION ${CURRENT_REVISION}


cd ${START}/quake2world
autoreconf -i --force
./configure --prefix=${PREFIX}
#sed -i 's:-O2:-O0:g' $(find . -name Makefile)

# start icon 
cd ${START}/quake2world/win32/build/icons
windres.exe -i quake2world-icon.rc -o quake2world-icon.o
cd ${START}/quake2world/src/main
sed -i 's@-lm -lz -lstdc++@-lm -lz -lstdc++ '${START}'/quake2world/win32/build/icons/quake2world-icon.o@g' Makefile
# end icon

cd ${START}/quake2world
make -j4

make install
cd ${START}/quake2world/src/game/default
gcc -shared -o game.dll *.o ../../.libs/libshared.a
cd ${START}/quake2world/src/cgame/default
gcc -shared -o cgame.dll *.o ../../.libs/libshared.a -lopengl32

cd ${START}
rm -Rf *.zip dist
mkdir -p dist/quake2world/default
cd dist/quake2world
cp ../../../updater/bin/Update.exe .
echo "[Update.exe]" > update.cfg
echo "arch=${CURRENT_ARCH}" >> update.cfg
echo "keep_local_data=true" >> update.cfg
echo "keep_update_config=false" >> update.cfg
updater_version=`cat ../../../updater/src/Update.au3 |grep "version_self ="|cut -d\= -f2|sed 's/ //g'`
echo "version=${updater_version}" >> update.cfg


cp ${PREFIX}/bin/pak.exe .
cp ${PREFIX}/bin/q2wmap.exe .
cp ${PREFIX}/bin/quake2world.exe .
rm -Rf ${PREFIX}
cp ${START}/quake2world/src/game/default/game.dll ./default
cp ${START}/quake2world/src/cgame/default/cgame.dll ./default

LIBS=`ldd.exe -R quake2world.exe |grep mingw|cut -d\: -f 2|cut -d\  -f1|cut -d\\\ -f4-0|sed 's@\\\@/@g' | sed 's/.*mingw[0-9][0-9]\/\(.*\)/\1/'`

cd /mingw
cp ${LIBS} ${START}/dist/quake2world


cd ${START}/dist
zip -9 -x quake2world/quake2world.exe quake2world/q2wmap.exe quake2world/pak.exe -r ../quake2world-BETA-${CURRENT_ARCH}.zip quake2world

cd ${START}

../_rsync_retry.sh -vrzhP --timeout=120 --chmod="u=rwx,go=rx" -p --delete --inplace --rsh='ssh' dist/quake2world/* maci@quake2world.net:/opt/rsync/quake2world-win32/${CURRENT_ARCH}
../_rsync_retry.sh -vrzhP --timeout=120 --delete --inplace --rsh='ssh' quake2world-BETA-${CURRENT_ARCH}.zip maci@quake2world.net:/var/www/quake2world/files/quake2world-BETA-${CURRENT_ARCH}.zip
../_rsync_retry.sh -vrzhP --timeout=120 --delete --inplace --rsh='ssh' dist/quake2world/Update.exe maci@quake2world.net:/var/www/quake2world/files/Update.exe
