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

function SWITCH
{
	if [ -z ${CURRENT_ARCH} ]; then
	  echo "/mingw is not mounted or gcc not installed"
	elif [ ${CURRENT_ARCH} == "i686" ]; then
	  echo "Current toolchain:" ${CURRENT_ARCH}
	  echo "Switching to 64bit toolchain"
	  sed -i 's/mingw32/mingw64/g' /etc/fstab
	  echo -n "New toolchain:"; gcc -v 2>&1|grep Target|cut -d\  -f2
	  
	elif [ ${CURRENT_ARCH} == "x86_64" ]; then
	  echo "Current toolchain:" ${CURRENT_ARCH}
	  echo "Switching to 32bit toolchain"
	  sed -i 's/mingw64/mingw32/g' /etc/fstab
	  echo -n "New toolchain:"; gcc -v 2>&1|grep Target|cut -d\  -f2
	fi
}

#workaround for sed not being able to write to /etc/fstab at the given moment
while [ ${CURRENT_ARCH} == `gcc -v 2>&1|grep Target|cut -d\  -f2|cut -d\- -f1` ]; do
	SWITCH
	sleep 5
done
