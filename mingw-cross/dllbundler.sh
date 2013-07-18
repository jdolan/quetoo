#!/bin/bash -e
#
# Prints the runtime dependencies of the specified program.
#

while getopts "h:" opt; do
	case "${opt}" in
		h)
			host="${OPTARG}"
			;;
		\?)
			echo "Invalid option: -${OPTARG}" >&2
			exit 1
			;;
	esac
done

test "${host}" || {
	echo "Required option -h host is missing" >&2
	exit 1
}

objdump=$(which ${host}-objdump)
test -x "${objdump}" || {
	echo "No ${host}-objdump in PATH" >&2
	exit 2
}

exe="${2}"
test -x "${exe}" || {
	echo "${exe} is not an executable" >&2
	exit 3
}

dir=$(dirname "${exe}")
test -w "${dir}" || {
	echo "${dir} is not writable" >&2
	exit 3
}

search_path="${MINGW_PREFIX}/usr/${host}"
test -d "${search_path}" || {
	echo "${search_path} does not exist" >&2
	exit 4
}

echo "Bundling .dll files for ${exe} in ${dir}.."

#
# Resolve dependencies recursively, copying them from the search path to dir.
#
function bundle_recursively(){
	local deps=$($objdump -p "${1}" | sed -rn 's/DLL Name: (.*\.dll)/\1/p' | sort -u)
	for dep in ${deps}; do
		test -f "${dir}/${dep}" && continue

		local dll=$(find "${search_path}" -name "${dep}")
		test -z "${dll}" && {
			echo "WARNING: Couldn't find ${dep} in ${search_path}" >&2
			continue
		}

		bundle_recursively "${dll}"

		echo "Installing ${dll}.."
		install "${dll}" "${dir}"
	done
}

bundle_recursively "${exe}"
