#!/bin/bash

QUETOO_HOME=$(dirname "$0")
QUETOO_HOME=${QUETOO_HOME/Update.app*/Quetoo.app}

test -w "${QUETOO_HOME}" || {
	echo "${QUETOO_HOME} is not writable" >&2
	exit 1
}

ARCH=$(uname -m)

echo
echo "Updating ${QUETOO_HOME} for ${ARCH}.."
echo

APPLE=rsync://quetoo.org/quetoo-apple/${ARCH}/
rsync -rLzhP --delete "${APPLE}" ${QUETOO_HOME}/Contents || exit 2

echo
echo "Update complete."
echo
