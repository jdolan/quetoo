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

echo "$(ldd ${1})"

for dep in $(ldd "${1}" | sed -rn 's:.* => (/usr/[^ ]+) .*:\1:p'); do
	test -f "${dep}" || {
		echo "WARNING: Failed to resolve ${dep}" >2
		continue
	} 
	install "${dep}" "${dir}"
done

