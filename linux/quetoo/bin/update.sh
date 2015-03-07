#!/bin/bash

SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd -P)"
QUETOO="${SCRIPT_PATH%/bin*}"

test -w "${QUETOO}" || {
	echo "${QUETOO} is not writable" >&2
	exit 1
}

ARCH=$(uname -m)

echo
echo "Updating ${QUETOO} for ${ARCH}.."
echo

LINUX=rsync://quetoo.org/quetoo-linux/${ARCH}/
rsync -rLzhP --delete "${LINUX}" "${QUETOO}" || exit 2

# Ensure that the .desktop entries are in working order
sed -i "s:%QUETOO%:${QUETOO}:g" \
	"${QUETOO}/Quetoo.desktop" \
	"${QUETOO}/Quetoo Update.desktop"

echo
echo "Update complete."
echo

