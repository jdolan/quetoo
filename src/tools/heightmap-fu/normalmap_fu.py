#!/usr/bin/env python3
"""
Interactive normalmap editor with heightmap generation.

Browse normal maps (*_nm, *_n, *_local, *_bump), attenuate normals,
generate heightmaps via Frankot-Chellappa, and save back with the
heightmap embedded in the alpha channel.

Usage:
    python normalmap_fu.py
    python normalmap_fu.py /path/to/normalmap.tga
    python normalmap_fu.py --directory /path/to/textures/
"""

import fnmatch
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# ---------------------------------------------------------------------------
# Heightmap generation (Frankot-Chellappa integration)
# ---------------------------------------------------------------------------

def _normals_to_gradients(normals: np.ndarray, y_flip: bool) -> tuple[np.ndarray, np.ndarray]:
    """Convert a decoded normal map (float32, [-1,1]) to surface gradients."""
    nx = normals[:, :, 0]
    ny = normals[:, :, 1]
    nz = normals[:, :, 2]

    nz = np.clip(nz, 0.01, None)

    p = -nx / nz  # dh/dx
    q = -ny / nz  # dh/dy

    if y_flip:
        q = -q

    return p, q


def frankot_chellappa(p: np.ndarray, q: np.ndarray) -> np.ndarray:
    """
    Integrate surface gradients into a height field using the
    Frankot-Chellappa algorithm (frequency-domain Poisson solver).
    """
    h, w = p.shape
    P = np.fft.fft2(p)
    Q = np.fft.fft2(q)

    u = np.fft.fftfreq(w) * 2 * np.pi
    v = np.fft.fftfreq(h) * 2 * np.pi
    uu, vv = np.meshgrid(u, v)

    denom = uu ** 2 + vv ** 2
    denom[0, 0] = 1.0

    H = (1j * uu * P + 1j * vv * Q) / denom
    H[0, 0] = 0.0

    height = np.real(np.fft.ifft2(H))
    return height.astype(np.float32)


def generate_heightmap(
    normal_img: np.ndarray,
    strength: float = 1.0,
    blur: int = 0,
    detail_cutoff: float = 0.0,
    y_flip: bool = False,
    invert: bool = False,
) -> np.ndarray:
    """
    Generate a heightmap from an RGB normal map image.

    Parameters
    ----------
    normal_img : uint8 HxWx3 array (RGB normal map)
    strength   : height amplitude multiplier
    blur       : post-integration Gaussian blur radius (0 = off)
    detail_cutoff : low-pass frequency cutoff, 0–1.  0 = keep all detail,
                    1 = extremely smooth.  Applies a Gaussian low-pass in
                    frequency domain before integration.
    y_flip     : flip the Y component (DirectX → OpenGL convention)
    invert     : invert the final heightmap

    Returns
    -------
    uint8 HxW heightmap, 0–255
    """
    normals = normal_img.astype(np.float32) / 127.5 - 1.0

    p, q = _normals_to_gradients(normals, y_flip)

    # Optional frequency-domain low-pass on the gradients
    if detail_cutoff > 0.01:
        sigma = detail_cutoff * min(p.shape) * 0.1
        if sigma >= 0.5:
            p = cv2.GaussianBlur(p, (0, 0), sigma)
            q = cv2.GaussianBlur(q, (0, 0), sigma)

    height = frankot_chellappa(p, q)

    # Scale by strength as contrast around midpoint (applied after normalize)
    lo, hi = height.min(), height.max()
    if hi - lo > 1e-6:
        height = (height - lo) / (hi - lo)
    else:
        height = np.full_like(height, 0.5)

    # Strength as contrast: 1.0 = identity, <1 = flatten, >1 = sharpen
    height = 0.5 + (height - 0.5) * strength
    height = np.clip(height, 0.0, 1.0)

    # Post-integration Gaussian blur
    if blur > 0:
        ksize = blur * 2 + 1
        height = cv2.GaussianBlur(height, (ksize, ksize), 0)

    height *= 255.0

    if invert:
        height = 255.0 - height

    return np.clip(height, 0, 255).astype(np.uint8)


