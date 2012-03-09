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

### ABOUT
### Runs rsync, retrying on errors up to a maximum number of tries.
 
# Trap interrupts and exit instead of continuing the loop
trap "echo Exited!; exit;" SIGINT SIGTERM
 
MAX_RETRIES=1000
i=0
# Set the initial return value to failure
false
 
while [ $? -ne 0 -a $i -lt $MAX_RETRIES ]
do
  i=$(($i+1))
  `which rsync` $*
done
 
if [ $i -eq $MAX_RETRIES ]
  then
  echo "Hit maximum number of retries, giving up."
fi
