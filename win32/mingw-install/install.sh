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
TMP=$START/tmp

mkdir $TMP
cd $TMP

for n in ../scripts/*.sh
do
sh $n
cd $TMP
done

#copy DLLs
cd $START
rm -Rf DLLs
mkdir DLLs


#cd /mingw/bin
#cp AntTweakBar.dll libcurl-4.dll libpng15-15.dll pdcurses.dll \
#   SDL.dll libgcc_s_dw2-1.dll libvorbis-0.dll SDL_image.dll \
#   libjpeg-8.dll libvorbisfile-3.dll SDL_mixer.dll libogg-0.dll \
#   libz-1.dll $START/DLLs

echo "DONE!"
