#!/bin/bash
#
# Scans an executable for runtime dependencies and copies them to the desired
# output directory. This script relies on the output of `ldd`, and only copies
# dependencies from /usr/*.
#

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

exe="${1}"
test -x "${exe}" || {
	echo "${exe} is not an executable" >&2
	exit 2
}

test "${dir}" || dir=$(dirname "${exe}")

test -w "${dir}" || {
	echo "${dir} is not writable" >&2
	exit 3
}

echo
echo "Bundling .so files for ${exe} in ${dir}.."
echo

SKIP='
	libGL.so*
	libnsl.so*
	libresolv.so*
	librt.so*
	libuuid.so*
	libwrap.so*
	libX11.so*
	libXext.so*
'

nl=$'\n'

for dep in $(ldd "${1}" | sed -rn 's:.* => ([^ ]+) .*:\1:p' | sort); do
	soname=$(basename "${dep}")
	
	unset skip
	for s in ${SKIP}; do
		if [[ ${soname} = ${s} ]]; then
			skip=true
			skipped="${skipped}${nl} ${soname} @ ${dep}"
			break
		fi
	done
	
	test "${skip}" || {
		echo "Installing ${dep} in ${dir}.."
		install "${dep}" "${dir}"
	}
done

install `ldd /usr/bin/file | grep ld- | sed s:\(.*::` "${dir}/../bin"

echo
echo "The following libraries were intentionally skipped:${nl}${skipped}"
echo

