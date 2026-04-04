#!/usr/bin/env python3
"""Clean obvious floating point rounding errors in Quake .map files.

Snaps texture offsets, rotations, and scales to nearby "clean" values
when the difference is negligible (sub-pixel noise from float32 math).
Intentional values for angled surfaces are left untouched.
"""

import argparse
import re
import sys

# Regex to match brush face lines:
# ( x y z ) ( x y z ) ( x y z ) texture offset_x offset_y rotation scale_x scale_y [extras...]
FACE_RE = re.compile(
    r'^(\s*'
    r'\(\s*[-\d.e+]+\s+[-\d.e+]+\s+[-\d.e+]+\s*\)\s*'
    r'\(\s*[-\d.e+]+\s+[-\d.e+]+\s+[-\d.e+]+\s*\)\s*'
    r'\(\s*[-\d.e+]+\s+[-\d.e+]+\s+[-\d.e+]+\s*\)\s*'
    r'\S+\s+)'                # prefix: planes + texture name
    r'([-\d.e+]+)\s+'         # offset_x
    r'([-\d.e+]+)\s+'         # offset_y
    r'([-\d.e+]+)\s+'         # rotation
    r'([-\d.e+]+)\s+'         # scale_x
    r'([-\d.e+]+)'            # scale_y
    r'(.*?)$'                 # optional extras (flags, content, value)
)

# Progressively finer grids to snap to: (step_size, max_error)
SNAP_TARGETS = [
    (1.0,    0.005),
    (0.5,    0.003),
    (0.25,   0.001),
    (0.125,  0.0005),
    (0.1,    0.0005),
    (0.0625, 0.0003),
    (0.05,   0.0003),
    (0.025,  0.0001),
    (0.01,   0.0001),
    (0.005,  0.00005),
]


def format_value(val):
    """Format a numeric value concisely (no trailing zeros)."""
    if val == int(val):
        return str(int(val))
    return f"{val:.10f}".rstrip('0').rstrip('.')


def clean_value(val_str):
    """Snap a texture parameter to the nearest clean value, if close enough."""
    val = float(val_str)

    if abs(val) < 0.002:
        return "0"

    sign = -1 if val < 0 else 1
    absval = abs(val)

    for step, threshold in SNAP_TARGETS:
        nearest = round(absval / step) * step
        if abs(absval - nearest) < threshold:
            return format_value(nearest * sign)

    return val_str


def clean_map(path, dry_run=False):
    """Clean floating point rounding errors in a .map file."""
    with open(path, 'r') as f:
        lines = f.readlines()

    changes = 0
    lines_changed = 0
    output = []

    for line in lines:
        m = FACE_RE.match(line.rstrip('\n'))
        if m:
            prefix = m.group(1)
            params = [m.group(i) for i in range(2, 7)]
            suffix = m.group(7)

            new_params = []
            line_changed = False
            for p in params:
                cleaned = clean_value(p)
                if cleaned != p:
                    changes += 1
                    line_changed = True
                new_params.append(cleaned)

            if line_changed:
                lines_changed += 1

            output.append(prefix + ' '.join(new_params) + suffix + '\n')
        else:
            output.append(line)

    if not dry_run:
        with open(path, 'w') as f:
            f.writelines(output)

    return changes, lines_changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('maps', nargs='+', help='.map files to clean')
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help='report changes without writing')
    args = parser.parse_args()

    total_changes = 0
    total_lines = 0

    for path in args.maps:
        changes, lines_changed = clean_map(path, dry_run=args.dry_run)
        total_changes += changes
        total_lines += lines_changed
        if changes:
            print(f"{path}: cleaned {changes} values across {lines_changed} lines")

    if total_changes == 0:
        print("No rounding errors found.")
    elif args.dry_run:
        print(f"(dry run) Would clean {total_changes} values across {total_lines} lines")

    return 0


if __name__ == '__main__':
    sys.exit(main())
