#!/usr/bin/env python3
"""Interactive GUI for reviewing color variant normalmap consolidation.

For each group of color-variant textures that have independently authored
normalmaps, shows the diffuse + normalmap pairs side by side and lets the
user choose:
  - MERGE: full duplicate — pick one canonical texture, delete others,
           remap .map references (diffuse + normalmap are both dupes)
  - CONSOLIDATE: pick a canonical normalmap, update .mat files for others
           (diffuses are different color variants sharing geometry)
  - SKIP: leave this group alone (intentionally different normalmaps)

Usage:
    python variant_review.py <textures_dir> <maps_dir> [--execute]

Example:
    python variant_review.py \\
        ~/Coding/quetoo-data/target/default/textures \\
        ~/Coding/quetoo-data/target/default/maps
"""

import argparse
import glob
import hashlib
import json
import os
import re
import sys
import tkinter as tk
from tkinter import ttk

import numpy as np
from PIL import Image, ImageTk

NM_SUFFIXES = ("_norm",)


def is_shorthand_ref(nm_name):
    base = nm_name.split("/")[-1]
    if base.endswith(".tga"):
        base = base[:-4]
    return not any(base.endswith(s) for s in NM_SUFFIXES)


def load_image(textures_dir, ref):
    """Load an image, trying with and without .tga extension."""
    for path in [os.path.join(textures_dir, ref),
                 os.path.join(textures_dir, ref + ".tga")]:
        if os.path.exists(path):
            return Image.open(path).convert("RGB"), path
    return None, None


def img_hash(img):
    return hashlib.sha256(np.array(img).tobytes()).hexdigest()[:16]


def nm_similarity(img1, img2):
    a1 = np.array(img1).astype(np.float32) / 255.0
    a2 = np.array(img2).astype(np.float32) / 255.0
    if a1.shape != a2.shape:
        return 0.0
    return float(1.0 - np.mean(np.abs(a1 - a2)))


def scan_map_references(maps_dir):
    """Scan all .map files and return {texture_name: [map_file, ...]}."""
    refs = {}
    if not maps_dir or not os.path.isdir(maps_dir):
        return refs
    for map_file in glob.glob(os.path.join(maps_dir, "*.map")):
        with open(map_file) as f:
            content = f.read()
        # Brush face format: ) texture_name ...
        for m in re.finditer(r'\)\s+(\S+)\s+[-\d]', content):
            tex = m.group(1)
            if tex not in refs:
                refs[tex] = set()
            refs[tex].add(os.path.basename(map_file))
    return {k: sorted(v) for k, v in refs.items()}


def build_review_list(textures_dir, report_path):
    """Scan the dedupe report and build a list of groups needing review."""
    with open(report_path) as f:
        data = json.load(f)

    variants = data["color_variants"]
    review = []

    for i, g in enumerate(variants):
        files = g["files"]
        existing = [f for f in files
                    if os.path.exists(os.path.join(textures_dir, f["path"]))]
        if len(existing) < 2:
            continue

        # Collect real normalmap files (not shorthand .mat refs)
        nm_info = {}
        for f in existing:
            nm = f.get("normalmap") or f.get("mat_normalmap")
            if not nm or is_shorthand_ref(nm):
                continue
            if nm not in nm_info:
                img, path = load_image(textures_dir, nm)
                if img:
                    nm_info[nm] = {
                        "img": img, "path": path,
                        "hash": img_hash(img), "users": [],
                    }
            if nm in nm_info:
                nm_info[nm]["users"].append(f["path"])

        # Dedupe by pixel hash
        seen = {}
        unique = {}
        for nm_name, info in nm_info.items():
            if info["hash"] not in seen:
                seen[info["hash"]] = nm_name
                unique[nm_name] = info
            else:
                unique[seen[info["hash"]]]["users"].extend(info["users"])

        if len(unique) < 2:
            continue

        # Pairwise similarity
        names = list(unique.keys())
        sims = []
        for j in range(len(names)):
            for k in range(j + 1, len(names)):
                s = nm_similarity(unique[names[j]]["img"],
                                  unique[names[k]]["img"])
                sims.append(s)
        avg_sim = float(np.mean(sims)) if sims else 0.0

        # Compute diffuse hashes to detect full duplicates
        diffuse_hashes = {}
        for f in existing:
            d_img, _ = load_image(textures_dir, f["path"])
            if d_img:
                dh = img_hash(d_img)
                if dh not in diffuse_hashes:
                    diffuse_hashes[dh] = []
                diffuse_hashes[dh].append(f["path"])

        has_dupe_diffuse = any(len(v) > 1 for v in diffuse_hashes.values())

        review.append({
            "group": i + 1,
            "avg_sim": avg_sim,
            "normalmaps": unique,  # nm_name -> {img, path, hash, users}
            "files": existing,
            "diffuse_hashes": diffuse_hashes,
            "has_dupe_diffuse": has_dupe_diffuse,
        })

    review.sort(key=lambda x: x["avg_sim"], reverse=True)
    return review


