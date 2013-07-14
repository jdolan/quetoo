#!/bin/bash -x
#
# Prints the runtime dependencies of the specified program.
#

prog="$1"
arch="$2"

#
# Recursively finds and prints the .dll names required by the specified object.
#
function find_deps(){	
	for i in $(/usr/bin/${arch}-w64-mingw32-objdump -p "$1" | grep "DLL Name:" | cut -d\: -f2 | cut -d\ -f2 | uniq); do
		dll=$(find /usr/${arch}-w64-mingw32 2>/dev/null | grep "$i" | grep -v .dll.a)
		echo $dll
		find_deps $dll
	done | uniq
}

find_deps "$prog"