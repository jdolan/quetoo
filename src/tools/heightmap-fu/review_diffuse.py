#!/usr/bin/env python3
"""
Interactive diffuse correction and albedo review tool.

Three-panel preview:
    [ Original diffuse ]  [ Corrected albedo ]  [ Integrated height ]

Controls:
  Drag yellow circle    — move light source, auto-updates shadow angle
  Single-click image    — set high point (⊕ yellow); auto-flips height if inverted
  Shift-click image     — set low point  (⊕ blue);   auto-flips height if inverted
  f / Flip norm Y       — toggle Y-axis convention and re-integrate
  r / Revert            — reset sliders
  Enter / Save          — write corrected diffuse + updated norm heightmap
"""

import argparse
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import ttk

from correct_diffuse import (correct_diffuse, integrate_normals,
                              height_from_diffuse, fit_light_direction)

# Directory containing this script — used to locate quake_textures/
_TOOL_DIR = Path(__file__).resolve().parent
_QUAKE_REF_DIR = _TOOL_DIR / "quake_textures"

# ---------------------------------------------------------------------------
# Parameters  (light position is set by dragging the ☀ handle, not a slider)
# ---------------------------------------------------------------------------

DEFAULTS = dict(
    gamma=1.0,
    shadow_reach=0,
    shadow_strength=0.8,
    ref_strength=0.3,
    global_delight=0.0,
)

PARAM_SPECS = [
    # (key,             label,                       from,   to,  step,  is_int)
    ("gamma",           "Output gamma",              0.25,     3,  0.05,  False),
    ("global_delight",  "Global delight (Retinex)",    0,      1,  0.05,  False),
    ("shadow_reach",    "Shadow reach (px)",           0,     60,   1,    True),
    ("shadow_strength", "Shadow strength",             0,      8,  0.1,   False),
    ("ref_strength",    "Quake ref blend",             0,      1,  0.05,  False),
]

MAX_DISPLAY  = 380   # max canvas px per panel
LIGHT_RADIUS = 10    # handle circle radius (px)

_SKIP_SUFFIXES = ("_norm", "_gloss", "_spec", "_emit", "_local", "_height")


def is_diffuse(path: Path) -> bool:
    return not any(path.stem.lower().endswith(s) for s in _SKIP_SUFFIXES)


# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------