def attenuate_normals(normal_img: np.ndarray, strength: float) -> np.ndarray:
    """
    Scale the XY components of a tangent-space normal map and renormalize.

    strength < 1.0 reduces surface detail (normals become more vertical).
    strength > 1.0 exaggerates surface detail.
    strength == 1.0 returns a copy unchanged.
    """
    if abs(strength - 1.0) < 0.001:
        return normal_img.copy()

    normals = normal_img.astype(np.float64) / 127.5 - 1.0
    nx = normals[:, :, 0] * strength
    ny = normals[:, :, 1] * strength

    # Clamp XY magnitude so we can reconstruct Z
    xy_sq = nx * nx + ny * ny
    too_large = xy_sq > 0.999
    scale = np.where(too_large, np.sqrt(0.999 / np.maximum(xy_sq, 1e-10)), 1.0)
    nx *= scale
    ny *= scale

    nz = np.sqrt(np.maximum(1.0 - nx * nx - ny * ny, 0.0))

    result = np.stack([nx, ny, nz], axis=2)
    return np.clip((result + 1.0) * 127.5, 0, 255).astype(np.uint8)


def normals_from_diffuse(diffuse_img: np.ndarray, strength: float = 1.0,
                         blur: int = 1) -> tuple[np.ndarray, np.ndarray]:
    """
    Generate a normalmap and heightmap from a diffuse texture.

    Converts to luminance, applies optional blur, then computes Sobel
    gradients to produce tangent-space normals.

    Returns (normals_uint8_HxWx3, heightmap_uint8_HxW).
    """
    # Luminance
    gray = np.dot(diffuse_img[:, :, :3].astype(np.float64),
                  [0.2126, 0.7152, 0.0722])

    # Optional pre-blur to reduce noise
    if blur > 0:
        ksize = blur * 2 + 1
        gray = cv2.GaussianBlur(gray.astype(np.float32), (ksize, ksize), 0).astype(np.float64)

    # Sobel gradients
    dx = cv2.Sobel(gray.astype(np.float32), cv2.CV_32F, 1, 0, ksize=3)
    dy = cv2.Sobel(gray.astype(np.float32), cv2.CV_32F, 0, 1, ksize=3)

    # Scale by strength
    dx *= strength
    dy *= strength

    # Build normal vectors: (-dx, -dy, 1) then normalize
    nz = np.ones_like(dx)
    length = np.sqrt(dx * dx + dy * dy + nz * nz)
    nx = -dx / length
    ny = -dy / length
    nz = nz / length

    normals = np.stack([nx, ny, nz], axis=2)
    normals_u8 = np.clip((normals + 1.0) * 127.5, 0, 255).astype(np.uint8)

    # Heightmap from luminance, normalized to 0-255
    lo, hi = gray.min(), gray.max()
    if hi - lo > 1e-6:
        heightmap = ((gray - lo) / (hi - lo) * 255.0)
    else:
        heightmap = np.full_like(gray, 128.0)
    heightmap_u8 = np.clip(heightmap, 0, 255).astype(np.uint8)

    return normals_u8, heightmap_u8


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

MAX_DISPLAY = 196
THUMB_SIZE = 96
NORMAL_SUFFIXES = ("_nm", "_norm", "_n", "_local", "_bump")
DIFFUSE_SUFFIXES = ("_d", "")

PARAM_SPECS = [
    # (key,              label,                 lo,   hi,   default,  step,  is_int)
    ("normal_strength",  "Normal strength",    0.1,   2.0,     1.0,  0.01,  False),
    ("strength",         "Height strength",    0.1,  10.0,     1.0,   0.1,  False),
    ("blur",             "Post blur",            0,    30,       0,     1,   True),
    ("detail_cutoff",    "Detail cutoff",      0.0,   1.0,     0.0,  0.01,  False),
]


def _is_normal_map(path: Path) -> bool:
    stem = path.stem.lower()
    return any(stem.endswith(s) for s in NORMAL_SUFFIXES)


def _strip_suffix(stem: str, suffixes: tuple[str, ...]) -> str | None:
    """Strip the first matching suffix, returning the base. None if no match."""
    lower = stem.lower()
    for s in suffixes:
        if not s:
            continue
        if lower.endswith(s):
            return stem[:len(stem) - len(s)]
    return None


def _find_counterpart(path: Path) -> Path | None:
    """Given a diffuse, find its normalmap; given a normalmap, find its diffuse."""
    stem = path.stem
    parent = path.parent

    if _is_normal_map(path):
        base = _strip_suffix(stem, NORMAL_SUFFIXES)
        if base is None:
            return None
        for dsuf in DIFFUSE_SUFFIXES:
            for ext in (".tga", ".png"):
                candidate = parent / (base + dsuf + ext)
                if candidate.exists():
                    return candidate
    else:
        # Strip optional _d suffix to get the base
        base = _strip_suffix(stem, ("_d",))
        if base is None:
            base = stem
        for nsuf in NORMAL_SUFFIXES:
            for ext in (".tga", ".png"):
                candidate = parent / (base + nsuf + ext)
                if candidate.exists():
                    return candidate
    return None

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

class HeightmapApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Normalmap-fu")
        self.root.resizable(True, True)
        self.root.configure(bg="#2b2b2b")
        self.root.geometry("1600x900")
        self.root.minsize(1000, 700)

        self.diffuse_array: np.ndarray | None = None   # uint8 HxWx3
        self.normal_array: np.ndarray | None = None    # uint8 HxWx3
        self.attenuated_normals: np.ndarray | None = None  # uint8 HxWx3
        self.heightmap: np.ndarray | None = None       # uint8 HxW
        self.existing_alpha: np.ndarray | None = None  # uint8 HxW
        self.source_path: Path | None = None

        self._build_ui()
        self._poll_directory()

    # ------------------------------------------------------------------
    # UI
    # ------------------------------------------------------------------

    def _build_ui(self):
        # ── vertical paned window ───────────────────────────────────
        paned = tk.PanedWindow(self.root, orient=tk.VERTICAL, bg="#2b2b2b",
                               sashwidth=6, sashrelief=tk.RAISED)
        paned.pack(fill=tk.BOTH, expand=True)

        # ── top pane: previews + controls ───────────────────────────
        top_pane = tk.Frame(paned, bg="#2b2b2b")
        paned.add(top_pane, stretch="always")

        # file info
        top = tk.Frame(top_pane, bg="#2b2b2b")
        top.pack(side=tk.TOP, fill=tk.X, padx=8, pady=(8, 0))

        self.file_var = tk.StringVar(master=self.root, value="Select a normal map to begin")
        tk.Label(top, textvariable=self.file_var, anchor="w", bg="#2b2b2b",
                 fg="#cccccc", font=("Helvetica", 11, "bold")).pack(side=tk.LEFT)

        self.stats_var = tk.StringVar(master=self.root)
        tk.Label(top, textvariable=self.stats_var, anchor="e", bg="#2b2b2b",
                 fg="#888888", font=("Helvetica", 10)).pack(side=tk.RIGHT)

        # buttons bar (pack from BOTTOM first so it's never clipped)
        btn_frame = tk.Frame(top_pane, bg="#2b2b2b")
        btn_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=(4, 4))

        tk.Button(btn_frame, text="Set directory...", command=self._browse_directory)\
            .pack(side=tk.LEFT, padx=4)

        self.dir_var = tk.StringVar(master=self.root, value="(no directory)")
        tk.Label(btn_frame, textvariable=self.dir_var, anchor="w", bg="#2b2b2b",
                 fg="#999999", font=("Helvetica", 10)).pack(side=tk.LEFT, padx=4)

        # filename filter (shell-style globs)
        tk.Label(btn_frame, text="Filter:", bg="#2b2b2b",
                 fg="#cccccc", font=("Helvetica", 10)).pack(side=tk.LEFT, padx=(12, 2))
        self.filter_var = tk.StringVar(master=self.root, value="")
        self._filter_entry = tk.Entry(btn_frame, textvariable=self.filter_var,
                                      width=20, font=("Helvetica", 10))
        self._filter_entry.pack(side=tk.LEFT, padx=2)
        self.filter_var.trace_add("write", lambda *_: self._apply_filter())

        tk.Button(btn_frame, text="Save", width=16,
                  command=self._save)\
            .pack(side=tk.RIGHT, padx=4)

        tk.Button(btn_frame, text="Generate from diffuse",
                  command=self._generate_from_diffuse)\
            .pack(side=tk.RIGHT, padx=4)

        self.browser_count_var = tk.StringVar(master=self.root)
        tk.Label(btn_frame, textvariable=self.browser_count_var, anchor="e",
                 bg="#2b2b2b", fg="#888888", font=("Helvetica", 10))\
            .pack(side=tk.RIGHT, padx=4)

        # sliders (pack from BOTTOM, above buttons)
        slider_frame = ttk.LabelFrame(top_pane, text="Parameters")
        slider_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=4)

        self.slider_vars: dict[str, tk.Variable] = {}

        for row, (key, label, lo, hi, default, step, is_int) in enumerate(PARAM_SPECS):
            var = tk.IntVar(master=self.root, value=default) if is_int \
                else tk.DoubleVar(master=self.root, value=default)
            self.slider_vars[key] = var

            tk.Label(slider_frame, text=label, width=16, anchor="w")\
                .grid(row=row, column=0, padx=(8, 4), pady=2, sticky="w")

            val_label = tk.Label(slider_frame, width=6, anchor="e")
            val_label.grid(row=row, column=2, padx=(4, 8))

            def _update(v, vl=val_label, vr=var, is_i=is_int):
                vl.config(text=str(int(vr.get())) if is_i else f"{vr.get():.2f}")
                self._reprocess()

            scale = ttk.Scale(slider_frame, from_=lo, to=hi,
                              variable=var, orient=tk.HORIZONTAL,
                              command=_update, length=380)
            scale.grid(row=row, column=1, padx=4, pady=2, sticky="ew")

            fmt_val = str(int(default)) if is_int else f"{default:.2f}"
            val_label.config(text=fmt_val)

        toggle_row = len(PARAM_SPECS)

        self.y_flip_var = tk.BooleanVar(master=self.root, value=False)
        ttk.Checkbutton(slider_frame, text="Flip Y (DirectX normals)",
                        variable=self.y_flip_var,
                        command=self._reprocess)\
            .grid(row=toggle_row, column=0, columnspan=2, padx=8, pady=4, sticky="w")

        self.invert_var = tk.BooleanVar(master=self.root, value=False)
        ttk.Checkbutton(slider_frame, text="Invert height",
                        variable=self.invert_var,
                        command=self._reprocess)\
            .grid(row=toggle_row, column=1, columnspan=2, padx=8, pady=4, sticky="e")

        slider_frame.columnconfigure(1, weight=1)

        # image panels (pack last — fills remaining space, shrinks first)
        img_frame = tk.Frame(top_pane, bg="#1e1e1e")
        img_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=8, pady=8)

        # Diffuse column
        tk.Label(img_frame, text="DIFFUSE", bg="#1e1e1e",
                 fg="#cccc50", font=("Helvetica", 10, "bold"))\
            .grid(row=0, column=0, sticky="w", padx=4)

        self.diffuse_canvas = tk.Label(
            img_frame, bg="#1e1e1e",
            text="",
            fg="#555555", font=("Helvetica", 14),
            relief="sunken", borderwidth=2,
        )
        self.diffuse_canvas.grid(row=1, column=0, padx=4, sticky="nsew")

        # Normal map column
        tk.Label(img_frame, text="NORMAL MAP", bg="#1e1e1e",
                 fg="#5090dc", font=("Helvetica", 10, "bold"))\
            .grid(row=0, column=1, sticky="w", padx=4)

        self.normal_canvas = tk.Label(
            img_frame, bg="#1e1e1e", cursor="hand2",
            text="select from browser below",
            fg="#555555", font=("Helvetica", 14),
            relief="sunken", borderwidth=2,
        )
        self.normal_canvas.grid(row=1, column=1, padx=4, sticky="nsew")
        self.normal_canvas.bind("<Button-1>", lambda e: self._browse())

        # Existing alpha heightmap column
        tk.Label(img_frame, text="EXISTING HEIGHT", bg="#1e1e1e",
                 fg="#dc9050", font=("Helvetica", 10, "bold"))\
            .grid(row=0, column=2, sticky="w", padx=4)

        self.alpha_canvas = tk.Label(
            img_frame, bg="#1e1e1e",
            text="",
            fg="#555555", font=("Helvetica", 14),
            relief="sunken", borderwidth=2,
        )
        self.alpha_canvas.grid(row=1, column=2, padx=4, sticky="nsew")

        # Generated heightmap column
        tk.Label(img_frame, text="GENERATED HEIGHT", bg="#1e1e1e",
                 fg="#50dc50", font=("Helvetica", 10, "bold"))\
            .grid(row=0, column=3, sticky="w", padx=4)

        self.height_canvas = tk.Label(
            img_frame, bg="#1e1e1e",
            text="",
            fg="#555555", font=("Helvetica", 14),
            relief="sunken", borderwidth=2,
        )
        self.height_canvas.grid(row=1, column=3, padx=4, sticky="nsew")

        img_frame.columnconfigure(0, weight=1)
        img_frame.columnconfigure(1, weight=1)
        img_frame.columnconfigure(2, weight=1)
        img_frame.columnconfigure(3, weight=1)
        img_frame.rowconfigure(1, weight=1)

        # ── bottom pane: texture browser ────────────────────────────
        browser_pane = tk.Frame(paned, bg="#2b2b2b")
        paned.add(browser_pane, stretch="always", minsize=120)

        self._build_browser(browser_pane)

        # keyboard shortcuts
        self.root.bind("<s>", lambda e: self._save())

    # ------------------------------------------------------------------
    # Texture browser
    # ------------------------------------------------------------------

    def _build_browser(self, parent: tk.Frame):
        # scrollable canvas for thumbnails
        browser_frame = tk.Frame(parent, bg="#1e1e1e")
        browser_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=4, pady=4)

        self.browser_canvas = tk.Canvas(browser_frame, bg="#1e1e1e",
                                        highlightthickness=0)
        self.browser_scrollbar = ttk.Scrollbar(browser_frame, orient=tk.VERTICAL,
                                               command=self.browser_canvas.yview)
        self.browser_canvas.configure(yscrollcommand=self.browser_scrollbar.set)

        self.browser_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.browser_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.thumb_frame = tk.Frame(self.browser_canvas, bg="#1e1e1e")
        self._thumb_window = self.browser_canvas.create_window(
            (0, 0), window=self.thumb_frame, anchor="nw")

        self.thumb_frame.bind("<Configure>",
                              lambda e: self.browser_canvas.configure(
                                  scrollregion=self.browser_canvas.bbox("all")))

        # reflow grid on canvas resize
        self.browser_canvas.bind("<Configure>", self._on_browser_resize)

        # mousewheel scrolling (vertical)
        def _on_mousewheel(event):
            self.browser_canvas.yview_scroll(-1 * event.delta, "units")
        self.browser_canvas.bind("<MouseWheel>", _on_mousewheel)
        self.thumb_frame.bind("<MouseWheel>", _on_mousewheel)

        # state
        self.browser_dir: Path | None = None
        self._all_browser_files: list[Path] = []  # unfiltered
        self.browser_files: list[Path] = []  # filtered (displayed)
        self._thumb_photos: list[ImageTk.PhotoImage] = []  # prevent GC
        self._thumb_labels: list[tk.Label] = []
        self._thumb_path_map: dict[Path, tk.Label] = {}  # path -> label
        self._thumb_load_index = 0
        self._selected_thumb: tk.Label | None = None
        self._browser_cols = 1

    def _on_browser_resize(self, event):
        # match inner frame width to canvas width
        self.browser_canvas.itemconfigure(self._thumb_window, width=event.width)
        # reflow thumbnails into new column count
        pad = 2 * 2 + 4  # label borderwidth + grid padx
        cols = max(1, event.width // (THUMB_SIZE + pad))
        if cols != self._browser_cols:
            self._browser_cols = cols
            self._reflow_thumbs()

    def _filter_files(self, files: list[Path]) -> list[Path]:
        """Return files matching the current glob filter (applied to filename)."""
        pattern = self.filter_var.get().strip()
        if not pattern:
            return list(files)
        # match against filename (case-insensitive)
        return [f for f in files if fnmatch.fnmatch(f.name.lower(), pattern.lower())]

    def _apply_filter(self):
        """Re-filter displayed thumbnails when the filter text changes."""
        if self.browser_dir is None:
            return
        self._incremental_update()

    def _browse_directory(self):
        path = filedialog.askdirectory(title="Select texture directory")
        if not path:
            return
        self._set_directory(Path(path))

    def _set_directory(self, directory: Path):
        self.browser_dir = directory
        self.dir_var.set(str(directory))

        # scan for all textures
        files = []
        for ext in ("*.tga", "*.png"):
            files.extend(directory.rglob(ext))
        self._all_browser_files = sorted(files)

        # apply current filter
        self.browser_files = self._filter_files(self._all_browser_files)

        self.browser_count_var.set(f"{len(self.browser_files)} textures")
        print(f"Browser: {len(self.browser_files)} textures in {directory}")

        # clear existing thumbnails
        for lbl in self._thumb_labels:
            lbl.destroy()
        self._thumb_labels.clear()
        self._thumb_photos.clear()
        self._thumb_path_map.clear()
        self._selected_thumb = None
        self._thumb_load_index = 0

        # start lazy loading
        if self.browser_files:
            self._load_thumb_batch()

    def _load_thumb_batch(self, batch_size: int = 20):
        """Load a batch of thumbnails, then schedule the next batch."""
        end = min(self._thumb_load_index + batch_size, len(self.browser_files))

        for i in range(self._thumb_load_index, end):
            path = self.browser_files[i]
            self._create_thumbnail(i, path)

        self._thumb_load_index = end

        if self._thumb_load_index < len(self.browser_files):
            self.root.after(10, self._load_thumb_batch)

    def _create_thumbnail(self, index: int, path: Path):
        try:
            img = Image.open(path)
            img.load()
            has_alpha_height = False
            if img.mode in ("RGBA", "LA", "PA"):
                alpha = np.array(img.split()[-1])
                has_alpha_height = alpha.min() < 250 and np.std(alpha) >= 2.0
            img = img.convert("RGB")
            img.thumbnail((THUMB_SIZE, THUMB_SIZE), Image.LANCZOS)
            photo = ImageTk.PhotoImage(img, master=self.root)
        except Exception:
            return

        self._thumb_photos.append(photo)

        needs_height = _is_normal_map(path) and not has_alpha_height

        border_color = "#cc3333" if needs_height else "#1e1e1e"
        pad = 3 if needs_height else 0
        lbl = tk.Label(self.thumb_frame, image=photo, bg=border_color,
                       relief="flat", borderwidth=0, cursor="hand2",
                       padx=pad, pady=pad,
                       highlightthickness=3 if needs_height else 0,
                       highlightbackground="#cc3333" if needs_height else "#1e1e1e")
        lbl._default_bg = border_color
        lbl._default_highlight = "#cc3333" if needs_height else "#1e1e1e"
        lbl._needs_height = needs_height

        cols = self._browser_cols
        row, col = divmod(len(self._thumb_labels), cols)
        lbl.grid(row=row, column=col, padx=2, pady=2)

        lbl.bind("<Button-1>", lambda e, p=path, l=lbl: self._on_thumb_click(p, l))

        # tooltip on hover
        name = path.stem
        lbl.bind("<Enter>", lambda e, n=name: self.browser_count_var.set(n))
        lbl.bind("<Leave>", lambda e: self.browser_count_var.set(
            f"{len(self.browser_files)} textures"))

        # forward mousewheel to canvas
        lbl.bind("<MouseWheel>", lambda e: self.browser_canvas.yview_scroll(
            -1 * e.delta, "units"))

        self._thumb_labels.append(lbl)
        self._thumb_path_map[path] = lbl

    def _reflow_thumbs(self):
        """Re-grid all thumbnail labels into the current column count."""
        cols = self._browser_cols
        for i, lbl in enumerate(self._thumb_labels):
            row, col = divmod(i, cols)
            lbl.grid(row=row, column=col, padx=2, pady=2)

    def _on_thumb_click(self, path: Path, label: tk.Label):
        # deselect previous — restore its default style
        if self._selected_thumb is not None:
            prev = self._selected_thumb
            prev.config(highlightbackground=prev._default_highlight,
                        highlightthickness=3 if prev._needs_height else 0)

        # highlight selected
        label.config(highlightbackground="#5090dc", highlightthickness=3)
        self._selected_thumb = label

        self._load_file(path)

    # ------------------------------------------------------------------
    # File loading
    # ------------------------------------------------------------------

    def _browse(self):
        path = filedialog.askopenfilename(
            title="Open normal map",
            filetypes=[
                ("TGA files", "*.tga"),
                ("PNG files", "*.png"),
                ("All image files", "*.tga *.png *.jpg *.jpeg *.bmp *.tif *.tiff"),
                ("All files", "*"),
            ],
        )
        if path:
            self._load_file(Path(path))

    def _load_file(self, path: Path):
        """Load a texture pair. Clicking either diffuse or normalmap loads both."""
        if _is_normal_map(path):
            normal_path = path
            diffuse_path = _find_counterpart(path)
        else:
            diffuse_path = path
            normal_path = _find_counterpart(path)

        # Load diffuse
        if diffuse_path and diffuse_path.is_file():
            try:
                img = Image.open(diffuse_path)
                img.load()
                self.diffuse_array = np.array(img.convert("RGB"))
            except Exception:
                self.diffuse_array = None
        else:
            self.diffuse_array = None
        self._update_diffuse_canvas()

        # Load normalmap
        if normal_path and normal_path.is_file():
            try:
                img = Image.open(normal_path)
                img.load()
            except Exception as exc:
                import traceback
                traceback.print_exc()
                messagebox.showerror("Error", f"Failed to open image:\n{exc}")
                return

            rgb = img.convert("RGB")
            self.normal_array = np.array(rgb)
            self.source_path = normal_path

            # Extract existing alpha heightmap if present
            if img.mode in ("RGBA", "LA", "PA"):
                alpha = np.array(img.split()[-1])
                if alpha.min() < 250:
                    self.existing_alpha = alpha
                else:
                    self.existing_alpha = None
            else:
                self.existing_alpha = None

            h, w = self.normal_array.shape[:2]
            self.file_var.set(f"{normal_path.name}  ({w}×{h})")
            print(f"Loaded: {normal_path.name} ({w}×{h}, {img.mode})")

            self._update_alpha_canvas()
            self._reprocess()
        else:
            # No normalmap found — clear normal panels
            self.normal_array = None
            self.attenuated_normals = None
            self.heightmap = None
            self.existing_alpha = None
            self.source_path = None
            self.file_var.set(f"{path.name}  (no normalmap found)")
            self._update_normal_canvas()
            self._update_alpha_canvas()
            self._update_height_canvas()

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    _reprocess_pending: bool = False

    def _reprocess(self, *_):
        if self.normal_array is None:
            return
        if self._reprocess_pending:
            return
        self._reprocess_pending = True
        self.root.after(50, self._do_reprocess)

    def _do_reprocess(self):
        self._reprocess_pending = False
        if self.normal_array is None:
            return

        def fv(k): return float(self.slider_vars[k].get())
        def iv(k): return int(self.slider_vars[k].get())

        # Apply normal attenuation
        ns = fv("normal_strength")
        self.attenuated_normals = attenuate_normals(self.normal_array, ns)

        # Generate heightmap from (attenuated) normals
        self.heightmap = generate_heightmap(
            self.attenuated_normals,
            strength=fv("strength"),
            blur=iv("blur"),
            detail_cutoff=fv("detail_cutoff"),
            y_flip=self.y_flip_var.get(),
            invert=self.invert_var.get(),
        )

        std = float(np.std(self.heightmap))
        self.stats_var.set(f"height std: {std:.1f}")

        self._update_normal_canvas()
        self._update_height_canvas()

    def _generate_from_diffuse(self):
        """Generate a normalmap and heightmap from the loaded diffuse texture."""
        if self.diffuse_array is None:
            messagebox.showinfo("Info", "No diffuse texture loaded.")
            return

        ns = float(self.slider_vars["normal_strength"].get())
        normals, heightmap = normals_from_diffuse(self.diffuse_array, strength=ns)

        self.normal_array = normals
        self.attenuated_normals = normals
        self.heightmap = heightmap
        self.existing_alpha = None

        h, w = normals.shape[:2]
        name = self.source_path.name if self.source_path else "generated"
        self.file_var.set(f"{name}  ({w}×{h})  [from diffuse]")

        self._update_normal_canvas()
        self._update_alpha_canvas()
        self._update_height_canvas()

    # ------------------------------------------------------------------
    # Display
    # ------------------------------------------------------------------

    def _to_photoimage(self, img: Image.Image) -> ImageTk.PhotoImage:
        w, h = img.size
        scale = MAX_DISPLAY / max(w, h)
        if scale != 1.0:
            img = img.resize((int(w * scale), int(h * scale)), Image.LANCZOS)
        return ImageTk.PhotoImage(img, master=self.root)

    def _update_diffuse_canvas(self):
        if self.diffuse_array is not None:
            img = Image.fromarray(self.diffuse_array, mode="RGB")
            self._diffuse_photo = self._to_photoimage(img)
            self.diffuse_canvas.config(image=self._diffuse_photo, text="")
        else:
            self._diffuse_photo = None
            self.diffuse_canvas.config(image="", text="no diffuse")

    def _update_normal_canvas(self):
        arr = self.attenuated_normals if self.attenuated_normals is not None \
            else self.normal_array
        if arr is None:
            return
        img = Image.fromarray(arr, mode="RGB")
        self._normal_photo = self._to_photoimage(img)
        self.normal_canvas.config(image=self._normal_photo, text="")

    def _update_alpha_canvas(self):
        if self.existing_alpha is not None:
            img = Image.fromarray(self.existing_alpha, mode="L").convert("RGB")
            self._alpha_photo = self._to_photoimage(img)
            self.alpha_canvas.config(image=self._alpha_photo, text="")
        else:
            self._alpha_photo = None
            self.alpha_canvas.config(image="", text="no alpha")

    def _update_height_canvas(self):
        if self.heightmap is None:
            return
        img = Image.fromarray(self.heightmap, mode="L").convert("RGB")
        self._height_photo = self._to_photoimage(img)
        self.height_canvas.config(image=self._height_photo, text="")

    # ------------------------------------------------------------------
    # Saving
    # ------------------------------------------------------------------

    def _save(self):
        """Save attenuated normals + heightmap alpha back to the source file."""
        if self.heightmap is None or self.source_path is None:
            messagebox.showinfo("Info", "No heightmap to save.")
            return

        normals = self.attenuated_normals if self.attenuated_normals is not None \
            else self.normal_array
        if normals is None:
            return

        h, w = normals.shape[:2]
        rgba = np.zeros((h, w, 4), dtype=np.uint8)
        rgba[:, :, :3] = normals
        rgba[:, :, 3] = self.heightmap

        result = Image.fromarray(rgba, mode="RGBA")

        path = self.source_path
        if str(path).lower().endswith(".tga"):
            result.save(path, compression="tga_rle")
        else:
            result.save(path)
        print(f"Saved: {path.name}")

        # Update existing alpha preview
        self.existing_alpha = self.heightmap.copy()
        self._update_alpha_canvas()

        self._refresh_browser_if_needed(path)

    def _refresh_browser_if_needed(self, saved_path: Path):
        """Refresh the thumbnail for a saved file to update its status."""
        lbl = self._thumb_path_map.get(saved_path)
        if lbl is None:
            return

        # Re-check if it still needs a heightmap
        try:
            img = Image.open(saved_path)
            img.load()
            has_alpha_height = False
            if img.mode in ("RGBA", "LA", "PA"):
                alpha = np.array(img.split()[-1])
                has_alpha_height = alpha.min() < 250 and np.std(alpha) >= 2.0
        except Exception:
            return

        needs_height = _is_normal_map(saved_path) and not has_alpha_height
        lbl._needs_height = needs_height

        border_color = "#cc3333" if needs_height else "#1e1e1e"
        lbl._default_bg = border_color
        lbl._default_highlight = "#cc3333" if needs_height else "#1e1e1e"
        lbl.config(bg=border_color, padx=3 if needs_height else 0,
                   pady=3 if needs_height else 0)

        # Keep selection highlight if this is the selected thumb
        if lbl is self._selected_thumb:
            lbl.config(highlightbackground="#5090dc", highlightthickness=3)
        else:
            lbl.config(highlightbackground=lbl._default_highlight,
                       highlightthickness=3 if needs_height else 0)

    def _poll_directory(self):
        """Periodically check if the browser directory contents have changed."""
        if self.browser_dir is not None and self.browser_dir.is_dir():
            files = []
            for ext in ("*.tga", "*.png"):
                files.extend(self.browser_dir.rglob(ext))
            files = sorted(files)
            if files != self._all_browser_files:
                self._all_browser_files = files
                self._incremental_update(self._filter_files(files))
        self.root.after(1000, self._poll_directory)

    def _incremental_update(self, new_files: list[Path] | None = None):
        """Add/remove only the thumbnails that changed."""
        if new_files is None:
            new_files = self._filter_files(self._all_browser_files)

        old_set = set(self.browser_files)
        new_set = set(new_files)

        added = new_set - old_set
        removed = old_set - new_set

        # Remove deleted files
        for path in removed:
            lbl = self._thumb_path_map.pop(path, None)
            if lbl is not None:
                if lbl is self._selected_thumb:
                    self._selected_thumb = None
                lbl.destroy()
                self._thumb_labels.remove(lbl)

        self.browser_files = new_files
        self.browser_count_var.set(f"{len(self.browser_files)} textures")

        # Add new files
        for path in sorted(added):
            self._create_thumbnail(len(self._thumb_labels), path)

        # Reflow if anything changed
        if added or removed:
            self._reflow_thumbs()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    root = tk.Tk()

    app = HeightmapApp(root)

    # Parse arguments: files and --directory
    args = sys.argv[1:]
    directory = None
    files = []

    i = 0
    while i < len(args):
        if args[i] in ("--directory", "-d") and i + 1 < len(args):
            directory = Path(args[i + 1]).expanduser()
            i += 2
        else:
            files.append(Path(args[i]).expanduser())
            i += 1

    # Set browser directory if specified, or from first file's parent
    if directory and directory.is_dir():
        root.after(100, lambda: app._set_directory(directory))
    elif files and files[0].is_file():
        root.after(100, lambda: app._set_directory(files[0].parent))

    # Load first file if specified
    if files and files[0].is_file():
        root.after(200, lambda: app._load_file(files[0]))

    root.mainloop()


if __name__ == "__main__":
    main()
