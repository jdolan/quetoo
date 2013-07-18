#!/bin/bash

QUAKE2WORLD_HOME=$(dirname "$0")
QUAKE2WORLD_HOME=${QUAKE2WORLD_HOME/Update.app*/Quake2World.app}

test -w "${QUAKE2WORLD_HOME}" || {
	echo "${QUAKE2WORLD_HOME} is not writable" >&2
	exit 1
}

ARCH=$(uname -m)

echo
echo "Updating ${QUAKE2WORLD_HOME} for ${ARCH}.."
echo

APPLE=rsync://quake2world.net/quake2world-apple/${ARCH}/
rsync -rLzhP --delete "${APPLE}" ${QUAKE2WORLD_HOME}/Contents || exit 2

echo
echo "Update complete."
echo
