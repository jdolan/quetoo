#!/bin/bash

# Usage: ./set-icon.sh file.icns path/to/folder

DeRez -only icns $1 > /tmp/set-icon.resource
Rez -append /tmp/set-icon.resource -o $2$'/Icon\r'
SetFile -a C $2
SetFile -a V $2$'/Icon\r'
