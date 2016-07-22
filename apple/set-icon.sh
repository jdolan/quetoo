#!/bin/bash

# Usage: ./set-icon.sh file.icns path/to/[folder|file]

sips -i $1
DeRez -only icns $1 > /tmp/set-icon.resource

if [ -d "$2" ]; then
	Rez -append /tmp/set-icon.resource -o $2$'/Icon\r'
	SetFile -a C $2
	SetFile -a V $2$'/Icon\r'	
elif [ -f "$2" ]; then
	Rez -append /tmp/set-icon.resource -o $2
	SetFile -a C $2
else 
	echo "No such file or directory: $2" >&2
	exit 1
fi


