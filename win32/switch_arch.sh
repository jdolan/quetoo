#!/bin/bash

if [ -z `mount |grep x86_64` ]; then
  sed -i 's/mingw-x86 /mingw-x86_64 /g' /etc/fstab
else
  sed -i 's/mingw-x86_64 /mingw-x86 /g' /etc/fstab
fi
	
