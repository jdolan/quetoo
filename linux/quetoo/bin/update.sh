#!/bin/bash

SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd -P)"
QUETOO_HOME="${SCRIPT_PATH%/bin*}"

test -w "${QUETOO_HOME}" || {
	echo "${QUETOO_HOME} is not writable" >&2
	exit 1
}

ARCH=$(uname -m)

echo
echo "Updating $QUETOO_HOME for ${ARCH}.."
echo

LINUX=rsync://quetoo.org/quetoo-linux/${ARCH}/
rsync -rLzhP --delete "${LINUX}" "${QUETOO_HOME}" || exit 2

# Ensure that the .desktop entries are in working order
sed -i "s:%QUETOO_HOME%:${QUETOO_HOME}:g" \
	"${QUETOO_HOME}/Quetoo.desktop" \
	"${QUETOO_HOME}/Quetoo Update.desktop"

chmod +x "${QUETOO_HOME}/Quetoo.desktop" \
	"${QUETOO_HOME}/Quetoo Update.desktop"

echo
echo "Update complete."
echo

