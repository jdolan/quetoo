#!/usr/bin/env python3
"""
Interactive heightmap review tool.

Steps through all *_norm.tga files in a directory, showing a live before/after
preview with adjustable filter parameters. Save writes the processed heightmap
back into the TGA's alpha channel in-place.

Usage:
    python review_heightmaps.py /path/to/textures
    python review_heightmaps.py /path/to/textures --pattern "*_norm.tga"
"""

import argparse
import io
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import ttk

from clean_heightmaps import process_heightmap

# ---------------------------------------------------------------------------
# Defaults — must stay in sync with clean_heightmaps.py
# ---------------------------------------------------------------------------
DEFAULTS = dict(
    median=5,
    bilateral_d=9,
    bilateral_sigma=75,
    sharpen_sigma=5.0,
    sharpen_amount=2.5,
    sharpen2_sigma=8.0,
    sharpen2_amount=2.0,
)

PARAM_SPECS = [
    # (key,              label,                   from,  to,   step,  is_int)
    ("median",           "Median blur",            0,    21,    2,    True),
    ("bilateral_d",      "Bilateral diameter",     0,    31,    2,    True),
    ("bilateral_sigma",  "Bilateral sigma",        0,   200,    5,    True),
    ("sharpen_sigma",    "Sharpen (large) sigma",  0,    20,  0.5,   False),
    ("sharpen_amount",   "Sharpen (large) amount", 0,     6,  0.25,  False),
    ("sharpen2_sigma",   "Sharpen (fine) sigma",   0,    20,  0.5,   False),
    ("sharpen2_amount",  "Sharpen (fine) amount",  0,     6,  0.25,  False),
]

MAX_DISPLAY = 512   # max px per image panel in the preview


