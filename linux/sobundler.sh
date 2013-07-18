#!/bin/bash
#
#
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

install $(ldd "${1}" | sed -rn 's:.* => (/usr/[^ ]+) .*:\1:p') "${dir}"

