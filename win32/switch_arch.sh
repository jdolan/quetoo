#!/bin/bash
CURRENTARCH=`gcc -v 2>&1|grep Target|cut -d\  -f2|cut -d\- -f1`

if [ -z $CURRENTARCH ]; then
  echo "/mingw is not mounted or gcc not installed"

elif [ $CURRENTARCH == "i686" ]; then
  echo "Current toolchain:" $CURRENTARCH
  echo "Switching to 64bit toolchain"
  sed -i 's/mingw32/mingw64/g' /etc/fstab
  echo -n "New toolchain:"; gcc -v 2>&1|grep Target|cut -d\  -f2
  
elif [ $CURRENTARCH == "x86_64" ]; then
  echo "Current toolchain:" $CURRENTARCH
  echo "Switching to 32bit toolchain"
  sed -i 's/mingw64/mingw32/g' /etc/fstab
  echo -n "New toolchain:"; gcc -v 2>&1|grep Target|cut -d\  -f2


fi

