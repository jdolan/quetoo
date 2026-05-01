#!/bin/bash
# Docker entrypoint for quetoo-dedicated.
# Reads /etc/quetoo-dedicated/<instance>.cfg and passes each non-blank,
# non-comment line as a +command argument to the binary.
# On bare-metal installs the systemd unit calls quetoo-dedicated-start instead.

INSTANCE="${1:-default}"
CFG="/etc/quetoo-dedicated/${INSTANCE}.cfg"
BINARY="/usr/lib/quetoo/bin/quetoo-dedicated"

OPTS=()
if [ -f "${CFG}" ]; then
  while IFS= read -r line || [ -n "${line}" ]; do
    line="${line%%//*}"
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [ -z "${line}" ] && continue
    eval "OPTS+=(+${line})"
  done < "${CFG}"
fi

exec "${BINARY}" "${OPTS[@]}"
