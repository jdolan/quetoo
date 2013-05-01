#!/bin/bash

# Usage: ./set-icon.sh file.icns path/to/folder
# Be sure that file.icns actually has the icon resource set!

DeRez -only icns $1 > /tmp/set-icon.resource
Rez -append /tmp/set-icon.resource -o $2$'/Icon\r'
SetFile -a C $2
SetFile -a V $2$'/Icon\r'
