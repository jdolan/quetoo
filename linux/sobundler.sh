#!/bin/bash
#
# Scans executables for runtime dependencies and copies them to the desired
# output directory. This script relies on the output of `ldd`.
#

SKIP='
	libc.so*
	libdl.so*
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
# Returns 1 if the specified library should be skipped, 0 otherwise.
#
function skip_lib() {
	
	soname=$(basename "${1}")
	
	for skip in ${SKIP}; do
		if [[ ${soname} = ${skip} ]]; then
			return 1
		fi
	done

	return 0
}


libs=$(ldd $@ | grep ' => ' | sed -rn 's:.* => ([^ ]+) .*:\1:p' | sort -u)

for lib in $libs; do
	if skip_lib "${lib}"; then
		echo "Installing ${lib} in ${dir}.."
		install "${lib}" "${dir}"
	fi
done