class ReviewApp:
    def __init__(self, root: tk.Tk, files: list[Path], start_index: int = 0):
        self.root = root
        self.files = files
        self.index = start_index

        self.original_rgb: np.ndarray | None = None
        self.norm_arr: np.ndarray | None = None
        self.ref_rgb: np.ndarray | None = None          # Quake reference (may be None)
        self._norm_alpha_height: np.ndarray | None = None  # raw alpha channel from .norm, never mutated
        self._base_height: np.ndarray | None = None    # raw F-C output, never mutated
        self.integrated_height: np.ndarray | None = None  # tone-curve adjusted
        self._flip_y = False
        self._fitted_light_dir: tuple | None = None    # auto-fitted (lx,ly,lz)

        # Ranked markers: list of [ix, iy, rank]  rank ∈ 1..5
        self._markers: list[list[int]] = []

        # Light handle state — position in canvas coords; converted to image coords on use
        self._light_pos: tuple | None = None  # (cx, cy) canvas coords

        # Drag bookkeeping
        self._dragging_light    = False
        self._drag_start:       tuple | None = None
        self._drag_light_start: tuple | None = None

        self.root.title("Diffuse Correction Review")
        self.root.resizable(True, True)
        self._build_ui()
        self._load_current()

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _build_ui(self):
        top = tk.Frame(self.root)
        top.pack(side=tk.TOP, fill=tk.X, padx=8, pady=(8, 0))
        self.progress_var = tk.StringVar()
        tk.Label(top, textvariable=self.progress_var, anchor="w",
                 font=("Helvetica", 12, "bold")).pack(side=tk.LEFT)
        self.info_var = tk.StringVar()
        tk.Label(top, textvariable=self.info_var, anchor="e",
                 font=("Helvetica", 10), fg="#666").pack(side=tk.RIGHT)

        # Image canvases
        img_frame = tk.Frame(self.root, bg="#1e1e1e")
        img_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=8, pady=8)

        self._panel_title_labels = {}
        for col, (key, text, color) in enumerate([
                ("orig",        "ORIGINAL DIFFUSE",  "#dcdc50"),
                ("albedo",      "CORRECTED ALBEDO",   "#50dc50"),
                ("height_orig", "ORIGINAL HEIGHT",    "#dc8050"),
                ("height",      "CORRECTED HEIGHT",   "#50dcdc"),
        ]):
            lbl = tk.Label(img_frame, text=text, bg="#1e1e1e", fg=color,
                           font=("Helvetica", 10))
            lbl.grid(row=0, column=col, padx=4)
            self._panel_title_labels[key] = lbl

        sz = MAX_DISPLAY
        self.canvas_orig         = tk.Canvas(img_frame, width=sz, height=sz,
                                             bg="#282828", highlightthickness=0)
        self.canvas_albedo       = tk.Canvas(img_frame, width=sz, height=sz,
                                             bg="#282828", highlightthickness=0)
        self.canvas_height_orig  = tk.Canvas(img_frame, width=sz, height=sz,
                                             bg="#282828", highlightthickness=0)
        self.canvas_height       = tk.Canvas(img_frame, width=sz, height=sz,
                                             bg="#282828", highlightthickness=0)
        self.canvas_orig        .grid(row=1, column=0, padx=4)
        self.canvas_albedo      .grid(row=1, column=1, padx=4)
        self.canvas_height_orig .grid(row=1, column=2, padx=4)
        self.canvas_height      .grid(row=1, column=3, padx=4)

        # Click = place/rank markers; right-click = remove; 'c' = clear all
        for canvas in (self.canvas_orig, self.canvas_albedo,
                       self.canvas_height_orig, self.canvas_height):
            canvas.bind("<Button-1>",       self._on_click)
            canvas.bind("<Shift-Button-1>", self._on_shift_click)
            canvas.bind("<Button-2>",       self._on_right_click)
            canvas.bind("<Button-3>",       self._on_right_click)

        # Light drag: motion/release bound to the widget so they fire even
        # after the cursor leaves the small handle circle.
        self.canvas_orig.bind("<B1-Motion>",       self._light_drag_motion)
        self.canvas_orig.bind("<ButtonRelease-1>", self._light_drag_end)

        # Sliders
        slider_frame = ttk.LabelFrame(self.root, text="Correction parameters")
        slider_frame.pack(side=tk.TOP, fill=tk.X, padx=8, pady=4)
        self.slider_vars: dict[str, tk.Variable] = {}

        for row, (key, label, lo, hi, step, is_int) in enumerate(PARAM_SPECS):
            var = tk.IntVar(value=DEFAULTS[key]) if is_int \
                else tk.DoubleVar(value=DEFAULTS[key])
            self.slider_vars[key] = var
            tk.Label(slider_frame, text=label, width=26, anchor="w") \
                .grid(row=row, column=0, padx=(8, 4), pady=2, sticky="w")
            val_label = tk.Label(slider_frame, width=6, anchor="e")
            val_label.grid(row=row, column=2, padx=(4, 8))

            def _update(v, vl=val_label, vr=var, is_i=is_int):
                vl.config(text=str(int(vr.get())) if is_i else f"{vr.get():.2f}")
                self._reprocess()

            scale = ttk.Scale(slider_frame, from_=lo, to=hi, variable=var,
                              orient=tk.HORIZONTAL, command=_update, length=400)
            scale.grid(row=row, column=1, padx=4, pady=2, sticky="ew")
            val_label.config(text=str(DEFAULTS[key]) if is_int
                             else f"{DEFAULTS[key]:.2f}")

        slider_frame.columnconfigure(1, weight=1)

        # Buttons
        btn_frame = tk.Frame(self.root)
        btn_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=8)
        tk.Button(btn_frame, text="◀  Prev",    width=10, command=self._prev) \
            .pack(side=tk.LEFT, padx=4)
        tk.Button(btn_frame, text="Skip  ▶",    width=10, command=self._skip) \
            .pack(side=tk.LEFT, padx=4)
        tk.Button(btn_frame, text="Revert",      width=10, command=self._revert,
                  fg="#a05000") \
            .pack(side=tk.LEFT, padx=4)
        tk.Button(btn_frame, text="Flip norm Y", width=12, command=self._flip_norm_y,
                  fg="#505080") \
            .pack(side=tk.LEFT, padx=4)
        tk.Button(btn_frame, text="💾  Save",    width=14, command=self._save,
                  bg="#206020", fg="white",
                  font=("Helvetica", 10, "bold")) \
            .pack(side=tk.RIGHT, padx=4)

        self.root.bind("<Left>",   lambda e: self._prev())
        self.root.bind("<Right>",  lambda e: self._skip())
        self.root.bind("<Return>", lambda e: self._save())
        self.root.bind("r",        lambda e: self._revert())
        self.root.bind("f",        lambda e: self._flip_norm_y())
        self.root.bind("c",        lambda e: self._clear_markers())

    # ------------------------------------------------------------------
    # File navigation
    # ------------------------------------------------------------------

    def _load_current(self):
        if not (0 <= self.index < len(self.files)):
            self._done()
            return

        path = self.files[self.index]
        self.progress_var.set(
            f"{self.index + 1} / {len(self.files)}   —   {path.name}"
        )

        try:
            img = Image.open(path).convert("RGB")
        except Exception as e:
            print(f"Could not open {path}: {e}")
            self.index += 1
            self._load_current()
            return

        self.original_rgb = np.array(img)
        self._flip_y      = False
        self._markers     = []
        self._fitted_light_dir = None
        self._norm_alpha_height = None

        norm_path = path.parent / (path.stem + "_norm.tga")
        if norm_path.exists():
            try:
                self.norm_arr  = np.array(Image.open(norm_path).convert("RGBA"))
                alpha = self.norm_arr[:, :, 3]
                if alpha.max() > alpha.min():
                    self._norm_alpha_height = alpha.astype(np.float32) / 255.0
                self._base_height = integrate_normals(self.norm_arr, flip_y=False)
            except Exception as e:
                print(f"Could not load norm {norm_path}: {e}")
                self.norm_arr     = None
                self._base_height = height_from_diffuse(self.original_rgb)
        else:
            self.norm_arr     = None
            self._base_height = height_from_diffuse(self.original_rgb)

        # Pre-fit the baked light direction from normal map + diffuse correlation
        if self.norm_arr is not None:
            luma = (self.original_rgb[:, :, 0].astype(np.float32) * 0.2126
                  + self.original_rgb[:, :, 1].astype(np.float32) * 0.7152
                  + self.original_rgb[:, :, 2].astype(np.float32) * 0.0722) / 255.0
            self._fitted_light_dir, corr = fit_light_direction(self.norm_arr, luma)
            lx, ly, lz = self._fitted_light_dir
            az = math.degrees(math.atan2(ly, lx))
            el = math.degrees(math.asin(max(-1.0, min(1.0, lz))))
            print(f"  fitted light: az={az:.0f}°  el={el:.0f}°  r={corr:.3f}")

        # Look for matching Quake reference texture
        ref_candidates = [
            _QUAKE_REF_DIR / f"{path.stem}.png",
            _QUAKE_REF_DIR / f"{path.stem.lower()}.png",
        ]
        self.ref_rgb = None
        for rc in ref_candidates:
            if rc.exists():
                try:
                    self.ref_rgb = np.array(Image.open(rc).convert("RGB"))
                    print(f"  ref texture:  {rc.name}")
                except Exception:
                    pass
                break

        self.integrated_height = self._base_height.copy()

        self._update_panel_labels()
        self._reset_light_pos()
        self._revert()
        self._update_orig_canvas()

    def _advance(self):
        self.index += 1
        if self.index >= len(self.files):
            self._done()
        else:
            self._load_current()

    def _prev(self):
        if self.index > 0:
            self.index -= 1
            self._load_current()

    def _skip(self):
        self._advance()

    def _revert(self):
        for key, var in self.slider_vars.items():
            var.set(DEFAULTS[key])
        self._reprocess()

    def _flip_norm_y(self):
        if self.norm_arr is None:
            return
        self._flip_y = not self._flip_y
        self._base_height = integrate_normals(self.norm_arr, flip_y=self._flip_y)
        self._update_height_orig_canvas()
        self._apply_marker_curve()
        self._reprocess()

    # ------------------------------------------------------------------
    # Save
    # ------------------------------------------------------------------

    def _save(self):
        if self.original_rgb is None or self._result is None:
            return
        corrected_rgb, heightmap, _ = self._result
        path = self.files[self.index]

        Image.fromarray(corrected_rgb, mode="RGB").save(path)
        print(f"Saved diffuse: {path}")

        norm_path = path.parent / (path.stem + "_norm.tga")
        if norm_path.exists():
            norm_img  = Image.open(norm_path).convert("RGBA")
            norm_arr  = np.array(norm_img)
            if norm_arr.shape[:2] != heightmap.shape[:2]:
                heightmap_out = np.array(
                    Image.fromarray(heightmap).resize(
                        (norm_arr.shape[1], norm_arr.shape[0]), Image.LANCZOS))
            else:
                heightmap_out = heightmap
            norm_arr[:, :, 3] = heightmap_out
            Image.fromarray(norm_arr, mode="RGBA").save(norm_path)
            print(f"Updated norm:  {norm_path}")
        else:
            h, w = heightmap.shape
            norm_arr = np.zeros((h, w, 4), dtype=np.uint8)
            norm_arr[:, :, 0] = 128
            norm_arr[:, :, 1] = 128
            norm_arr[:, :, 2] = 255
            norm_arr[:, :, 3] = heightmap
            Image.fromarray(norm_arr, mode="RGBA").save(norm_path)
            print(f"Created norm:  {norm_path}")

        self._advance()

    def _done(self):
        self.progress_var.set("All done!")
        self.info_var.set("")
        self.root.after(1500, self.root.destroy)

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    _result: tuple | None = None
    _reprocess_pending: bool = False

    def _reprocess(self, *_):
        if self._reprocess_pending:
            return
        self._reprocess_pending = True
        self.root.after(30, self._do_reprocess)

    def _do_reprocess(self):
        self._reprocess_pending = False
        if self.original_rgb is None:
            return

        def iv(k): return int(self.slider_vars[k].get())
        def fv(k): return float(self.slider_vars[k].get())

        lx, ly = self._light_image_pos()
        # Pass manual light pos only when the user has explicitly dragged the handle
        # away from default; otherwise let correct_diffuse auto-fit via normal map.
        manual_light = (lx, ly) if self._light_pos is not None else (None, None)

        # Auto-scale Retinex sigma to span at least half the texture's shorter edge,
        # so it removes full-height illumination gradients (not just local spots).
        h, w = self.original_rgb.shape[:2]
        illum_sigma = max(min(h, w) / 3.0, 40.0)

        self._result = correct_diffuse(
            self.original_rgb,
            illum_sigma     = illum_sigma,
            strength        = fv("global_delight"),
            gamma           = fv("gamma"),
            height_field    = self.integrated_height,
            norm_arr        = self.norm_arr,
            light_x         = manual_light[0],
            light_y         = manual_light[1],
            shadow_reach    = iv("shadow_reach"),
            shadow_strength = fv("shadow_strength"),
            ref_rgb         = self.ref_rgb,
            ref_strength    = fv("ref_strength"),
        )

        corrected_rgb, heightmap, light_dir = self._result
        norm_tag = "  ✓ norm" if self.norm_arr is not None else "  ✗ no norm"
        ref_tag  = "  ✓ ref"  if self.ref_rgb  is not None else ""

        if light_dir is not None:
            dlx, dly, dlz = light_dir
            az = math.degrees(math.atan2(dly, dlx))
            el = math.degrees(math.asin(max(-1.0, min(1.0, dlz))))
            light_info = f"  ☀ az={az:.0f}° el={el:.0f}°"
        else:
            light_info = f"  ☀ ({lx:.0f},{ly:.0f})"

        self.info_var.set(f"{w}×{h}{norm_tag}{ref_tag}{light_info}")

        self._update_albedo_canvas(corrected_rgb)
        self._update_height_canvas(heightmap)
        self._draw_overlays()

    # ------------------------------------------------------------------
    # Light handle
    # ------------------------------------------------------------------

    def _light_image_pos(self) -> tuple[float, float]:
        """Light position in image pixel coordinates."""
        if self._light_pos is None or self.original_rgb is None:
            h, w = (self.original_rgb.shape[:2]
                    if self.original_rgb is not None else (256, 256))
            return w * 1.2, -h * 0.2
        s = self._display_scale()
        return self._light_pos[0] / s, self._light_pos[1] / s

    def _reset_light_pos(self):
        """Place light handle at top-right corner (≈315° directional default)."""
        s = self._display_scale()
        if self.original_rgb is not None:
            h, w = self.original_rgb.shape[:2]
        else:
            h = w = MAX_DISPLAY
        dw, dh = int(w * s), int(h * s)
        # Off top-right corner
        self._light_pos = (dw * 1.1, -dh * 0.1)

    def _draw_light_handle(self):
        self.canvas_orig.delete("light")
        if self._light_pos is None:
            return

        lx, ly = self._light_pos
        s = self._display_scale()
        if self.original_rgb is not None:
            h, w = self.original_rgb.shape[:2]
        else:
            h = w = MAX_DISPLAY
        cx, cy = int(w * s) / 2.0, int(h * s) / 2.0
        r = LIGHT_RADIUS

        self.canvas_orig.create_line(cx, cy, lx, ly,
                                     fill="#ffff80", width=1, dash=(4, 3),
                                     tags="light")
        self.canvas_orig.create_oval(lx - r, ly - r, lx + r, ly + r,
                                     fill="#ffff00", outline="#ffffff", width=2,
                                     tags="light")
        # Label showing image-space position
        ix, iy = self._light_image_pos()
        self.canvas_orig.create_text(lx + r + 4, ly,
                                     text=f"☀ ({ix:.0f},{iy:.0f})",
                                     fill="#ffff80", anchor="w",
                                     font=("Helvetica", 8), tags="light")

        self.canvas_orig.tag_bind("light", "<Button-1>", self._light_drag_start)

    def _light_drag_start(self, event):
        self._dragging_light   = True
        self._drag_start       = (event.x, event.y)
        self._drag_light_start = self._light_pos

    def _light_drag_motion(self, event):
        if not self._dragging_light or self._drag_start is None \
                or self._drag_light_start is None:
            return
        dx = event.x - self._drag_start[0]
        dy = event.y - self._drag_start[1]
        self._light_pos = (self._drag_light_start[0] + dx,
                           self._drag_light_start[1] + dy)
        self._draw_light_handle()
        self._reprocess()

    def _light_drag_end(self, event):
        self._dragging_light   = False
        self._drag_start       = None
        self._drag_light_start = None

    # ------------------------------------------------------------------
    # Marker tone-curve
    # ------------------------------------------------------------------

    # Colors for ranks 1-5
    RANK_COLORS = {1: "#4466ff", 2: "#44ccff", 3: "#44dd44", 4: "#ffaa00", 5: "#ffff00"}

    def _apply_marker_curve(self):
        """Rebuild integrated_height from _base_height using marker tone curve."""
        if self._base_height is None:
            return
        bh = self._base_height
        if not self._markers:
            self.integrated_height = bh.copy()
            return

        # Sample base height at each marker position
        pairs = []
        for mx, my, rank in self._markers:
            src = float(bh[my, mx])
            tgt = rank / 5.0
            pairs.append((src, tgt))

        # Sort by source value, deduplicate (keep last if same src)
        pairs.sort(key=lambda p: p[0])
        seen = {}
        for src, tgt in pairs:
            seen[src] = tgt
        pairs = sorted(seen.items())

        # Add boundary anchors only if not already pinned by markers
        xs = [p[0] for p in pairs]
        ys = [p[1] for p in pairs]
        if xs[0] > 0.0:
            xs = [0.0] + xs;  ys = [0.0] + ys
        if xs[-1] < 1.0:
            xs = xs + [1.0];  ys = ys + [1.0]

        remapped = np.interp(bh, xs, ys).astype(np.float32)
        self.integrated_height = np.clip(remapped, 0.0, 1.0)

    def _find_nearest_marker(self, ix: int, iy: int, radius_img: int = 12) -> int | None:
        """Return index of nearest marker within radius_img pixels, or None."""
        best_idx, best_d2 = None, radius_img * radius_img
        for i, (mx, my, _) in enumerate(self._markers):
            d2 = (mx - ix) ** 2 + (my - iy) ** 2
            if d2 <= best_d2:
                best_idx, best_d2 = i, d2
        return best_idx

    def _clear_markers(self):
        self._markers = []
        self._apply_marker_curve()
        self._reprocess()

    # ------------------------------------------------------------------
    # Click handlers
    # ------------------------------------------------------------------

    def _display_scale(self) -> float:
        if self.original_rgb is None:
            return 1.0
        h, w = self.original_rgb.shape[:2]
        m = max(w, h)
        return MAX_DISPLAY / m if m > MAX_DISPLAY else 1.0

    def _canvas_to_image(self, cx: int, cy: int) -> tuple[int, int]:
        s = self._display_scale()
        return int(cx / s), int(cy / s)

    def _clamp_to_image(self, ix: int, iy: int) -> tuple[int, int]:
        if self.integrated_height is not None:
            h, w = self.integrated_height.shape
        elif self.original_rgb is not None:
            h, w = self.original_rgb.shape[:2]
        else:
            return ix, iy
        return max(0, min(ix, w - 1)), max(0, min(iy, h - 1))

    def _height_at(self, ix: int, iy: int) -> float | None:
        if self._base_height is None:
            return None
        return float(self._base_height[iy, ix])

    def _on_click(self, event):
        if self._dragging_light:
            return
        ix, iy = self._clamp_to_image(*self._canvas_to_image(event.x, event.y))
        idx = self._find_nearest_marker(ix, iy)
        if idx is not None:
            # Increment rank of existing marker (up to 5)
            self._markers[idx][2] = min(self._markers[idx][2] + 1, 5)
        else:
            self._markers.append([ix, iy, 3])  # default mid rank
        self._apply_marker_curve()
        self._draw_overlays()
        self._reprocess()
        mk = self._markers[idx if idx is not None else -1]
        h_str = f"  h={self._height_at(mk[0], mk[1]):.3f}" if self._height_at(mk[0], mk[1]) is not None else ""
        self.info_var.set(f"Marker [{mk[0]},{mk[1]}] rank={mk[2]}{h_str}")

    def _on_shift_click(self, event):
        if self._dragging_light:
            return
        ix, iy = self._clamp_to_image(*self._canvas_to_image(event.x, event.y))
        idx = self._find_nearest_marker(ix, iy)
        if idx is not None:
            # Decrement rank of existing marker (down to 1)
            self._markers[idx][2] = max(self._markers[idx][2] - 1, 1)
            self._apply_marker_curve()
            self._draw_overlays()
            self._reprocess()
            mk = self._markers[idx]
            self.info_var.set(f"Marker [{mk[0]},{mk[1]}] rank={mk[2]}")
        else:
            # Shift-click empty space = place rank 1 (low point)
            self._markers.append([ix, iy, 1])
            self._apply_marker_curve()
            self._draw_overlays()
            self._reprocess()
            h_str = f"  h={self._height_at(ix, iy):.3f}" if self._height_at(ix, iy) is not None else ""
            self.info_var.set(f"Marker [{ix},{iy}] rank=1{h_str}")

    def _on_right_click(self, event):
        ix, iy = self._clamp_to_image(*self._canvas_to_image(event.x, event.y))
        idx = self._find_nearest_marker(ix, iy)
        if idx is not None:
            removed = self._markers.pop(idx)
            self._apply_marker_curve()
            self._draw_overlays()
            self._reprocess()
            self.info_var.set(f"Removed marker [{removed[0]},{removed[1]}] rank={removed[2]}")

    # ------------------------------------------------------------------
    # Canvas drawing
    # ------------------------------------------------------------------

    def _draw_overlays(self):
        s = self._display_scale()

        def _draw_marker(canvas, ix, iy, rank):
            color = self.RANK_COLORS.get(rank, "#ffffff")
            cx_, cy_ = int(ix * s), int(iy * s)
            r = 8
            canvas.create_oval(cx_ - r, cy_ - r, cx_ + r, cy_ + r,
                                fill="", outline=color, width=2, tags="marker")
            canvas.create_text(cx_, cy_, text=str(rank),
                                fill=color, font=("Helvetica", 8, "bold"),
                                tags="marker")

        for canvas in (self.canvas_orig, self.canvas_albedo, self.canvas_height):
            canvas.delete("marker")
            for mx, my, rank in self._markers:
                _draw_marker(canvas, mx, my, rank)

        self._draw_light_handle()

    def _draw_image(self, canvas: tk.Canvas, photo: ImageTk.PhotoImage):
        canvas.delete("img")
        canvas.create_image(0, 0, anchor=tk.NW, image=photo, tags="img")
        canvas.tag_lower("img")   # keep below markers and light handle

    def _to_photo(self, arr_rgb: np.ndarray) -> ImageTk.PhotoImage:
        img = Image.fromarray(arr_rgb, mode="RGB")
        w, h = img.size
        if max(w, h) > MAX_DISPLAY:
            sc = MAX_DISPLAY / max(w, h)
            img = img.resize((int(w * sc), int(h * sc)), Image.LANCZOS)
        return ImageTk.PhotoImage(img)

    def _grey_to_photo(self, arr: np.ndarray) -> ImageTk.PhotoImage:
        img = Image.fromarray(arr, mode="L").convert("RGB")
        w, h = img.size
        if max(w, h) > MAX_DISPLAY:
            sc = MAX_DISPLAY / max(w, h)
            img = img.resize((int(w * sc), int(h * sc)), Image.LANCZOS)
        return ImageTk.PhotoImage(img)

    def _update_orig_canvas(self):
        if self.original_rgb is None:
            return
        self._photo_orig = self._to_photo(self.original_rgb)
        self._draw_image(self.canvas_orig, self._photo_orig)
        self._update_height_orig_canvas()
        self._draw_overlays()

    def _update_albedo_canvas(self, rgb: np.ndarray):
        self._photo_albedo = self._to_photo(rgb)
        self._draw_image(self.canvas_albedo, self._photo_albedo)

    def _update_height_orig_canvas(self):
        # Prefer the raw alpha channel from the .norm file; fall back to F-C integrated
        src = self._norm_alpha_height if self._norm_alpha_height is not None else self._base_height
        if src is None:
            return
        raw = (src * 255).clip(0, 255).astype(np.uint8)
        self._photo_height_orig = self._grey_to_photo(raw)
        self._draw_image(self.canvas_height_orig, self._photo_height_orig)

    def _update_height_canvas(self, grey: np.ndarray):
        self._photo_height = self._grey_to_photo(grey)
        self._draw_image(self.canvas_height, self._photo_height)

    def _update_panel_labels(self):
        norm_tag = "✓" if self.norm_arr is not None else "✗"
        self._panel_title_labels["height"].config(
            text=f"CORRECTED HEIGHT  [{norm_tag} norm]",
            fg="#50dcdc" if self.norm_arr is not None else "#a0a0a0"
        )


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _textures_from_map(map_path: Path, tex_root: Path) -> list[Path]:
    """
    Parse a Quake .map file and return sorted unique diffuse TGA paths.

    Brush face lines have the form:
        ( x y z ) ( x y z ) ( x y z ) TEXTURE u v rot xs ys
    The texture name (e.g. "common/quake/city4_2") is the 10th whitespace-
    separated token.  We try <tex_root>/<name>.tga (and .png as fallback).
    """
    import re
    face_re = re.compile(
        r'^\s*\(\s*[-\d. ]+\)\s*\(\s*[-\d. ]+\)\s*\(\s*[-\d. ]+\)\s+(\S+)'
    )
    textures: set[str] = set()
    with open(map_path, "r", errors="replace") as fh:
        for line in fh:
            m = face_re.match(line)
            if m:
                textures.add(m.group(1).lower())

    files: list[Path] = []
    seen: set[Path] = set()
    for tex in sorted(textures):
        for ext in (".tga", ".png"):
            candidate = tex_root / (tex + ext)
            if candidate.exists() and is_diffuse(candidate) and candidate not in seen:
                files.append(candidate)
                seen.add(candidate)
                break
    return files


