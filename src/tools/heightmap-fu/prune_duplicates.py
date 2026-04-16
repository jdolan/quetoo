#!/usr/bin/env python3
"""Prune exact duplicate textures from quetoo-data.

For each duplicate group, selects a canonical copy and removes the rest,
updating any .map file references to point to the canonical version.

Usage:
    python prune_duplicates.py <textures_dir> <maps_dir> [--execute]

Without --execute, prints a dry-run plan. With --execute, performs deletions
and map rewrites.
"""

import argparse
import json
import os
import re
import glob
import shutil
import sys
from collections import defaultdict


def load_report(report_path):
    with open(report_path) as f:
        return json.load(f)


def scan_map_references(maps_dir, texture_bases):
    """Scan all .map files for brush face texture references.
    
    Returns dict: texture_base -> {map_file: [line_numbers]}
    """
    refs = defaultdict(lambda: defaultdict(list))
    tex_set = set(texture_bases)
    
    for mf in sorted(glob.glob(os.path.join(maps_dir, "*.map"))):
        basename = os.path.basename(mf)
        with open(mf) as fh:
            for line_no, line in enumerate(fh, 1):
                stripped = line.strip()
                if not stripped.startswith('('):
                    continue
                parts = stripped.split(')')
                if len(parts) >= 4:
                    tex_part = parts[3].strip().split()
                    if tex_part and tex_part[0] in tex_set:
                        refs[tex_part[0]][basename].append(line_no)
    
    return refs


def find_associated_files(tex_dir, tex_path):
    """Find .mat, normalmap, heightmap, specularmap files associated with a texture."""
    base = tex_path.rsplit('.', 1)[0]
    ext = tex_path.rsplit('.', 1)[1] if '.' in tex_path else 'tga'
    
    associated = []
    
    # .mat file
    mat_path = os.path.join(tex_dir, base + '.mat')
    if os.path.exists(mat_path):
        associated.append(mat_path)
    
    # Normal suffixes
    stem = base.rsplit('/', 1)[-1] if '/' in base else base
    directory = os.path.join(tex_dir, base.rsplit('/', 1)[0]) if '/' in base else tex_dir
    
    for suffix in ['_norm']:
        nm_path = os.path.join(tex_dir, base + suffix + '.' + ext)
        if os.path.exists(nm_path):
            associated.append(nm_path)
    
    # Specularmap
    for suffix in ['_spec']:
        path = os.path.join(tex_dir, base + suffix + '.' + ext)
        if os.path.exists(path):
            associated.append(path)
    
    return associated


def score_candidate(file_info, tex_dir, map_refs):
    """Score a texture candidate for being the canonical version.
    
    Higher score = more likely canonical.
    """
    path = file_info['path']
    base = path.rsplit('.', 1)[0]
    score = 0
    
    # Has filesystem normalmap
    if file_info.get('normalmap'):
        score += 100
    
    # Has .mat normalmap
    if file_info.get('mat_normalmap'):
        score += 80
    
    # Has .mat file
    mat_path = os.path.join(tex_dir, base + '.mat')
    if os.path.exists(mat_path):
        with open(mat_path) as f:
            mat_content = f.read()
        # Score by richness of material definition
        for key in ['roughness', 'hardness', 'specularity', 'parallax', 'shadow']:
            if key in mat_content:
                score += 10
        for key in ['normalmap', 'heightmap', 'specularmap']:
            if key in mat_content:
                score += 20
    
    # Used in maps (strong signal)
    if base in map_refs:
        score += 200 * len(map_refs[base])
    
    # Prefer shorter/base names — penalize suffixed variants
    name = base.rsplit('/', 1)[-1] if '/' in base else base
    if re.search(r'_\d+$', name):
        score -= 500  # _1, _2 editor artifacts — strongly penalize
    if name.endswith('_'):
        score -= 500
    
    # Tiebreak: shorter names preferred
    score -= len(name) * 0.1
    
    return score


def select_canonical(group, tex_dir, map_refs):
    """Select the canonical file from a duplicate group."""
    files = group['files']
    scored = [(score_candidate(f, tex_dir, map_refs), i, f) for i, f in enumerate(files)]
    scored.sort(key=lambda x: (-x[0], x[1]))
    return scored[0][2], [s[2] for s in scored[1:]]


def adopt_mat(canonical_base, donor_base, tex_dir, dry_run=True):
    """Adopt material properties from a richer .mat file into the canonical.
    
    If the donor .mat has more material directives (normalmap, heightmap, etc.)
    than the canonical .mat, replace the canonical .mat with the donor content,
    updating the diffusemap line to reference the canonical name.
    
    Returns description of what was done, or None.
    """
    canonical_mat = os.path.join(tex_dir, canonical_base + '.mat')
    donor_mat = os.path.join(tex_dir, donor_base + '.mat')
    
    if not os.path.exists(donor_mat):
        return None
    
    with open(donor_mat) as f:
        donor_content = f.read()
    
    # Count material richness
    rich_keys = ['normalmap', 'heightmap', 'specularmap', 'roughness', 'hardness',
                 'specularity', 'parallax', 'shadow']
    donor_richness = sum(1 for k in rich_keys if k in donor_content)
    
    canonical_richness = 0
    if os.path.exists(canonical_mat):
        with open(canonical_mat) as f:
            canonical_content = f.read()
        canonical_richness = sum(1 for k in rich_keys if k in canonical_content)
    
    if donor_richness <= canonical_richness:
        return None
    
    # Replace diffusemap directive to point to canonical
    new_content = re.sub(
        r'(diffusemap\s+)\S+',
        rf'\g<1>{canonical_base}',
        donor_content,
        count=1
    )
    
    if not dry_run:
        with open(canonical_mat, 'w') as f:
            f.write(new_content)
    
    return f"Adopted .mat from {donor_base} → {canonical_base} ({donor_richness} vs {canonical_richness} directives)"


