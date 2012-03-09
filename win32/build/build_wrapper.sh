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

CURRENTARCH=`gcc -v 2>&1|grep Target|cut -d\  -f2`


function BUILD
{
	rm -f _build.log
	sh _build_win32.sh > _build.log 2>&1

	if [ $? != "0" ];then
		echo "Build error"
		scp _build.log web@satgnu.net:www/satgnu.net/files
		#mailsend.exe -d satgnu.net -smtp 10.0.2.2 -t quake2world-dev@jdolan.dyndns.org -f q2wbuild@satgnu.net -sub "Build FAILED r$NEWREV" +cc +bc < _build.log
	else
    echo "build succeeded"
    	scp _build.log web@satgnu.net:www/satgnu.net/files
		rm _build.log
	fi
}

while true; do
	CURREV=`svn info			 $CURRENTARCH/quake2world|grep Revision:|cut -d\  -f2`
	NEWREV=`svn info svn://jdolan.dyndns.org/quake2world |grep Revision:|cut -d\  -f2`

	echo $CURREV $NEWREV

	if [ $CURREV != $NEWREV -o -e _build.log ];then
		echo "Building" - `date`
		BUILD
		echo "Building done" - `date`
	else
		echo "Nothing has changed and no previsouly failed build." - `date`
	fi

	sleep 3h
	
done