class ReviewApp:
    def __init__(self, root: tk.Tk, files: list[Path]):
        self.root = root
        self.files = files
        self.index = 0
        self.original_alpha: np.ndarray | None = None   # pristine alpha from disk
        self.current_image: Image.Image | None = None   # full RGBA image from disk

        self.root.title("Heightmap Review")
        self.root.resizable(True, True)

        self._build_ui()
        self._load_current()

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _build_ui(self):
        # ── top: progress label ──────────────────────────────────────
        top = tk.Frame(self.root)
        top.pack(side=tk.TOP, fill=tk.X, padx=8, pady=(8, 0))

        self.progress_var = tk.StringVar()
        tk.Label(top, textvariable=self.progress_var, anchor="w",
                 font=("Helvetica", 12, "bold")).pack(side=tk.LEFT)

        self.std_var = tk.StringVar()
        tk.Label(top, textvariable=self.std_var, anchor="e",
                 font=("Helvetica", 10), fg="#666").pack(side=tk.RIGHT)

        # ── middle: before / after canvases ──────────────────────────
        img_frame = tk.Frame(self.root, bg="#1e1e1e")
        img_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=8, pady=8)

        self.before_label = tk.Label(img_frame, text="BEFORE", bg="#1e1e1e",
                                     fg="#dcdc50", font=("Helvetica", 10))
        self.before_label.grid(row=0, column=0, sticky="w", padx=4)
        self.after_label = tk.Label(img_frame, text="AFTER", bg="#1e1e1e",
                                    fg="#50dc50", font=("Helvetica", 10))
        self.after_label.grid(row=0, column=1, sticky="w", padx=4)

        self.before_canvas = tk.Label(img_frame, bg="#1e1e1e", cursor="crosshair")
        self.before_canvas.grid(row=1, column=0, padx=4)
        self.after_canvas = tk.Label(img_frame, bg="#1e1e1e", cursor="crosshair")
        self.after_canvas.grid(row=1, column=1, padx=4)

        # ── sliders ──────────────────────────────────────────────────
        slider_frame = ttk.LabelFrame(self.root, text="Filter parameters")
        slider_frame.pack(side=tk.TOP, fill=tk.X, padx=8, pady=4)

        self.slider_vars: dict[str, tk.Variable] = {}

        for row, (key, label, lo, hi, step, is_int) in enumerate(PARAM_SPECS):
            var = tk.IntVar(value=DEFAULTS[key]) if is_int \
                else tk.DoubleVar(value=DEFAULTS[key])
            self.slider_vars[key] = var

            tk.Label(slider_frame, text=label, width=22, anchor="w")\
                .grid(row=row, column=0, padx=(8, 4), pady=2, sticky="w")

            val_label = tk.Label(slider_frame, width=5, anchor="e")
            val_label.grid(row=row, column=2, padx=(4, 8))

            def _update(v, vl=val_label, vr=var, is_i=is_int, k=key):
                vl.config(text=str(int(vr.get())) if is_i else f"{vr.get():.2f}")
                self._reprocess()

            scale = ttk.Scale(slider_frame, from_=lo, to=hi,
                              variable=var, orient=tk.HORIZONTAL,
                              command=_update, length=340)
            scale.grid(row=row, column=1, padx=4, pady=2, sticky="ew")
            val_label.config(text=str(DEFAULTS[key]) if is_int
                             else f"{DEFAULTS[key]:.2f}")

        slider_frame.columnconfigure(1, weight=1)

        # ── buttons ──────────────────────────────────────────────────
        btn_frame = tk.Frame(self.root)
        btn_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=8)

        tk.Button(btn_frame, text="◀  Prev",  width=10, command=self._prev)\
            .pack(side=tk.LEFT,  padx=4)
        tk.Button(btn_frame, text="Skip  ▶",  width=10, command=self._skip)\
            .pack(side=tk.LEFT,  padx=4)
        tk.Button(btn_frame, text="Revert",   width=10, command=self._revert,
                  fg="#a05000")\
            .pack(side=tk.LEFT,  padx=4)
        tk.Button(btn_frame, text="💾  Save",  width=12, command=self._save,
                  bg="#206020", fg="white", font=("Helvetica", 10, "bold"))\
            .pack(side=tk.RIGHT, padx=4)

        # keyboard shortcuts
        self.root.bind("<Left>",  lambda e: self._prev())
        self.root.bind("<Right>", lambda e: self._skip())
        self.root.bind("<Return>", lambda e: self._save())
        self.root.bind("r",       lambda e: self._revert())

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

        img = Image.open(path)
        if img.mode not in ("RGBA", "LA"):
            # No alpha — skip silently
            self.index += 1
            self._load_current()
            return

        self.current_image = img.convert("RGBA")
        alpha = np.array(self.current_image)[:, :, 3]
        self.original_alpha = alpha.copy()

        self._update_before_canvas()
        self._reprocess()

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

    def _save(self):
        if self.current_image is None or self._processed_alpha is None:
            return
        arr = np.array(self.current_image)
        arr[:, :, 3] = self._processed_alpha
        result = Image.fromarray(arr, mode="RGBA")
        result.save(self.files[self.index], compression="tga_rle")
        print(f"Saved: {self.files[self.index]}")
        self._advance()

    def _done(self):
        self.progress_var.set("All done!")
        self.std_var.set("")
        self.root.after(1500, self.root.destroy)

    # ------------------------------------------------------------------
    # Processing & display
    # ------------------------------------------------------------------

    _processed_alpha: np.ndarray | None = None
    _reprocess_pending: bool = False

    def _reprocess(self, *_):
        # Debounce: coalesce rapid slider drags into one call
        if self._reprocess_pending:
            return
        self._reprocess_pending = True
        self.root.after(30, self._do_reprocess)

    def _do_reprocess(self):
        self._reprocess_pending = False
        if self.original_alpha is None:
            return

        def iv(k): return int(self.slider_vars[k].get())
        def fv(k): return float(self.slider_vars[k].get())

        # Median kernel must be odd and >= 1
        median = iv("median")
        if median > 0 and median % 2 == 0:
            median += 1

        self._processed_alpha = process_heightmap(
            self.original_alpha,
            blur_size       = median,
            bilateral_d     = iv("bilateral_d"),
            bilateral_sigma = iv("bilateral_sigma"),
            sharpen_sigma   = fv("sharpen_sigma"),
            sharpen_amount  = fv("sharpen_amount"),
            sharpen2_sigma  = fv("sharpen2_sigma"),
            sharpen2_amount = fv("sharpen2_amount"),
        )

        orig_std = float(np.std(self.original_alpha))
        proc_std = float(np.std(self._processed_alpha))
        self.std_var.set(f"std  {orig_std:.1f} → {proc_std:.1f}")

        self._update_after_canvas()

    def _alpha_to_display(self, alpha: np.ndarray) -> ImageTk.PhotoImage:
        """Auto-level an alpha array and return a PhotoImage scaled for display."""
        lo, hi = int(alpha.min()), int(alpha.max())
        if hi > lo:
            scaled = ((alpha.astype(np.float32) - lo) / (hi - lo) * 255)\
                       .clip(0, 255).astype(np.uint8)
        else:
            scaled = alpha.copy()

        img = Image.fromarray(scaled, mode="L").convert("RGB")

        # Scale down to MAX_DISPLAY if needed
        w, h = img.size
        if max(w, h) > MAX_DISPLAY:
            scale = MAX_DISPLAY / max(w, h)
            img = img.resize((int(w * scale), int(h * scale)), Image.LANCZOS)

        return ImageTk.PhotoImage(img)

    def _update_before_canvas(self):
        if self.original_alpha is None:
            return
        self._before_photo = self._alpha_to_display(self.original_alpha)
        self.before_canvas.config(image=self._before_photo)

    def _update_after_canvas(self):
        if self._processed_alpha is None:
            return
        self._after_photo = self._alpha_to_display(self._processed_alpha)
        self.after_canvas.config(image=self._after_photo)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Interactive heightmap review and filtering tool"
    )
    parser.add_argument("directory", help="Directory to search for normalmap files")
    parser.add_argument("--pattern", default="*_norm.tga",
                        help="Glob pattern (default: *_norm.tga)")
    args = parser.parse_args()

    directory = Path(args.directory).expanduser()
    files = sorted(directory.rglob(args.pattern))
    if not files:
        print(f"No files matching '{args.pattern}' found in {directory}")
        sys.exit(1)

    print(f"Found {len(files)} files.")

    root = tk.Tk()
    app = ReviewApp(root, files)
    root.mainloop()


if __name__ == "__main__":
    main()