def should_skip_group(group, tex_dir, map_refs):
    """Determine if a group should be skipped (different material purposes)."""
    files = group['files']
    bases = [f['path'].rsplit('.', 1)[0] for f in files]
    
    # Check for fundamentally different .mat properties
    mat_contents = {}
    for base in bases:
        mat_path = os.path.join(tex_dir, base + '.mat')
        if os.path.exists(mat_path):
            with open(mat_path) as f:
                mat_contents[base] = f.read().lower()
    
    if len(mat_contents) >= 2:
        special_keys = ['surface', 'contents', 'warp', 'scroll.', 'flare', 'envmap',
                        '"sky"', 'sky\n', '"liquid', '"water', '"lava', '"blend']
        for base, content in mat_contents.items():
            for key in special_keys:
                if key in content:
                    others_have = sum(1 for b, c in mat_contents.items() if b != base and key in c)
                    if others_have < len(mat_contents) - 1:
                        return True, f"Different material purposes ({key} in {base})"
    
    # Check if both copies are used in DIFFERENT maps with both having rich .mat files
    # This suggests intentional duplication for different texture sets
    used_files = [f for f in files if f['path'].rsplit('.', 1)[0] in map_refs]
    if len(used_files) >= 2:
        dirs = set(f['path'].split('/')[0] for f in used_files)
        if len(dirs) >= 2:
            # Cross-set duplicates both actively used — skip
            return True, "Cross-set duplicates both used in maps"
    
    return False, ""


def check_normalmap_shared(nm_path, tex_dir, canonical_base):
    """Check if a normalmap file is referenced by other textures (via .mat files)."""
    if not os.path.exists(nm_path):
        return False
    
    nm_rel = os.path.relpath(nm_path, tex_dir)
    nm_base = nm_rel.rsplit('.', 1)[0]
    
    # Search all .mat files for references to this normalmap
    for mat_file in glob.glob(os.path.join(tex_dir, '**/*.mat'), recursive=True):
        mat_rel = os.path.relpath(mat_file, tex_dir).rsplit('.', 1)[0]
        if mat_rel == canonical_base:
            continue  # Skip the canonical's own .mat
        with open(mat_file) as f:
            content = f.read()
        if nm_base in content:
            return True
    
    return False


def rewrite_map_file(map_path, old_tex, new_tex, dry_run=True):
    """Rewrite texture references in a .map file."""
    with open(map_path) as f:
        content = f.read()
    
    # Replace on brush face lines: old_tex followed by space and numbers
    # Pattern: ") old_tex " -> ") new_tex "
    old_content = content
    content = content.replace(f') {old_tex} ', f') {new_tex} ')
    
    changes = old_content.count(f') {old_tex} ') 
    
    if not dry_run and changes > 0:
        with open(map_path, 'w') as f:
            f.write(content)
    
    return changes


