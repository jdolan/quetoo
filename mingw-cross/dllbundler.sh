#!/bin/bash
#
# Prints the runtime dependencies of the specified program.
#

objdump=$(which ${MINGW_HOST}-objdump)
test -x "${objdump}" || {
	echo "No ${MINGW_HOST}-objdump in PATH" >&2
	exit 1
}

exe="${1}"
test -x "${exe}" || {
	echo "${exe} is not an executable" >&2
	exit 2
}

dir=$(dirname "${1}")
test -w "${dir}" || {
	echo "${dir} is not writable" >&2
	exit 3
}

search_path="${MINGW_PREFIX}/usr/${MINGW_HOST}/lib"
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

		bundle_recursively "$dll"
		install "${dll}" "${dir}"
	done
}

bundle_recursively "$1"
