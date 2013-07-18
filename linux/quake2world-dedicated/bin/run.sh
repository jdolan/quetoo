#!/bin/bash

SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd -P)"
QUAKE2WORLD_HOME="${SCRIPT_PATH%/bin*}"

export LD_LIBRARY_PATH="${QUAKE2WORLD_HOME}/lib;${LD_LIBRARY_PATH}"

${QUAKE2WORLD_HOME}/bin/quake2world $@