def main():
    parser = argparse.ArgumentParser(description='Prune exact duplicate textures')
    parser.add_argument('textures_dir', help='Path to textures directory')
    parser.add_argument('maps_dir', help='Path to maps directory')
    parser.add_argument('--report', default=None, help='Path to dedupe_report.json')
    parser.add_argument('--execute', action='store_true', help='Actually perform deletions')
    args = parser.parse_args()
    
    tex_dir = os.path.abspath(args.textures_dir)
    maps_dir = os.path.abspath(args.maps_dir)
    
    # Find report
    report_path = args.report
    if not report_path:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        report_path = os.path.join(script_dir, 'dedupe_report.json')
    
    if not os.path.exists(report_path):
        print(f"Error: Report not found: {report_path}")
        sys.exit(1)
    
    data = load_report(report_path)
    exact = data['exact_duplicates']
    
    print(f"Loaded {len(exact)} exact duplicate groups from {report_path}")
    print(f"Textures: {tex_dir}")
    print(f"Maps: {maps_dir}")
    print(f"Mode: {'EXECUTE' if args.execute else 'DRY RUN'}\n")
    
    # Build index of all texture bases
    all_bases = set()
    for g in exact:
        for f in g['files']:
            all_bases.add(f['path'].rsplit('.', 1)[0])
    
    # Scan map references
    print("Scanning map files for texture references...")
    map_refs = scan_map_references(maps_dir, all_bases)
    print(f"Found {len(map_refs)} duplicate textures referenced in maps\n")
    
    # Process groups
    files_to_delete = []
    maps_to_rewrite = []  # (map_path, old_tex, new_tex)
    skipped_groups = []
    processed_groups = []
    
    for g in exact:
        skip, reason = should_skip_group(g, tex_dir, map_refs)
        if skip:
            skipped_groups.append((g, reason))
            continue
        
        canonical, removable = select_canonical(g, tex_dir, map_refs)
        canonical_base = canonical['path'].rsplit('.', 1)[0]
        
        group_actions = {
            'canonical': canonical['path'],
            'remove': [],
            'remap': [],
            'adopt_mat': None,
        }
        
        for rem in removable:
            rem_base = rem['path'].rsplit('.', 1)[0]
            rem_full = os.path.join(tex_dir, rem['path'])
            
            # Check if we should adopt the removable's .mat
            adopt_desc = adopt_mat(canonical_base, rem_base, tex_dir, dry_run=True)
            if adopt_desc:
                group_actions['adopt_mat'] = (canonical_base, rem_base, adopt_desc)
            
            # Queue the texture file for deletion
            if os.path.exists(rem_full):
                files_to_delete.append(rem_full)
                group_actions['remove'].append(rem['path'])
            
            # Queue associated files (.mat, etc) for deletion
            for assoc in find_associated_files(tex_dir, rem['path']):
                if os.path.exists(assoc):
                    # Don't delete normalmaps shared by other textures
                    if '_norm' in assoc:
                        if check_normalmap_shared(assoc, tex_dir, rem_base):
                            continue
                    files_to_delete.append(assoc)
            
            # Queue map remappings
            if rem_base in map_refs:
                for map_file, line_nums in map_refs[rem_base].items():
                    map_path = os.path.join(maps_dir, map_file)
                    maps_to_rewrite.append((map_path, rem_base, canonical_base))
                    group_actions['remap'].append(
                        f"{map_file}: {rem_base} → {canonical_base} ({len(line_nums)} faces)"
                    )
        
        processed_groups.append(group_actions)
    
    # Print plan
    print("=" * 72)
    print("PRUNING PLAN")
    print("=" * 72)
    
    print(f"\n--- SKIPPED ({len(skipped_groups)} groups) ---\n")
    for g, reason in skipped_groups:
        paths = [f['path'] for f in g['files']]
        print(f"  {paths[0]} + {len(paths)-1} others: {reason}")
    
    print(f"\n--- WILL PROCESS ({len(processed_groups)} groups) ---\n")
    total_remaps = 0
    total_adopts = 0
    for ga in processed_groups:
        print(f"  Keep: {ga['canonical']}")
        if ga['adopt_mat']:
            print(f"    Adopt .mat: {ga['adopt_mat'][2]}")
            total_adopts += 1
        for r in ga['remove']:
            print(f"    Delete: {r}")
        for remap in ga['remap']:
            print(f"    Remap: {remap}")
            total_remaps += 1
        print()
    
    # Deduplicate file list
    files_to_delete = sorted(set(files_to_delete))
    
    print("=" * 72)
    print("SUMMARY")
    print("=" * 72)
    print(f"  Groups processed: {len(processed_groups)}")
    print(f"  Groups skipped:   {len(skipped_groups)}")
    print(f"  Files to delete:  {len(files_to_delete)}")
    print(f"  Mat adoptions:    {total_adopts}")
    print(f"  Map remappings:   {total_remaps}")
    print()
    
    if not args.execute:
        print("This is a DRY RUN. Pass --execute to perform these changes.")
        
        # Print full file deletion list
        print(f"\nFiles to delete ({len(files_to_delete)}):")
        for f in files_to_delete:
            rel = os.path.relpath(f, tex_dir)
            print(f"  {rel}")
        
        return
    
    # Execute
    print("EXECUTING...\n")
    
    # Adopt .mat files first (before deleting donors)
    adopt_count = 0
    for ga in processed_groups:
        if ga['adopt_mat']:
            canonical_base, donor_base, desc = ga['adopt_mat']
            adopt_mat(canonical_base, donor_base, tex_dir, dry_run=False)
            print(f"  {desc}")
            adopt_count += 1
    
    # Rewrite maps (before deleting textures)
    rewrite_count = 0
    seen_rewrites = set()
    for map_path, old_tex, new_tex in maps_to_rewrite:
        key = (map_path, old_tex, new_tex)
        if key in seen_rewrites:
            continue
        seen_rewrites.add(key)
        
        changes = rewrite_map_file(map_path, old_tex, new_tex, dry_run=False)
        if changes > 0:
            print(f"  Rewrote {os.path.basename(map_path)}: {old_tex} → {new_tex} ({changes} faces)")
            rewrite_count += changes
    
    # Delete files
    delete_count = 0
    for f in files_to_delete:
        if os.path.exists(f):
            os.remove(f)
            rel = os.path.relpath(f, tex_dir)
            print(f"  Deleted: {rel}")
            delete_count += 1
    
    print(f"\nDone: {delete_count} files deleted, {adopt_count} .mat adoptions, {rewrite_count} brush faces remapped.")


if __name__ == '__main__':
    main()