def main():
    parser = argparse.ArgumentParser(
        description="Interactive diffuse-to-albedo correction tool"
    )
    # Input sources (mutually exclusive)
    src = parser.add_mutually_exclusive_group()
    src.add_argument(
        "--directory", "-d", metavar="DIR",
        help="Scan a directory recursively for diffuse TGA files"
    )
    src.add_argument(
        "--map", "-m", metavar="MAPFILE",
        help="Parse a Quake .map file and review its distinct textures"
    )
    # When --map is used we need to know where textures live
    parser.add_argument(
        "--textures", "-t", metavar="TEXDIR",
        help="Texture root directory (required with --map; e.g. .../textures/)"
    )
    # Positional: explicit file list (used when neither --directory nor --map given)
    parser.add_argument(
        "files", nargs="*", metavar="FILE",
        help="Explicit list of diffuse TGA/PNG files to review"
    )
    parser.add_argument("--start", default=None, metavar="FILENAME")
    args = parser.parse_args()

    files: list[Path] = []

    if args.map:
        if not args.textures:
            parser.error("--textures DIR is required when using --map")
        map_path = Path(args.map).expanduser()
        tex_root = Path(args.textures).expanduser()
        if not map_path.exists():
            print(f"Map file not found: {map_path}")
            sys.exit(1)
        if not tex_root.is_dir():
            print(f"Texture directory not found: {tex_root}")
            sys.exit(1)
        files = _textures_from_map(map_path, tex_root)
        if not files:
            print(f"No diffuse textures found for map {map_path.name} under {tex_root}")
            sys.exit(1)
        print(f"Found {len(files)} diffuse textures in {map_path.name}.")

    elif args.directory:
        directory = Path(args.directory).expanduser()
        if not directory.is_dir():
            print(f"Directory not found: {directory}")
            sys.exit(1)
        files = [f for f in sorted(directory.rglob("*.tga")) if is_diffuse(f)]
        if not files:
            print(f"No diffuse TGA files found in {directory}")
            sys.exit(1)
        print(f"Found {len(files)} diffuse files.")

    elif args.files:
        for p in args.files:
            path = Path(p).expanduser()
            if path.exists():
                files.append(path)
            else:
                print(f"Warning: file not found: {path}")
        files = sorted(set(files))
        if not files:
            print("No valid files provided.")
            sys.exit(1)
        print(f"Reviewing {len(files)} file(s).")

    else:
        parser.print_help()
        sys.exit(1)

    start_index = 0
    if args.start:
        matches = [i for i, f in enumerate(files) if args.start in f.name]
        if matches:
            start_index = matches[0]
            print(f"Starting at file {start_index + 1}: {files[start_index].name}")
        else:
            print(f"Warning: --start '{args.start}' not found, starting from beginning")

    root = tk.Tk()
    ReviewApp(root, files, start_index=start_index)
    root.mainloop()


if __name__ == "__main__":
    main()
