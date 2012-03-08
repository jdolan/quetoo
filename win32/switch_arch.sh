#!/bin/bash

if [ -z `mount |grep 64` ]; then
  sed -i 's/mingw32 /mingw64 /g' /etc/fstab
else
  sed -i 's/mingw64 /mingw32 /g' /etc/fstab
fi
	
