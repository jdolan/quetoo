#!/bin/bash
#
# Scans executables and shared libraries for runtime dependencies and copies
# them to the specified output directory. Uses patchelf to rewrite RPATHs.
#

set -e

# Libraries that must NOT be bundled. These are either GPU driver interfaces,
# kernel ABIs, or glibc components that cannot safely be substituted.
SKIP='
  ld-linux
  libc.so
  libm.so
  libdl.so
  libpthread.so
  librt.so
  libresolv.so
  libutil.so
  libgcc_s.so
  libstdc++.so
  libGL.so
  libEGL.so
  libGLdispatch.so
  libGLX.so
  libdrm.so
  libgbm.so
  libvulkan.so
'

while getopts "d:" opt; do
  case "${opt}" in
    d)
      dir="${OPTARG}"
      ;;
    *)
      echo "Usage: $0 -d <libdir> <executables...>" >&2
      exit 1
      ;;
  esac
done

shift $((OPTIND - 1))

test "${dir}" || dir=$(dirname "${1}")

skip_lib() {
  local soname
  soname=$(basename "${1}")
  for skip in ${SKIP}; do
    case "${soname}" in
      ${skip}*) return 0 ;;
    esac
  done
  return 1
}

libs=$(ldd "$@" 2>/dev/null | grep ' => ' | sed -rn 's:.* => ([^ ]+) .*:\1:p' | sort -u)

echo "Bundling libraries into ${dir}..."

for lib in ${libs}; do
  if skip_lib "${lib}"; then
    echo "  Skipping $(basename "${lib}")"
    continue
  fi

  real_lib=$(realpath "${lib}")
  real_name=$(basename "${real_lib}")
  soname=$(basename "${lib}")

  echo "  Bundling ${real_name}"
  cp -f "${real_lib}" "${dir}/${real_name}"

  if [ "${soname}" != "${real_name}" ]; then
    ln -sf "${real_name}" "${dir}/${soname}"
  fi
done

echo
echo "Fixing RPATHs..."

for exe in "$@"; do
  echo "  ${exe}"
  patchelf --set-rpath '$ORIGIN/../lib' "${exe}"
done