class VariantReviewApp:
    THUMB = 192

    SAVE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "variant_decisions.json")

    def __init__(self, root, textures_dir, maps_dir, review_list, map_refs):
        self.root = root
        self.textures_dir = textures_dir
        self.maps_dir = maps_dir
        self.review = review_list
        self.map_refs = map_refs
        self.current = 0
        self.decisions = {}  # group_num -> {action, ...}
        self.removed = {}    # group_num -> set of removed texture paths
        self.photo_refs = []

        self._load_decisions()
        # Advance to first unreviewed group
        for i, r in enumerate(self.review):
            if r["group"] not in self.decisions:
                self.current = i
                break

        self.root.title("Color Variant Review")

        # Top bar: navigation and summary
        top = ttk.Frame(root, padding=5)
        top.pack(fill=tk.X)

        self.lbl_progress = ttk.Label(top, text="", font=("Helvetica", 12))
        self.lbl_progress.pack(side=tk.LEFT)

        ttk.Button(top, text="Export Actions", command=self.export).pack(
            side=tk.RIGHT, padx=5)
        ttk.Button(top, text="▶ Next", command=self.next_group).pack(
            side=tk.RIGHT, padx=2)
        ttk.Button(top, text="◀ Prev", command=self.prev_group).pack(
            side=tk.RIGHT, padx=2)

        # Bind keyboard
        root.bind("<Right>", lambda e: self.next_group())
        root.bind("<Left>", lambda e: self.prev_group())
        root.bind("<s>", lambda e: self.decide_skip())
        root.bind("<c>", lambda e: self.enter_consolidate_mode())

        # Info bar
        info = ttk.Frame(root, padding=5)
        info.pack(fill=tk.X)
        self.lbl_info = ttk.Label(info, text="", font=("Helvetica", 11))
        self.lbl_info.pack(side=tk.LEFT)
        self.lbl_decision = ttk.Label(
            info, text="", font=("Helvetica", 11, "bold"))
        self.lbl_decision.pack(side=tk.RIGHT)

        # Scrollable content area
        container = ttk.Frame(root)
        container.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(container, bg="#333333")
        scrollbar = ttk.Scrollbar(container, orient=tk.VERTICAL,
                                  command=self.canvas.yview)
        self.scroll_frame = ttk.Frame(self.canvas)
        self.scroll_frame.bind(
            "<Configure>",
            lambda e: self.canvas.configure(
                scrollregion=self.canvas.bbox("all")))
        self.canvas.create_window((0, 0), window=self.scroll_frame,
                                  anchor="nw")
        self.canvas.configure(yscrollcommand=scrollbar.set)

        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Mouse wheel scrolling
        self.canvas.bind_all(
            "<MouseWheel>",
            lambda e: self.canvas.yview_scroll(-e.delta, "units"))

        # Bottom action bar
        actions = ttk.Frame(root, padding=5)
        actions.pack(fill=tk.X)

        ttk.Button(
            actions, text="SKIP (s)",
            command=self.decide_skip).pack(side=tk.LEFT, padx=5)

        ttk.Separator(actions, orient=tk.VERTICAL).pack(
            side=tk.LEFT, fill=tk.Y, padx=5)

        self.lbl_hint = ttk.Label(
            actions,
            text="MERGE → click a diffuse to keep  |  "
                 "CONSOLIDATE (c) → click a normalmap to share  |  "
                 "SKIP (s) → leave as-is",
            font=("Helvetica", 10))
        self.lbl_hint.pack(side=tk.LEFT, padx=5)

        self.show_group()

    def show_group(self):
        self.photo_refs.clear()
        for w in self.scroll_frame.winfo_children():
            w.destroy()

        if self.current >= len(self.review):
            self.lbl_progress.config(text="All groups reviewed!")
            return

        r = self.review[self.current]
        gnum = r["group"]
        n_reviewed = len(self.decisions)
        n_total = len(self.review)

        # Detect duplicate diffuses
        removed = self.removed.get(gnum, set())
        active_nms = self._get_active_normalmaps(r)
        dupe_sets = {k: v for k, v in r["diffuse_hashes"].items()
                     if len(v) > 1}
        dupe_tag = "  ⚠ DUPE DIFFUSES" if dupe_sets else ""

        self.lbl_progress.config(
            text=f"Group {self.current + 1}/{n_total}  "
                 f"(reviewed: {n_reviewed})")
        self.lbl_info.config(
            text=f"Dedupe group #{gnum}  |  "
                 f"NM similarity: {r['avg_sim']:.3f}  |  "
                 f"{len(active_nms)} normalmaps  |  "
                 f"{sum(len(i['users']) for i in active_nms.values())} "
                 f"variants{dupe_tag}")

        # Show current decision
        self._update_decision_label(gnum)

        # For each normalmap row, show: [normalmap] | [diffuse tiles...]
        for nm_idx, (nm_name, info) in enumerate(active_nms.items()):
            row = ttk.Frame(self.scroll_frame, padding=3)
            row.pack(fill=tk.X, pady=2)

            # Normalmap column — clickable for CONSOLIDATE
            nm_frame = ttk.Frame(row)
            nm_frame.pack(side=tk.LEFT, padx=3)

            nm_img = info["img"]
            nm_thumb = nm_img.resize((self.THUMB, self.THUMB), Image.LANCZOS)
            nm_photo = ImageTk.PhotoImage(nm_thumb)
            self.photo_refs.append(nm_photo)

            nm_short = nm_name.split("/")[-1]
            ttk.Label(nm_frame, text=nm_short,
                      font=("Helvetica", 9, "bold"),
                      foreground="#5599ff").pack()

            nm_label = tk.Label(nm_frame, image=nm_photo, cursor="hand2",
                                borderwidth=3, relief="raised",
                                bg="#5599ff")
            nm_label.pack()
            nm_label.bind(
                "<Button-1>",
                lambda e, nm=nm_name: self.decide_consolidate(nm))
            ttk.Label(nm_frame, text="click → CONSOLIDATE",
                      font=("Helvetica", 8),
                      foreground="#5599ff").pack()

            # Separator
            ttk.Separator(row, orient=tk.VERTICAL).pack(
                side=tk.LEFT, fill=tk.Y, padx=5)

            # Diffuse thumbnails — clickable for MERGE
            for user_path in info["users"]:
                d_frame = ttk.Frame(row)
                d_frame.pack(side=tk.LEFT, padx=2)

                d_img, _ = load_image(self.textures_dir, user_path)
                if d_img:
                    d_thumb = d_img.resize((self.THUMB, self.THUMB),
                                          Image.LANCZOS)
                    d_photo = ImageTk.PhotoImage(d_thumb)
                    self.photo_refs.append(d_photo)

                    # Highlight duplicate diffuses
                    is_dupe = any(user_path in v for v in dupe_sets.values())
                    border_color = "#ff5555" if is_dupe else "#55aa55"

                    d_label = tk.Label(d_frame, image=d_photo,
                                       cursor="hand2",
                                       borderwidth=3, relief="raised",
                                       bg=border_color)
                    d_label.pack()
                    d_label.bind(
                        "<Button-1>",
                        lambda e, p=user_path: self.decide_merge(p))
                else:
                    tk.Label(d_frame, text="?", bg="#550000",
                             width=20, height=10).pack()

                d_short = user_path.split("/")[-1]
                ttk.Label(d_frame, text=d_short[:28],
                          font=("Helvetica", 8)).pack()

                # Show map references
                tex_name = user_path.rsplit(".", 1)[0]
                maps = self.map_refs.get(tex_name, [])
                if maps:
                    map_str = ", ".join(maps[:3])
                    ttk.Label(d_frame, text=f"📍 {map_str}",
                              font=("Helvetica", 7),
                              foreground="#ffaa00").pack()

                ttk.Label(d_frame, text="click → MERGE",
                          font=("Helvetica", 8),
                          foreground="#55aa55").pack()

                # Remove button
                rm_btn = ttk.Button(
                    d_frame, text="✗ remove",
                    command=lambda p=user_path: self.remove_texture(p))
                rm_btn.pack(pady=(2, 0))

            # Diff visualization for 2-normalmap groups
            if len(active_nms) == 2 and nm_idx == 0:
                nms = list(active_nms.values())
                a1 = np.array(nms[0]["img"]).astype(np.float32) / 255.0
                a2 = np.array(nms[1]["img"]).astype(np.float32) / 255.0
                if a1.shape == a2.shape:
                    diff = np.clip(np.abs(a1 - a2) * 10, 0, 1)
                    diff_img = Image.fromarray(
                        (diff * 255).astype(np.uint8))
                    diff_thumb = diff_img.resize(
                        (self.THUMB, self.THUMB), Image.LANCZOS)
                    diff_photo = ImageTk.PhotoImage(diff_thumb)
                    self.photo_refs.append(diff_photo)

                    diff_frame = ttk.Frame(row)
                    diff_frame.pack(side=tk.RIGHT, padx=5)
                    ttk.Label(diff_frame, text="DIFF ×10",
                              foreground="red",
                              font=("Helvetica", 9, "bold")).pack()
                    tk.Label(diff_frame, image=diff_photo,
                             bg="#333333").pack()

        self.canvas.yview_moveto(0)

    def _update_decision_label(self, gnum):
        dec = self.decisions.get(gnum)
        removed = self.removed.get(gnum, set())
        removed_tag = f"  ({len(removed)} removed)" if removed else ""
        if not dec:
            self.lbl_decision.config(
                text=f"⬤ PENDING{removed_tag}", foreground="gray")
        elif dec["action"] == "skip":
            self.lbl_decision.config(
                text=f"✗ SKIP{removed_tag}", foreground="orange")
        elif dec["action"] == "consolidate":
            short = dec["canonical_nm"].split("/")[-1]
            self.lbl_decision.config(
                text=f"✓ CONSOLIDATE → {short}{removed_tag}",
                foreground="#5599ff")
        elif dec["action"] == "merge":
            short = dec["canonical_texture"].split("/")[-1]
            self.lbl_decision.config(
                text=f"✓ MERGE → {short}{removed_tag}",
                foreground="#55ff55")

    def _load_decisions(self):
        """Load previous decisions from auto-save file."""
        if not os.path.exists(self.SAVE_PATH):
            return
        try:
            with open(self.SAVE_PATH) as f:
                data = json.load(f)
            for action in data.get("actions", []):
                gnum = action["group"]
                if action["action"] == "skip":
                    self.decisions[gnum] = {"action": "skip"}
                elif action["action"] == "consolidate":
                    self.decisions[gnum] = {
                        "action": "consolidate",
                        "canonical_nm": action["canonical_nm"],
                    }
                elif action["action"] == "merge":
                    self.decisions[gnum] = {
                        "action": "merge",
                        "canonical_texture": action["canonical_texture"],
                    }
            # Load removed textures
            for entry in data.get("removed", []):
                gnum = entry["group"]
                self.removed[gnum] = set(entry["textures"])
            print(f"Loaded {len(self.decisions)} decisions, "
                  f"{sum(len(v) for v in self.removed.values())} removals")
        except Exception as e:
            print(f"Warning: could not load decisions: {e}")

    def _auto_save(self):
        """Auto-save current decisions to disk."""
        self.export(quiet=True)

    def _get_active_normalmaps(self, r):
        """Get normalmaps for this group, excluding removed textures."""
        gnum = r["group"]
        removed = self.removed.get(gnum, set())
        active = {}
        for nm_name, info in r["normalmaps"].items():
            active_users = [u for u in info["users"] if u not in removed]
            if active_users:
                active[nm_name] = {**info, "users": active_users}
        return active

    def remove_texture(self, user_path):
        """Remove a texture from the current group."""
        if self.current >= len(self.review):
            return
        r = self.review[self.current]
        gnum = r["group"]

        if gnum not in self.removed:
            self.removed[gnum] = set()
        self.removed[gnum].add(user_path)

        # If this removal leaves < 2 normalmaps, auto-skip
        active = self._get_active_normalmaps(r)
        if len(active) < 2:
            self.decisions[gnum] = {"action": "skip"}

        # Clear any existing decision that referenced the removed texture
        dec = self.decisions.get(gnum)
        if dec:
            if (dec["action"] == "merge"
                    and dec.get("canonical_texture") == user_path):
                del self.decisions[gnum]
            elif (dec["action"] == "consolidate"
                  and dec.get("canonical_nm") in r["normalmaps"]):
                # Check if the canonical nm lost all its users
                nm = dec["canonical_nm"]
                if nm in r["normalmaps"]:
                    remaining = [u for u in r["normalmaps"][nm]["users"]
                                 if u not in self.removed.get(gnum, set())]
                    if not remaining:
                        del self.decisions[gnum]

        self._auto_save()
        self.show_group()

    def decide_skip(self):
        if self.current >= len(self.review):
            return
        gnum = self.review[self.current]["group"]
        self.decisions[gnum] = {"action": "skip"}
        self._update_decision_label(gnum)
        self._auto_save()
        self.root.after(300, self.next_group)

    def decide_consolidate(self, canonical_nm):
        """Share one normalmap across all variants (keep all diffuses)."""
        if self.current >= len(self.review):
            return
        gnum = self.review[self.current]["group"]
        self.decisions[gnum] = {
            "action": "consolidate",
            "canonical_nm": canonical_nm,
        }
        self._update_decision_label(gnum)
        self._auto_save()
        self.root.after(300, self.next_group)

    def enter_consolidate_mode(self):
        """Keyboard shortcut hint — consolidate is click-on-normalmap."""
        pass

    def decide_merge(self, canonical_texture):
        """Full merge — keep this texture, delete all others + remap maps."""
        if self.current >= len(self.review):
            return
        gnum = self.review[self.current]["group"]
        self.decisions[gnum] = {
            "action": "merge",
            "canonical_texture": canonical_texture,
        }
        self._update_decision_label(gnum)
        self._auto_save()
        self.root.after(300, self.next_group)

    def next_group(self):
        if self.current < len(self.review) - 1:
            self.current += 1
            self.show_group()

    def prev_group(self):
        if self.current > 0:
            self.current -= 1
            self.show_group()

    def export(self, quiet=False):
        """Export decisions to JSON and print summary."""
        output = {
            "textures_dir": self.textures_dir,
            "maps_dir": self.maps_dir,
            "total_groups": len(self.review),
            "reviewed": len(self.decisions),
            "actions": [],
            "removed": [],
        }

        # Persist removed textures
        for gnum, paths in self.removed.items():
            if paths:
                output["removed"].append({
                    "group": gnum,
                    "textures": sorted(paths),
                })

        merge_count = 0
        consolidate_count = 0
        skip_count = 0
        nm_delete_count = 0
        diffuse_delete_count = 0
        mat_update_count = 0
        map_remap_count = 0

        for r in self.review:
            gnum = r["group"]
            dec = self.decisions.get(gnum)
            if not dec:
                continue

            if dec["action"] == "skip":
                skip_count += 1
                output["actions"].append({"group": gnum, "action": "skip"})
                continue

            if dec["action"] == "merge":
                canonical_tex = dec["canonical_texture"]
                canonical_base = canonical_tex.rsplit(".", 1)[0]
                removed = self.removed.get(gnum, set())

                # Find which normalmap the canonical texture uses
                canonical_nm = None
                for nm_name, info in r["normalmaps"].items():
                    if canonical_tex in info["users"]:
                        canonical_nm = nm_name
                        break

                # Everything not the canonical (and not removed) gets deleted
                diffuse_deletes = []
                nm_deletes = []
                map_remaps = []
                mat_deletes = []

                for f in r["files"]:
                    fpath = f["path"]
                    if fpath == canonical_tex or fpath in removed:
                        continue
                    fbase = fpath.rsplit(".", 1)[0]
                    tex_name = fbase  # e.g. "quake/tek_la12"

                    diffuse_deletes.append(fpath)
                    diffuse_delete_count += 1

                    # Delete associated .mat
                    mat_path = fbase + ".mat"
                    mat_full = os.path.join(self.textures_dir, mat_path)
                    if os.path.exists(mat_full):
                        mat_deletes.append(mat_path)

                    # Remap map references
                    maps = self.map_refs.get(tex_name, [])
                    if maps:
                        map_remaps.append({
                            "old_texture": tex_name,
                            "new_texture": canonical_base,
                            "maps": maps,
                        })
                        map_remap_count += len(maps)

                # Delete non-canonical normalmaps
                for nm_name, info in r["normalmaps"].items():
                    if nm_name == canonical_nm:
                        continue
                    # Only delete if no remaining textures use it
                    remaining_users = [u for u in info["users"]
                                       if u == canonical_tex]
                    if not remaining_users:
                        nm_deletes.append(info["path"])
                        nm_delete_count += 1

                merge_count += 1
                output["actions"].append({
                    "group": gnum,
                    "action": "merge",
                    "canonical_texture": canonical_tex,
                    "canonical_nm": canonical_nm,
                    "diffuse_deletes": diffuse_deletes,
                    "nm_deletes": nm_deletes,
                    "mat_deletes": mat_deletes,
                    "map_remaps": map_remaps,
                })
                continue

            # action == "consolidate"
            canonical_nm = dec["canonical_nm"]
            canonical_nm_base = canonical_nm[:-4] \
                if canonical_nm.endswith(".tga") else canonical_nm
            removed = self.removed.get(gnum, set())

            nm_deletes = []
            mat_updates = []
            canonical_nm_path = r["normalmaps"][canonical_nm]["path"]

            for nm_name, info in r["normalmaps"].items():
                if nm_name == canonical_nm:
                    continue
                active_users = [u for u in info["users"]
                                if u not in removed]
                if not active_users:
                    continue
                nm_deletes.append(info["path"])
                nm_delete_count += 1

                for user in active_users:
                    user_base = user.rsplit(".", 1)[0]
                    mat_updates.append({
                        "texture": user,
                        "mat_file": user_base + ".mat",
                        "normalmap": canonical_nm_base,
                    })
                    mat_update_count += 1

            consolidate_count += 1
            output["actions"].append({
                "group": gnum,
                "action": "consolidate",
                "canonical_nm": canonical_nm,
                "canonical_nm_path": canonical_nm_path,
                "nm_deletes": nm_deletes,
                "mat_updates": mat_updates,
            })

        with open(self.SAVE_PATH, "w") as f:
            json.dump(output, f, indent=2)

        if quiet:
            return

        summary = (
            f"Exported to {self.SAVE_PATH}\n\n"
            f"Merge (full dedupe): {merge_count}\n"
            f"Consolidate (share nm): {consolidate_count}\n"
            f"Skip: {skip_count}\n"
            f"Pending: {len(self.review) - len(self.decisions)}\n"
            f"---\n"
            f"Diffuse files to delete: {diffuse_delete_count}\n"
            f"Normalmap files to delete: {nm_delete_count}\n"
            f"Mat files to update/create: {mat_update_count}\n"
            f"Map face remaps: {map_remap_count}\n"
        )

        popup = tk.Toplevel(self.root)
        popup.title("Export Summary")
        popup.geometry("450x300")
        text = tk.Text(popup, font=("Helvetica", 11))
        text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        text.insert("1.0", summary)
        text.config(state=tk.DISABLED)


