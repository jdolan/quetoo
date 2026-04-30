#!/bin/sh
# Docker entrypoint for quetoo-dedicated.
# Reads /etc/quetoo-dedicated/<instance>.cfg and passes each non-blank,
# non-comment line as a +command argument to the binary, exactly as the
# init.d script does.

INSTANCE="${1:-default}"
CFG="/etc/quetoo-dedicated/${INSTANCE}.cfg"
BINARY="/usr/lib/quetoo/bin/quetoo-dedicated"

set --
if [ -f "${CFG}" ]; then
  while IFS= read -r line || [ -n "${line}" ]; do
    line="${line%%//*}"  # strip // comments
    line="${line#"${line%%[! ]*}"}"  # ltrim
    line="${line%"${line##*[! ]}"}"  # rtrim
    [ -z "${line}" ] && continue
    set -- "$@" "+${line}"
  done < "${CFG}"
fi

exec "${BINARY}" "$@"
