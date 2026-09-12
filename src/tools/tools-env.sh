#!/usr/bin/env bash
# Activates the src/tools Python virtual environment created by `make
# install-tools`, and stops Python from littering this directory with
# __pycache__ folders when the tools are run directly (e.g. `python3
# md3fu.py ...`) rather than via their installed console-script wrappers.
#
# Usage: source src/tools/tools-env.sh

if [ -n "${BASH_SOURCE:-}" ]; then
  _tools_env_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
elif [ -n "${ZSH_VERSION:-}" ]; then
  _tools_env_dir="$(cd "$(dirname "${(%):-%x}")" && pwd)"
else
  echo "tools-env.sh: unsupported shell; activate .venv/bin/activate manually" >&2
  _tools_env_dir=""
fi

if [ -n "$_tools_env_dir" ]; then
  export PYTHONDONTWRITEBYTECODE=1
  # shellcheck disable=SC1091
  . "$_tools_env_dir/.venv/bin/activate"
fi

unset _tools_env_dir
