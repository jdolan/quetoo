#!/bin/bash

SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd -P)"
QUAKE2WORLD_HOME="${SCRIPT_PATH%/bin*}"

test -w "${QUAKE2WORLD_HOME}" || {
	echo "${QUAKE2WORLD_HOME} is not writable" >&2
	exit 1
}

ARCH=$(uname -m)

echo
echo "Updating ${QUAKE2WORLD_HOME} for ${ARCH}.."
echo

LINUX=rsync://quake2world.net/quake2world-linux/${ARCH}/
rsync -rLzhP --delete "${LINUX}" "${QUAKE2WORLD_HOME}" || exit 2

# Ensure that the .desktop entries are in working order
sed -i "s:%QUAKE2WORLD_HOME%:${QUAKE2WORLD_HOME}:g" \
	"${QUAKE2WORLD_HOME}/Quake2World.desktop" \
	"${QUAKE2WORLD_HOME}/Quake2World Update.desktop"

echo
echo "Update complete."
echo