def apply_decisions(textures_dir, maps_dir, decisions_path):
    """Apply the exported decisions."""
    with open(decisions_path) as f:
        data = json.load(f)

    for action in data["actions"]:
        if action["action"] == "skip":
            continue

        if action["action"] == "merge":
            canonical = action["canonical_texture"]
            print(f"\nGroup {action['group']}: MERGE → {canonical}")

            # Delete duplicate diffuses and their normalmaps
            for dpath in action.get("diffuse_deletes", []):
                full = os.path.join(textures_dir, dpath)
                if os.path.exists(full):
                    os.remove(full)
                    print(f"  DELETE diffuse: {dpath}")

            for nm_path in action.get("nm_deletes", []):
                if os.path.exists(nm_path):
                    os.remove(nm_path)
                    print(f"  DELETE normalmap: "
                          f"{os.path.relpath(nm_path, textures_dir)}")

            for mat_path in action.get("mat_deletes", []):
                full = os.path.join(textures_dir, mat_path)
                if os.path.exists(full):
                    os.remove(full)
                    print(f"  DELETE mat: {mat_path}")

            # Remap map references
            for remap in action.get("map_remaps", []):
                old_tex = remap["old_texture"]
                new_tex = remap["new_texture"]
                for map_name in remap["maps"]:
                    map_path = os.path.join(maps_dir, map_name)
                    if not os.path.exists(map_path):
                        continue
                    with open(map_path) as f:
                        content = f.read()
                    # Replace texture references in brush faces
                    new_content = content.replace(
                        f") {old_tex} ", f") {new_tex} ")
                    if new_content != content:
                        with open(map_path, "w") as f:
                            f.write(new_content)
                        count = content.count(f") {old_tex} ") \
                            - new_content.count(f") {old_tex} ")
                        print(f"  REMAP {map_name}: "
                              f"{old_tex} → {new_tex} ({count} faces)")

            continue

        if action["action"] == "consolidate":
            canonical = action["canonical_nm"]
            print(f"\nGroup {action['group']}: CONSOLIDATE → {canonical}")

            for nm_path in action.get("nm_deletes", []):
                if os.path.exists(nm_path):
                    os.remove(nm_path)
                    print(f"  DELETE normalmap: "
                          f"{os.path.relpath(nm_path, textures_dir)}")

            canonical_base = canonical[:-4] \
                if canonical.endswith(".tga") else canonical

            for upd in action.get("mat_updates", []):
                mat_path = os.path.join(textures_dir, upd["mat_file"])
                nm_ref = upd["normalmap"]

                if os.path.exists(mat_path):
                    with open(mat_path) as f:
                        content = f.read()
                    lines = content.split("\n")
                    new_lines = []
                    found = False
                    for line in lines:
                        if line.strip().startswith("normalmap "):
                            new_lines.append(f"\tnormalmap {nm_ref}")
                            found = True
                        else:
                            new_lines.append(line)
                    if not found:
                        for j, line in enumerate(new_lines):
                            if line.strip() == "{":
                                new_lines.insert(
                                    j + 1, f"\tnormalmap {nm_ref}")
                                break
                    with open(mat_path, "w") as f:
                        f.write("\n".join(new_lines))
                    print(f"  UPDATE {upd['mat_file']}: normalmap {nm_ref}")
                else:
                    tex_name = upd["texture"].rsplit(".", 1)[0]
                    content = (
                        f"{{\n"
                        f"\tdiffusemap {tex_name}\n"
                        f"\tnormalmap {nm_ref}\n"
                        f"}}\n"
                    )
                    os.makedirs(os.path.dirname(mat_path), exist_ok=True)
                    with open(mat_path, "w") as f:
                        f.write(content)
                    print(f"  CREATE {upd['mat_file']}: normalmap {nm_ref}")

    print("\nDone!")


