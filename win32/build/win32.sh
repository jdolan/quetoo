#!/bin/bash
#################################################################################
# Copyright(c) 2007-2008 Quake2World.
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
#################################################################################

#UPLOAD: set to 1 to upload q2w only, set to something else to upload everything

UL=2
START=`pwd`

CHECKOUT(){
	rev=`svn co svn://jdolan.dyndns.org/quake2world |grep "evision"|cut -d\  -f 3`
	echo checked out $rev
}

CONFIGURATION(){
	cd $START/quake2world
	autoreconf -i --force
	./configure --with-tools='q2wmap pak'
}

MAKE(){
	cd $START/quake2world
	make
	cd src/game/default
	gcc -shared -o game.dll *.o ../../.libs/libshared.a
}

PACKAGE(){
	cd $START
	rm quake2world_rev*
	mkdir -p dist/quake2world/default
	cp deps/* dist/quake2world
	mv $START/quake2world/src/tools/pak/pak.exe $START/dist/quake2world
	mv $START/quake2world/src/tools/q2wmap/q2wmap.exe $START/dist/quake2world
	cp quake2world/src/game/default/game.dll dist/quake2world/default
	cp quake2world/src/quake2world.exe dist/quake2world
	cd $START/dist/quake2world
	strip *.exe
	strip default/game.dll
	cd ..
	$START/7za a -tzip -mx=9 ../quake2world_rev"$rev"zip quake2world
}

UPLOAD(){
	cd $START
	if [ $UL == 1 ]; then
		scp dist/quake2world/quake2world.exe satgnu.net@81.169.143.159:/var/www/satgnu.net/rsync/quake2world-win32
		scp dist/quake2world/pak.exe satgnu.net@81.169.143.159:/var/www/satgnu.net/rsync/quake2world-win32
		scp dist/quake2world/q2wmap.exe satgnu.net@81.169.143.159:/var/www/satgnu.net/rsync/quake2world-win32
		scp -r dist/quake2world/default satgnu.net@81.169.143.159:/var/www/satgnu.net/rsync/quake2world-win32/default
	else		
		scp -r dist/quake2world/* satgnu.net@81.169.143.159:/var/www/satgnu.net/rsync/quake2world-win32
		scp -r dist/quake2world/default satgnu.net@81.169.143.159:/var/www/satgnu.net/rsync/quake2world-win32/default	
	fi
	
	scp quake2world_rev"$rev"zip satgnu.net@81.169.143.159:/var/www/satgnu.net/public_html/maci/files
	ssh satgnu.net@satgnu.net ln -sf /var/www/satgnu.net/public_html/maci/files/quake2world_rev"$rev"zip  /var/www/satgnu.net/public_html/maci/files/quake2world-win32-snapshot.zip
}

CHECKOUT
CONFIGURATION
MAKE
PACKAGE
UPLOAD

