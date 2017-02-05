#!/bin/bash
#
# Scans executables for runtime dependencies and copies them to the desired
# output directory. This script relies on the output of `ldd`.
#

SKIP='
	libc.so*
	libdl.so*
	libdrm.so*
	libGL.so*
	libm.so*
	libpthread.so*
	libresolv.so*
	librt.so*
	libstdc++.so*
'

while getopts "d:" opt; do
	case "${opt}" in
		d)
			dir="${OPTARG}"
			;;
		\?)
			echo "Invalid option: -${OPTARG}" >&2
			exit 1
			;;
	esac
done

shift $((OPTIND-1))

test "${dir}" || dir=$(dirname "${1}")

#
# Returns 0 if the specified library should be skipped, 1 otherwise.
#
function skip_lib() {
	
	soname=$(basename "${1}")
	
	for skip in ${SKIP}; do
		if [[ ${soname} = ${skip} ]]; then
			return 0
		fi
	done

	return 1
}

libs=$(ldd $@ | grep ' => ' | sed -rn 's:.* => ([^ ]+) .*:\1:p' | sort -u)

skipped=""

for lib in ${libs}; do
	if skip_lib "${lib}"; then
		skipped="${skipped} ${lib}"
		continue
	fi
	
	echo "Installing ${lib} in ${dir}.."
	install "${lib}" "${dir}"
done

echo
echo "The following libraries were skipped:"

for skip in ${skipped}; do
	echo " ${skip}"
done

echo

for exe in $@; do
	echo "Rewriting RPATH for ${exe}.."
	chrpath -r '$ORIGIN/../lib' ${exe}
done
