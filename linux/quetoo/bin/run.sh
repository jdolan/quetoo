#!/bin/bash

SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd -P)"
QUETOO_HOME="${SCRIPT_PATH%/bin*}"

export LD_LIBRARY_PATH="${QUETOO_HOME}/lib:${LD_LIBRARY_PATH}"

${QUETOO_HOME}/bin/quetoo $@

