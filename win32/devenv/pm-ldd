#!/bin/bash
#############################################################################
# Copyright(c) 2012 Marcel Wysocki <maci@satgnu.net>
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

# Poor mans ldd (pm-ldd) v.0.1
#
# this script for msys/mingw takes a PE binary or shared library
# as argument and recursively checks which other shared libraries it 
# depends on. It then outputs the whole list in a fashion which makes
# it easy to copy all dependencies next to a binary
# It does not output shared libraries present in the WINDOWS directory.
# It is not optimized and might check the same file multiple times.
#
# Example:
# $ pm-ldd /mingw/local/bin/iconv.exe
#  /mingw/local/bin/libiconv-2.dll
#  /mingw/local/bin/libintl-8.dll
#
#

function analyze( ){
	if [ "$1" != "" ]; then
		for i in `objdump.exe -p $1 |grep "DLL Name:" |cut -d\: -f2|cut -d\  -f2|sort |uniq`; do
			file=`which $i|grep -v WINDOWS`
			echo $file
			analyze $file
		done
	fi
}

analyze $1|sort|uniq|grep "\n"
