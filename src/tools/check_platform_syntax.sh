#!/bin/sh
# Syntax-checks platform-specific code paths that the host compiler skips.
#
# The installer's update logic branches heavily on the host, so most of the
# Windows path never reaches a compiler on a macOS or Linux workstation, and a
# use-before-declaration or an unbalanced brace in it only surfaces in CI. The
# blocks involved use SDL and stdio rather than Win32 directly, so forcing them
# on locally is enough to parse them.
#
# Usage: src/tools/check_platform_syntax.sh [_WIN32|__APPLE__|__linux__]

set -e

top=$(cd "$(dirname "$0")/../.." && pwd)
want=${1:-_WIN32}
status=0

flags=$(pkg-config --cflags sdl3 Objectively physfs 2>/dev/null || true)

script=$(mktemp)
cat > "$script" <<SED
s/#if defined($want)/#if 1/
s/#if !defined($want)/#if 0/
SED

for other in _WIN32 __APPLE__ __linux__; do
	if [ "$other" != "$want" ]; then
		echo "s/#if defined($other)/#if 0/" >> "$script"
		echo "s/#if !defined($other)/#if 1/" >> "$script"
	fi
done

for source in "$top"/src/common/installer.c "$top"/src/common/archive.c; do

	scratch="$(dirname "$source")/.platform-syntax-$(basename "$source")"
	errors=$(mktemp)

	sed -f "$script" "$source" > "$scratch"

	printf '%-14s %-10s ... ' "$(basename "$source")" "$want"

	if cc -fsyntax-only -I"$top" -I"$top/src" $flags "$scratch" 2> "$errors"; then
		echo "ok"
	else
		echo "FAILED"
		sed "s|$scratch|$source|g" "$errors" | sed 's/^/  /' | head -20
		status=1
	fi

	rm -f "$scratch" "$errors"
done

rm -f "$script"
exit $status