def main():
    parser = argparse.ArgumentParser(
        description="Review color variant normalmaps for consolidation")
    parser.add_argument("textures_dir",
                        help="Path to textures directory")
    parser.add_argument("maps_dir", nargs="?", default=None,
                        help="Path to maps directory (for .map remapping)")
    parser.add_argument("--report", default=None,
                        help="Path to dedupe_report.json "
                             "(default: same dir as this script)")
    parser.add_argument("--execute", action="store_true",
                        help="Apply decisions from variant_decisions.json")
    args = parser.parse_args()

    textures_dir = os.path.abspath(args.textures_dir)
    maps_dir = os.path.abspath(args.maps_dir) if args.maps_dir else None
    report_path = args.report or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "dedupe_report.json")

    if args.execute:
        decisions_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "variant_decisions.json")
        if not os.path.exists(decisions_path):
            print("No variant_decisions.json found. Run GUI first.")
            sys.exit(1)
        apply_decisions(textures_dir, maps_dir, decisions_path)
        return

    if not os.path.exists(report_path):
        print(f"Report not found: {report_path}")
        print("Run dedupe.py first to generate it.")
        sys.exit(1)

    print("Building review list...")
    review_list = build_review_list(textures_dir, report_path)
    print(f"Found {len(review_list)} groups to review.")

    print("Scanning map references...")
    map_refs = scan_map_references(maps_dir)
    print(f"Found {len(map_refs)} unique texture references in maps.")

    root = tk.Tk()
    root.geometry("1400x800")
    VariantReviewApp(root, textures_dir, maps_dir, review_list, map_refs)
    root.mainloop()


if __name__ == "__main__":
    main()
