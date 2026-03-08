#!/usr/bin/env python3
"""
quake-fu — Quake/QRP texture set visual merger.

Browse two Quake texture sets side-by-side, drag components (diffuse, normal,
gloss, luma) from either set into a work area, and preview the result in an
OpenGL viewport with a single Blinn-Phong point light.

Usage:
    python3 quake_fu.py [--qrp DIR] [--quake DIR]
"""

import argparse
import math
import os
import queue
import re
import sys
import threading
import tkinter as tk
from concurrent.futures import ThreadPoolExecutor
from tkinter import ttk
from typing import Callable, Dict, List, Optional, Tuple

import numpy as np
from PIL import Image, ImageTk

# ── OpenGL (optional, via moderngl offscreen on all platforms) ────────────────
_GL_AVAILABLE = False
try:
    import moderngl
    _GL_AVAILABLE = True
except ImportError:
    pass

# ── Default paths ─────────────────────────────────────────────────────────────
_SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
# quake-fu/../../.. → quetoo root, then ../quetoo-data (sibling repo)
_QUETOO_DIR  = os.path.normpath(os.path.join(_SCRIPT_DIR, "../../.."))
_DATA_ROOT   = os.path.normpath(os.path.join(_QUETOO_DIR, "../quetoo-data"))
_TEX_BASE = os.path.join(_DATA_ROOT, "target", "default", "textures", "common")

DEFAULT_QRP_DIR    = os.path.join(_TEX_BASE, "qrp")
DEFAULT_QUAKE_DIR  = os.path.join(_TEX_BASE, "quake")
DEFAULT_OUTPUT_DIR = os.path.join(_TEX_BASE, "quake-fu")

# ── Appearance ────────────────────────────────────────────────────────────────
THUMB_SIZE  = 120         # pixels, square
THUMB_COLS  = 4           # columns in each browser
CELL_PAD    = 6           # padding between cells
SLOT_SIZE   = 172         # work area slot (square)
BG          = "#1e1e1e"
BG2         = "#2a2a2a"
ACCENT      = "#4a9eff"
TEXT_COLOR  = "#cccccc"
SEL_COLOR   = "#4a9eff"
SLOT_EMPTY  = "#333333"

# ── Texture scanning ──────────────────────────────────────────────────────────
_SUFFIX_RE = re.compile(r'_(norm|gloss|luma|glow)$', re.IGNORECASE)


def _base_name(filename: str) -> str:
    name = os.path.splitext(filename)[0]
    return _SUFFIX_RE.sub('', name)


def _variant(filename: str) -> str:
    name = os.path.splitext(filename)[0]
    m = _SUFFIX_RE.search(name)
    return m.group(1).lower() if m else 'diffuse'


class TextureEntry:
    """One logical texture with its variant paths."""
    __slots__ = ('name', 'diffuse', 'norm', 'gloss', 'luma', 'glow')

    def __init__(self, name: str):
        self.name    = name
        self.diffuse: Optional[str] = None
        self.norm:    Optional[str] = None
        self.gloss:   Optional[str] = None
        self.luma:    Optional[str] = None
        self.glow:    Optional[str] = None

    @property
    def display_name(self) -> str:
        return os.path.basename(self.name)

    def get_path(self, role: str) -> Optional[str]:
        return getattr(self, role, None)


class TextureScanner:
    """Scan a directory and group .tga files by base name → TextureEntry."""

    def __init__(self, directory: str):
        self.directory = directory
        self.entries: Dict[str, TextureEntry] = {}
        self._scan()

    def _scan(self) -> None:
        try:
            files = sorted(f for f in os.listdir(self.directory)
                           if f.lower().endswith('.tga'))
        except (FileNotFoundError, PermissionError):
            return
        for f in files:
            bname   = _base_name(f)
            variant = _variant(f)
            if bname not in self.entries:
                self.entries[bname] = TextureEntry(bname)
            setattr(self.entries[bname], variant,
                    os.path.join(self.directory, f))

    def diffuse_entries(self) -> List[TextureEntry]:
        return sorted(
            (e for e in self.entries.values() if e.diffuse),
            key=lambda e: e.name
        )


# ── Thumbnail cache ───────────────────────────────────────────────────────────
_PLACEHOLDER: Optional[Image.Image] = None


def _placeholder_img() -> Image.Image:
    global _PLACEHOLDER
    if _PLACEHOLDER is None:
        img = Image.new('RGBA', (THUMB_SIZE, THUMB_SIZE), (40, 40, 40, 255))
        _PLACEHOLDER = img
    return _PLACEHOLDER


class ThumbnailCache:
    """Thread-safe lazy thumbnail cache (PIL images → ImageTk.PhotoImage)."""

    def __init__(self):
        self._pil: Dict[str, Image.Image] = {}
        self._tk:  Dict[str, ImageTk.PhotoImage] = {}
        self._lock = threading.Lock()

    def load_pil(self, path: str) -> Image.Image:
        with self._lock:
            if path not in self._pil:
                try:
                    img = Image.open(path).convert('RGBA')
                    img.thumbnail((THUMB_SIZE, THUMB_SIZE), Image.LANCZOS)
                    padded = Image.new('RGBA', (THUMB_SIZE, THUMB_SIZE), (0, 0, 0, 0))
                    x = (THUMB_SIZE - img.width)  // 2
                    y = (THUMB_SIZE - img.height) // 2
                    padded.paste(img, (x, y))
                    self._pil[path] = padded
                except Exception:
                    self._pil[path] = _placeholder_img().copy()
            return self._pil[path]

    def get_tk(self, path: str) -> ImageTk.PhotoImage:
        """Must be called from the main thread (TK requirement)."""
        with self._lock:
            if path not in self._tk:
                pil = self._pil.get(path, _placeholder_img())
                self._tk[path] = ImageTk.PhotoImage(pil)
            return self._tk[path]

    def preload(self, path: str) -> None:
        """Call from background thread; populates PIL cache."""
        self.load_pil(path)


# ── Drag/drop controller ──────────────────────────────────────────────────────
class DragController:
    """Manages application-wide drag and drop from browsers to work slots."""

    DRAG_THRESHOLD = 6  # pixels before drag activates

    def __init__(self, root: tk.Tk):
        self.root = root
        self._entry: Optional[TextureEntry] = None
        self._role: Optional[str] = None   # what variant is being dragged
        self._path: Optional[str] = None
        self._thumb_pil: Optional[Image.Image] = None
        self._active = False
        self._start_x = self._start_y = 0
        self._ghost: Optional[tk.Toplevel] = None
        self._ghost_img: Optional[ImageTk.PhotoImage] = None
        self._slots: List['TextureSlot'] = []

    def register_slot(self, slot: 'TextureSlot') -> None:
        self._slots.append(slot)

    # Called by TextureBrowser on mouse events ---------------------------------
    def press(self, entry: TextureEntry, role: str, path: str,
              thumb_pil: Image.Image, event: tk.Event) -> None:
        self._entry     = entry
        self._role      = role
        self._path      = path
        self._thumb_pil = thumb_pil
        self._active    = False
        self._start_x   = event.x_root
        self._start_y   = event.y_root

    def motion(self, event: tk.Event) -> None:
        if self._path is None:
            return
        dx = abs(event.x_root - self._start_x)
        dy = abs(event.y_root - self._start_y)
        if not self._active and (dx > self.DRAG_THRESHOLD or dy > self.DRAG_THRESHOLD):
            self._active = True
            self._create_ghost()
        if self._active and self._ghost:
            self._ghost.geometry(f"+{event.x_root + 12}+{event.y_root + 12}")
            self._highlight_slots(event.x_root, event.y_root)

    def release(self, event: tk.Event) -> None:
        self._destroy_ghost()
        self._clear_highlights()
        if not self._active:
            self._entry = None
            self._path  = None
            return
        # Find drop target
        widget = self.root.winfo_containing(event.x_root, event.y_root)
        target = self._find_slot(widget)
        if target is not None and self._entry is not None:
            target.accept_drop(self._entry, self._role, self._path)
        self._entry  = None
        self._path   = None
        self._active = False

    # Internal helpers ---------------------------------------------------------
    def _create_ghost(self) -> None:
        if self._thumb_pil is None:
            return
        self._ghost = tk.Toplevel(self.root)
        self._ghost.overrideredirect(True)
        self._ghost.attributes('-alpha', 0.75)
        self._ghost.attributes('-topmost', True)
        self._ghost_img = ImageTk.PhotoImage(self._thumb_pil)
        tk.Label(self._ghost, image=self._ghost_img, bg='black',
                 bd=2, relief='solid').pack()

    def _destroy_ghost(self) -> None:
        if self._ghost:
            self._ghost.destroy()
            self._ghost = None

    def _find_slot(self, widget: Optional[tk.Widget]) -> Optional['TextureSlot']:
        while widget is not None:
            if isinstance(widget, TextureSlot):
                return widget
            try:
                widget = widget.master
            except AttributeError:
                break
        return None

    def _highlight_slots(self, x_root: int, y_root: int) -> None:
        for slot in self._slots:
            try:
                sx = slot.winfo_rootx()
                sy = slot.winfo_rooty()
                sw = slot.winfo_width()
                sh = slot.winfo_height()
                over = (sx <= x_root <= sx + sw) and (sy <= y_root <= sy + sh)
                slot.set_hover(over)
            except Exception:
                pass

    def _clear_highlights(self) -> None:
        for slot in self._slots:
            try:
                slot.set_hover(False)
            except Exception:
                pass


# ── Texture slot (work area drop target) ─────────────────────────────────────
class TextureSlot(tk.Frame):
    """A single drop target in the work area (diffuse / normal / gloss / luma)."""

    ROLE_LABELS = {
        'diffuse': 'DIFFUSE',
        'norm':    'NORMAL',
        'gloss':   'GLOSS',
        'luma':    'LUMA',
    }
    ROLE_COLORS = {
        'diffuse': '#5a8a5a',
        'norm':    '#5a5a9a',
        'gloss':   '#9a9a5a',
        'luma':    '#9a6a3a',
    }

    def __init__(self, parent: tk.Widget, role: str,
                 cache: ThumbnailCache, drag: DragController,
                 on_change: Callable, **kwargs):
        super().__init__(parent, bg=BG2, bd=2, relief='groove', **kwargs)
        self.role      = role
        self.cache     = cache
        self.drag_ctrl = drag
        self.on_change = on_change
        self._entry: Optional[TextureEntry] = None
        self._path:  Optional[str] = None
        self._tk_img: Optional[ImageTk.PhotoImage] = None
        self._hover  = False

        drag.register_slot(self)
        self._build()

    def _build(self) -> None:
        color = self.ROLE_COLORS.get(self.role, '#555555')
        lbl = tk.Label(self, text=self.ROLE_LABELS.get(self.role, self.role.upper()),
                       bg=color, fg='white',
                       font=('Helvetica', 8, 'bold'), pady=2)
        lbl.pack(fill='x')

        self._canvas = tk.Canvas(self, width=SLOT_SIZE, height=SLOT_SIZE,
                                 bg=SLOT_EMPTY, highlightthickness=0)
        self._canvas.pack()

        self._name_lbl = tk.Label(self, text='(empty)', bg=BG2,
                                  fg=TEXT_COLOR, font=('Helvetica', 7),
                                  wraplength=SLOT_SIZE - 4)
        self._name_lbl.pack(fill='x', padx=2)

        # Right-click clears the slot
        for w in (self, self._canvas, lbl, self._name_lbl):
            w.bind('<Button-3>', self._on_clear)

    def accept_drop(self, entry: TextureEntry, role: str, path: str) -> None:
        self._entry = entry
        self._path  = path
        self._refresh_thumb()
        self.on_change()

    def _refresh_thumb(self) -> None:
        if self._path is None:
            self._canvas.delete('all')
            self._name_lbl.config(text='(empty)')
            return
        pil = self.cache.load_pil(self._path)
        img = pil.resize((SLOT_SIZE, SLOT_SIZE), Image.LANCZOS)
        self._tk_img = ImageTk.PhotoImage(img)
        self._canvas.delete('all')
        self._canvas.create_image(0, 0, anchor='nw', image=self._tk_img)
        self._name_lbl.config(text=os.path.basename(self._path))

    def set_hover(self, active: bool) -> None:
        if active != self._hover:
            self._hover = active
            self.config(bd=3, relief='solid',
                        highlightbackground=ACCENT if active else BG2)
            self._canvas.config(bg=ACCENT if active else SLOT_EMPTY)

    def _on_clear(self, _event: tk.Event) -> None:
        self._entry = None
        self._path  = None
        self._tk_img = None
        self._refresh_thumb()
        self.on_change()

    @property
    def assigned_path(self) -> Optional[str]:
        return self._path


# ── Work area ─────────────────────────────────────────────────────────────────
class WorkArea(tk.Frame):
    """Top bar: four texture slots spanning the full width."""

    ROLES = ('diffuse', 'norm', 'gloss', 'luma')

    def __init__(self, parent: tk.Widget, cache: ThumbnailCache,
                 drag: DragController, on_change: Callable, **kwargs):
        super().__init__(parent, bg=BG, **kwargs)
        self.slots: Dict[str, TextureSlot] = {}
        self._build(cache, drag, on_change)

    def _build(self, cache: ThumbnailCache, drag: DragController,
               on_change: Callable) -> None:
        tk.Label(self, text="WORK AREA  —  drag textures from the browsers below into slots",
                 bg=BG, fg=TEXT_COLOR, font=('Helvetica', 9)).pack(anchor='w', padx=8, pady=(4, 0))
        row = tk.Frame(self, bg=BG)
        row.pack(fill='x', padx=8, pady=4)
        for role in self.ROLES:
            slot = TextureSlot(row, role, cache, drag, on_change)
            slot.pack(side='left', padx=6)
            self.slots[role] = slot

    def get_paths(self) -> Dict[str, Optional[str]]:
        return {role: s.assigned_path for role, s in self.slots.items()}


# ── Scrollable texture browser ────────────────────────────────────────────────
class TextureBrowser(tk.Frame):
    """Scrollable 4-column grid of diffusemap thumbnails."""

    CELL_W = THUMB_SIZE + CELL_PAD * 2
    CELL_H = THUMB_SIZE + 18 + CELL_PAD * 2  # +18 for name label

    def __init__(self, parent: tk.Widget, label: str, entries: List[TextureEntry],
                 cache: ThumbnailCache, drag: DragController,
                 on_select: Callable, executor: ThreadPoolExecutor, **kwargs):
        super().__init__(parent, bg=BG, **kwargs)
        self._all_entries = entries
        self._entries     = list(entries)
        self._cache       = cache
        self._drag        = drag
        self._on_select   = on_select
        self._executor    = executor
        self._label_text  = label
        self._selected:   Optional[TextureEntry] = None
        self._peer:       Optional['TextureBrowser'] = None
        self._item_map:   Dict[int, TextureEntry] = {}
        self._entry_bg:   Dict[str, int] = {}    # entry.name → bg rect item id
        self._tk_imgs:    Dict[str, ImageTk.PhotoImage] = {}
        self._load_q:     queue.Queue = queue.Queue()
        self._pending:    Dict[str, List[int]] = {}
        self._visible_range: Tuple[int, int] = (0, 0)
        self._build(label)
        self._schedule_queue_drain()

    def set_peer(self, peer: 'TextureBrowser') -> None:
        self._peer = peer

    def scroll_to_name(self, name: str) -> None:
        """Scroll so the entry with the given base name is visible, and highlight it."""
        try:
            idx = next(i for i, e in enumerate(self._entries) if e.name == name)
        except StopIteration:
            return
        cols = THUMB_COLS
        row  = idx // cols
        total_rows = math.ceil(len(self._entries) / cols)
        frac = row / max(total_rows - 1, 1)
        self._canvas.yview_moveto(max(0.0, frac - 0.1))
        self._highlight_entry_name(name)

    def _build(self, label: str) -> None:
        self._header = tk.Label(self, text=label, bg=BG2, fg=TEXT_COLOR,
                                font=('Helvetica', 9, 'bold'), pady=4)
        self._header.pack(fill='x')

        # Search box
        search_frame = tk.Frame(self, bg=BG2)
        search_frame.pack(fill='x')
        tk.Label(search_frame, text='🔍', bg=BG2, fg=TEXT_COLOR,
                 font=('Helvetica', 10)).pack(side='left', padx=(6, 2), pady=3)
        self._search_var = tk.StringVar()
        self._search_var.trace_add('write', self._on_search_change)
        search_entry = tk.Entry(search_frame, textvariable=self._search_var,
                                bg='#3a3a3a', fg=TEXT_COLOR, insertbackground=TEXT_COLOR,
                                relief='flat', font=('Helvetica', 9), bd=4)
        search_entry.pack(side='left', fill='x', expand=True, padx=(0, 6), pady=3)

        frame = tk.Frame(self, bg=BG)
        frame.pack(fill='both', expand=True)
        frame.grid_rowconfigure(0, weight=1)
        frame.grid_columnconfigure(0, weight=1)

        self._canvas = tk.Canvas(frame, bg=BG, highlightthickness=0,
                                 width=self.CELL_W * THUMB_COLS + 20)
        scrollbar = ttk.Scrollbar(frame, orient='vertical',
                                  command=self._canvas.yview)
        self._canvas.configure(yscrollcommand=scrollbar.set)
        self._canvas.grid(row=0, column=0, sticky='nsew')
        scrollbar.grid(row=0, column=1, sticky='ns')

        self._canvas.bind('<Configure>', self._on_configure)
        self._canvas.bind('<MouseWheel>', self._on_mousewheel)
        self._canvas.bind('<Button-4>',  lambda e: self._canvas.yview_scroll(-3, 'units'))
        self._canvas.bind('<Button-5>',  lambda e: self._canvas.yview_scroll(3, 'units'))
        self._canvas.bind('<ButtonPress-1>',   self._on_press)
        self._canvas.bind('<Double-Button-1>', self._on_double_click)
        self._canvas.bind('<B1-Motion>',        self._on_drag_motion)
        self._canvas.bind('<ButtonRelease-1>',  self._on_drag_release)

        self._render_grid()

    def _on_search_change(self, *_args) -> None:
        query = self._search_var.get().strip().lower()
        if query:
            self._entries = [e for e in self._all_entries
                             if query in e.display_name.lower()]
        else:
            self._entries = list(self._all_entries)
        count = len(self._entries)
        total = len(self._all_entries)
        suffix = f'  ({count}/{total})' if query else f'  ({total})'
        self._header.config(text=self._label_text.split('  (')[0] + suffix)
        self._canvas.yview_moveto(0)
        self._render_grid()

    def _render_grid(self) -> None:
        self._canvas.delete('all')
        self._item_map.clear()
        self._entry_bg.clear()
        self._pending.clear()

        cols = THUMB_COLS
        for idx, entry in enumerate(self._entries):
            col = idx % cols
            row = idx // cols
            x = col * self.CELL_W + CELL_PAD
            y = row * self.CELL_H + CELL_PAD

            bg_id = self._canvas.create_rectangle(
                x - CELL_PAD + 2, y - CELL_PAD + 2,
                x + THUMB_SIZE + CELL_PAD - 2, y + THUMB_SIZE + 18 + CELL_PAD - 2,
                fill=BG2, outline='', tags=('cell',))
            self._item_map[bg_id] = entry
            self._entry_bg[entry.name] = bg_id

            key = entry.diffuse or ''
            cached = self._tk_imgs.get(key)
            ph = cached if cached else ImageTk.PhotoImage(_placeholder_img())
            if not cached:
                self._tk_imgs[f'__ph_{id(entry)}'] = ph
            img_id = self._canvas.create_image(x, y, anchor='nw', image=ph)
            self._item_map[img_id] = entry

            txt_id = self._canvas.create_text(
                x + THUMB_SIZE // 2, y + THUMB_SIZE + 6,
                text=entry.display_name[:22],
                fill=TEXT_COLOR, font=('Helvetica', 7),
                width=THUMB_SIZE, anchor='n')
            self._item_map[txt_id] = entry

            if not cached and key:
                self._pending.setdefault(key, []).append(img_id)

        total_rows = math.ceil(len(self._entries) / cols)
        total_h = total_rows * self.CELL_H + CELL_PAD * 2
        self._canvas.configure(scrollregion=(0, 0,
                                             cols * self.CELL_W + 20,
                                             total_h))
        # Re-apply selection highlight if the selected entry is still visible
        if self._selected:
            self._highlight_entry_name(self._selected.name)
        self._lazy_load_visible()

    def _lazy_load_visible(self) -> None:
        """Submit load jobs for thumbnails currently in the viewport."""
        try:
            top    = self._canvas.canvasy(0)
            bottom = self._canvas.canvasy(self._canvas.winfo_height())
        except Exception:
            return

        cols = THUMB_COLS
        first_row = max(0, int(top    / self.CELL_H) - 1)
        last_row  = int(bottom / self.CELL_H) + 2
        first_idx = first_row * cols
        last_idx  = min(len(self._entries) - 1, last_row * cols + cols - 1)

        for idx in range(first_idx, last_idx + 1):
            if idx >= len(self._entries):
                break
            entry = self._entries[idx]
            if not entry.diffuse:
                continue
            path = entry.diffuse
            if path in self._tk_imgs:
                continue   # already loaded
            self._executor.submit(self._bg_load, path)

    def _bg_load(self, path: str) -> None:
        self._cache.load_pil(path)
        self._load_q.put(path)

    def _schedule_queue_drain(self) -> None:
        self._drain_queue()
        self.after(80, self._schedule_queue_drain)

    def _drain_queue(self) -> None:
        processed = 0
        while not self._load_q.empty() and processed < 20:
            path = self._load_q.get_nowait()
            if path in self._tk_imgs:
                continue
            tk_img = self._cache.get_tk(path)
            self._tk_imgs[path] = tk_img
            for item_id in self._pending.get(path, []):
                try:
                    self._canvas.itemconfig(item_id, image=tk_img)
                except Exception:
                    pass
            processed += 1

    def _on_configure(self, _event: tk.Event) -> None:
        self._lazy_load_visible()

    def _on_mousewheel(self, event: tk.Event) -> None:
        self._canvas.yview_scroll(int(-1 * (event.delta / 120)), 'units')
        self._lazy_load_visible()

    # Selection ----------------------------------------------------------------
    def _highlight_entry_name(self, name: str) -> None:
        """Highlight the cell for the given base name, clear all others."""
        for n, bg_id in self._entry_bg.items():
            try:
                self._canvas.itemconfig(bg_id,
                    fill=ACCENT if n == name else BG2,
                    outline=ACCENT if n == name else '')
            except Exception:
                pass

    def _entry_at(self, x: int, y: int) -> Optional[TextureEntry]:
        items = self._canvas.find_overlapping(x - 1, y - 1, x + 1, y + 1)
        for item in reversed(items):
            if item in self._item_map:
                return self._item_map[item]
        return None

    def _on_press(self, event: tk.Event) -> None:
        cx = self._canvas.canvasx(event.x)
        cy = self._canvas.canvasy(event.y)
        entry = self._entry_at(cx, cy)
        if entry is None:
            return
        self._selected = entry
        self._highlight_entry_name(entry.name)
        self._on_select(entry)

        # Sync peer browser to the same name
        if self._peer:
            self._peer.scroll_to_name(entry.name)

        # Prepare drag
        if entry.diffuse:
            pil = self._cache.load_pil(entry.diffuse)
            self._drag.press(entry, 'diffuse', entry.diffuse, pil, event)

    def _on_double_click(self, event: tk.Event) -> None:
        """Double-click: assign diffuse and reset all slots from this entry."""
        cx = self._canvas.canvasx(event.x)
        cy = self._canvas.canvasy(event.y)
        entry = self._entry_at(cx, cy)
        if entry is None:
            return
        self._selected = entry
        self._highlight_entry_name(entry.name)
        # Signal full-replace assignment (different from auto-fill)
        self._on_select(entry, replace=True)

        if self._peer:
            self._peer.scroll_to_name(entry.name)

    def _on_drag_motion(self, event: tk.Event) -> None:
        self._drag.motion(event)

    def _on_drag_release(self, event: tk.Event) -> None:
        self._drag.release(event)


# ── GL preview ────────────────────────────────────────────────────────────────
_VERT_SRC = """\
#version 330 core

in vec3 a_pos;
in vec2 a_uv;
in vec3 a_normal;
in vec3 a_tangent;

out vec2  v_uv;
out mat3  v_tbn;
out vec3  v_frag_pos;
out vec3  v_view_dir;

void main() {
    v_frag_pos  = a_pos;
    v_uv        = a_uv;

    vec3 T = normalize(a_tangent);
    vec3 N = normalize(a_normal);
    vec3 B = cross(N, T);
    v_tbn = mat3(T, B, N);

    v_view_dir = normalize(vec3(0.0, 0.0, 1.0) - a_pos);

    gl_Position = vec4(a_pos, 1.0);
}
"""

_FRAG_SRC = """\
#version 330 core

in vec2  v_uv;
in mat3  v_tbn;
in vec3  v_frag_pos;
in vec3  v_view_dir;

out vec4 out_color;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;
uniform sampler2D u_specular;
uniform sampler2D u_luma;

uniform int u_has_normal;
uniform int u_has_specular;
uniform int u_has_luma;

// Hardcoded point light (upper-left)
const vec3  LIGHT_POS   = vec3(-0.8, 0.8, 1.5);
const vec3  LIGHT_COLOR = vec3(1.00, 0.95, 0.88);
const float AMBIENT     = 0.18;
const float SPEC_POWER  = 64.0;

void main() {
    vec4 diffuse_samp = texture(u_diffuse, v_uv);
    if (diffuse_samp.a < 0.05) discard;

    vec3 N;
    if (u_has_normal != 0) {
        vec3 ns = texture(u_normal, v_uv).rgb * 2.0 - 1.0;
        N = normalize(v_tbn * ns);
    } else {
        N = normalize(v_tbn[2]);
    }

    vec3 L = normalize(LIGHT_POS - v_frag_pos);
    float NdotL = max(dot(N, L), 0.0);

    vec3 spec_contrib = vec3(0.0);
    if (u_has_specular != 0) {
        vec3 H    = normalize(L + v_view_dir);
        float sp  = pow(max(dot(N, H), 0.0), SPEC_POWER);
        vec3  sc  = texture(u_specular, v_uv).rgb;
        spec_contrib = LIGHT_COLOR * sp * sc;
    }

    vec3 luma_contrib = vec3(0.0);
    if (u_has_luma != 0) {
        luma_contrib = texture(u_luma, v_uv).rgb;
    }

    vec3 ambient = AMBIENT * LIGHT_COLOR;
    vec3 color   = (ambient + LIGHT_COLOR * NdotL) * diffuse_samp.rgb
                   + spec_contrib + luma_contrib;

    // Reinhard tone-map + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    out_color = vec4(color, diffuse_samp.a);
}
"""

# Quad: 4 verts × (pos3 + uv2 + norm3 + tan3) = 44 floats
_QUAD_VERTS = np.array([
    # pos              uv       normal       tangent
    -1.0, -1.0, 0.0,   0.0, 1.0,  0.0, 0.0, 1.0,  1.0, 0.0, 0.0,
     1.0, -1.0, 0.0,   1.0, 1.0,  0.0, 0.0, 1.0,  1.0, 0.0, 0.0,
     1.0,  1.0, 0.0,   1.0, 0.0,  0.0, 0.0, 1.0,  1.0, 0.0, 0.0,
    -1.0,  1.0, 0.0,   0.0, 0.0,  0.0, 0.0, 1.0,  1.0, 0.0, 0.0,
], dtype=np.float32)

_QUAD_IDX = np.array([0, 1, 2,  0, 2, 3], dtype=np.uint32)


if _GL_AVAILABLE:
    class GLPreview(tk.Canvas):
        """
        moderngl offscreen renderer displayed as a TK Canvas image.

        Renders a unit quad with diffuse + normal + specular + luma maps
        using Blinn-Phong shading with a single hardcoded point light.
        Refreshes whenever set_paths() is called and on resize.
        """

        def __init__(self, *args, **kwargs):
            super().__init__(*args, bg=BG, highlightthickness=0, **kwargs)
            self._tk_img:   Optional[ImageTk.PhotoImage] = None
            self._paths:    Dict[str, Optional[str]] = {
                'diffuse': None, 'norm': None,
                'gloss':   None, 'luma': None,
            }
            self._ctx:      Optional['moderngl.Context'] = None
            self._prog:     Optional['moderngl.Program'] = None
            self._vao:      Optional['moderngl.VertexArray'] = None
            self._tex_cache: Dict[str, 'moderngl.Texture'] = {}

            self.bind('<Configure>', self._on_configure)
            self.after(100, self._init_gl)

        def _init_gl(self) -> None:
            try:
                self._ctx = moderngl.create_standalone_context()
                self._prog = self._ctx.program(
                    vertex_shader=_VERT_SRC,
                    fragment_shader=_FRAG_SRC,
                )
                stride = 11 * 4   # 11 floats × 4 bytes
                vbo = self._ctx.buffer(_QUAD_VERTS.tobytes())
                ibo = self._ctx.buffer(_QUAD_IDX.tobytes())
                self._vao = self._ctx.vertex_array(
                    self._prog,
                    [(vbo, '3f 2f 3f 3f', 'a_pos', 'a_uv', 'a_normal', 'a_tangent')],
                    ibo,
                )
            except Exception as exc:
                print(f"[GL] Init failed: {exc}")
            self._rerender()

        def set_paths(self, paths: Dict[str, Optional[str]]) -> None:
            changed = False
            for role, path in paths.items():
                if path != self._paths.get(role):
                    self._paths[role] = path
                    changed = True
            if changed:
                self._rerender()

        def _on_configure(self, _event: tk.Event) -> None:
            self._rerender()

        def _load_texture(self, path: str) -> Optional['moderngl.Texture']:
            if self._ctx is None:
                return None
            if path not in self._tex_cache:
                try:
                    img  = Image.open(path).convert('RGBA')
                    data = img.tobytes()
                    tex  = self._ctx.texture(img.size, 4, data)
                    tex.filter    = (moderngl.LINEAR_MIPMAP_LINEAR, moderngl.LINEAR)
                    tex.repeat_x  = True
                    tex.repeat_y  = True
                    tex.build_mipmaps()
                    self._tex_cache[path] = tex
                except Exception as exc:
                    print(f"[GL] Texture load error ({path}): {exc}")
                    return None
            return self._tex_cache.get(path)

        def _rerender(self) -> None:
            if self._ctx is None or self._prog is None or self._vao is None:
                return
            diffuse_path = self._paths.get('diffuse')
            if not diffuse_path:
                self.delete('all')
                self._draw_empty()
                return

            w = max(self.winfo_width(),  4)
            h = max(self.winfo_height(), 4)

            fbo = self._ctx.framebuffer(
                color_attachments=[self._ctx.texture((w, h), 4)],
                depth_attachment=self._ctx.depth_renderbuffer((w, h)),
            )
            fbo.use()
            self._ctx.clear(0.12, 0.12, 0.12)
            self._ctx.enable(moderngl.DEPTH_TEST)

            def bind(role: str, unit: int, sampler_name: str) -> bool:
                p = self._paths.get(role)
                if p:
                    t = self._load_texture(p)
                    if t:
                        t.use(location=unit)
                        self._prog[sampler_name] = unit
                        return True
                return False

            # Always need a diffuse
            if not bind('diffuse', 0, 'u_diffuse'):
                fbo.release()
                return

            has_norm  = int(bind('norm',  1, 'u_normal'))
            has_gloss = int(bind('gloss', 2, 'u_specular'))
            has_luma  = int(bind('luma',  3, 'u_luma'))

            self._prog['u_has_normal']   = has_norm
            self._prog['u_has_specular'] = has_gloss
            self._prog['u_has_luma']     = has_luma

            self._vao.render()

            # Read back and display
            raw  = fbo.read(components=3)   # RGB
            img  = Image.frombytes('RGB', (w, h), raw).transpose(Image.FLIP_TOP_BOTTOM)
            self._tk_img = ImageTk.PhotoImage(img)
            self.delete('all')
            self.create_image(0, 0, anchor='nw', image=self._tk_img)

            fbo.color_attachments[0].release()
            fbo.depth_attachment.release()
            fbo.release()

        def _draw_empty(self) -> None:
            w = self.winfo_width()
            h = self.winfo_height()
            self.create_rectangle(0, 0, w, h, fill=BG, outline='')
            self.create_text(w // 2, h // 2,
                             text="Assign a diffusemap to preview",
                             fill=TEXT_COLOR, font=('Helvetica', 10))


# ── PIL fallback preview ──────────────────────────────────────────────────────
class PilPreview(tk.Canvas):
    """Simple PIL-composite preview when OpenGL is unavailable."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, bg=BG, highlightthickness=0, **kwargs)
        self._tk_img: Optional[ImageTk.PhotoImage] = None
        self._label  = tk.Label(self, text="OpenGL unavailable\n(PIL composite preview)",
                                bg=BG, fg=TEXT_COLOR, font=('Helvetica', 9))
        self.create_window(10, 10, anchor='nw', window=self._label)
        self.bind('<Configure>', self._on_configure)
        self._paths: Dict[str, Optional[str]] = {}

    def set_paths(self, paths: Dict[str, Optional[str]]) -> None:
        self._paths = paths
        self._rerender()

    def _on_configure(self, _event: tk.Event) -> None:
        self._rerender()

    def _rerender(self) -> None:
        w = self.winfo_width()
        h = self.winfo_height()
        if w < 4 or h < 4:
            return
        diffuse_path = self._paths.get('diffuse')
        if not diffuse_path:
            self.delete('preview')
            return
        try:
            size   = min(w, h) - 8
            base   = Image.open(diffuse_path).convert('RGBA').resize((size, size), Image.LANCZOS)
            luma_p = self._paths.get('luma')
            if luma_p:
                luma = Image.open(luma_p).convert('RGBA').resize((size, size), Image.LANCZOS)
                base = Image.alpha_composite(base, luma)
            self._tk_img = ImageTk.PhotoImage(base)
            self.delete('preview')
            self.create_image(w // 2, h // 2, anchor='center',
                              image=self._tk_img, tags=('preview',))
            self._label.place_forget()
        except Exception:
            pass


# ── Texture writer ────────────────────────────────────────────────────────────
_ROLE_SUFFIX: Dict[str, str] = {
    'diffuse': '',
    'norm':    '_norm',
    'gloss':   '_gloss',
    'luma':    '_luma',
}


def write_texture_set(paths: Dict[str, Optional[str]], output_dir: str) -> Tuple[int, str]:
    """
    Write a set of texture layers to output_dir as TGA files.

    All layers are resized to match the diffuse map's pixel dimensions.
    Returns (files_written, status_message).
    """
    diffuse_path = paths.get('diffuse')
    if not diffuse_path:
        return 0, "No diffuse map assigned — nothing to write."

    os.makedirs(output_dir, exist_ok=True)

    # Determine canonical size from diffuse
    try:
        diffuse_img = Image.open(diffuse_path).convert('RGBA')
    except Exception as exc:
        return 0, f"Failed to open diffuse: {exc}"

    canonical_size = diffuse_img.size
    base = os.path.splitext(os.path.basename(diffuse_path))[0]
    base = _SUFFIX_RE.sub('', base)   # strip any trailing _norm etc.

    written = 0
    for role, suffix in _ROLE_SUFFIX.items():
        src_path = paths.get(role)
        if not src_path:
            continue
        try:
            img = Image.open(src_path).convert('RGBA')
            if img.size != canonical_size:
                img = img.resize(canonical_size, Image.LANCZOS)
            out_name = f"{base}{suffix}.tga"
            out_path = os.path.join(output_dir, out_name)
            img.save(out_path)
            written += 1
        except Exception as exc:
            print(f"[writer] Error writing {role}: {exc}")

    return written, f"Wrote {written} file(s) → {os.path.basename(output_dir)}/{base}"


# ── Action panel ──────────────────────────────────────────────────────────────
class ActionPanel(tk.Frame):
    """Bottom bar with export buttons and a status line."""

    BTN_STYLE = dict(
        bg='#3a3a3a', fg=TEXT_COLOR, activebackground='#555555',
        activeforeground='white', relief='flat', bd=0,
        font=('Helvetica', 10, 'bold'), padx=18, pady=8, cursor='hand2',
    )
    BTN_COLORS = {
        'qrp':      '#4a6e4a',
        'combined': '#4a9eff',
        'rygel':    '#6e4a6e',
    }

    def __init__(self, parent: tk.Widget,
                 on_use_qrp: Callable,
                 on_use_combined: Callable,
                 on_use_rygel: Callable,
                 output_dir: str,
                 **kwargs):
        super().__init__(parent, bg=BG2, **kwargs)
        self._output_dir = output_dir
        self._build(on_use_qrp, on_use_combined, on_use_rygel)

    def _build(self, on_use_qrp: Callable,
               on_use_combined: Callable, on_use_rygel: Callable) -> None:
        left = tk.Frame(self, bg=BG2)
        left.pack(side='left', padx=10, pady=6)

        def _btn(parent, label, color, cmd):
            b = tk.Button(parent, text=label, command=cmd,
                          bg=color, **{k: v for k, v in self.BTN_STYLE.items()
                                       if k != 'bg'})
            b.config(bg=color, activebackground=color)
            b.pack(side='left', padx=4)
            return b

        _btn(left, "Use QRP",      self.BTN_COLORS['qrp'],      on_use_qrp)
        _btn(left, "Use Combined", self.BTN_COLORS['combined'],  on_use_combined)
        _btn(left, "Use Rygel",    self.BTN_COLORS['rygel'],     on_use_rygel)

        tk.Label(left, text="→", bg=BG2, fg='#666', font=('Helvetica', 12)).pack(side='left', padx=6)
        self._out_label = tk.Label(left, text=self._output_dir, bg=BG2,
                                   fg='#888', font=('Helvetica', 8))
        self._out_label.pack(side='left')

        self._status = tk.Label(self, text="", bg=BG2, fg=TEXT_COLOR,
                                font=('Helvetica', 9), anchor='e')
        self._status.pack(side='right', padx=16)

    def set_status(self, msg: str, ok: bool = True) -> None:
        self._status.config(text=msg, fg='#80d080' if ok else '#d08080')
        self.after(6000, lambda: self._status.config(text=''))


# ── Main application ──────────────────────────────────────────────────────────
class App(tk.Tk):
    def __init__(self, qrp_dir: str, quake_dir: str, output_dir: str):
        super().__init__()
        self.title("quake-fu — texture set merger")
        self.configure(bg=BG)
        self.geometry("1920x1080")
        self.minsize(1200, 720)

        self._executor   = ThreadPoolExecutor(max_workers=4, thread_name_prefix='thumb')
        self._cache      = ThumbnailCache()
        self._drag       = DragController(self)
        self._output_dir = output_dir

        # Style
        style = ttk.Style(self)
        style.theme_use('clam')
        style.configure('TScrollbar', background=BG2, troughcolor=BG,
                        bordercolor=BG, arrowcolor=TEXT_COLOR)

        # Scan texture sets; keep dicts for name-based lookup
        qrp_scan   = TextureScanner(qrp_dir)
        quake_scan = TextureScanner(quake_dir)
        self._qrp_by_name:   Dict[str, TextureEntry] = qrp_scan.entries
        self._rygel_by_name: Dict[str, TextureEntry] = quake_scan.entries
        qrp_entries   = qrp_scan.diffuse_entries()
        quake_entries = quake_scan.diffuse_entries()

        self._current_entry: Optional[TextureEntry] = None

        self._build_ui(qrp_entries, quake_entries)
        self.bind('<q>', lambda e: self._on_use_qrp())
        self.bind('<r>', lambda e: self._on_use_rygel())
        self.protocol('WM_DELETE_WINDOW', self._on_close)

    def _build_ui(self, qrp_entries: List[TextureEntry],
                  quake_entries: List[TextureEntry]) -> None:
        # ── Work area (top, full width) ────────────────────────────────────
        self._work_area = WorkArea(self, self._cache, self._drag,
                                   self._on_work_change)
        self._work_area.pack(fill='x', padx=4, pady=(4, 0))

        ttk.Separator(self, orient='horizontal').pack(fill='x', pady=2)

        # ── Three-column main area ─────────────────────────────────────────
        paned = tk.PanedWindow(self, orient='horizontal', bg=BG,
                               sashwidth=6, sashrelief='flat',
                               sashpad=2, relief='flat', bd=0)
        paned.pack(fill='both', expand=True, padx=4, pady=4)

        # Left browser (QRP)
        left_browser = TextureBrowser(
            paned, f"QRP  ({len(qrp_entries)} textures)",
            qrp_entries, self._cache, self._drag,
            self._on_qrp_select, self._executor)
        paned.add(left_browser, minsize=540, width=580)

        # Center: preview
        self._preview = self._make_preview(paned)
        paned.add(self._preview, minsize=320, width=720)

        # Right browser (Rygel)
        right_browser = TextureBrowser(
            paned, f"Rygel  ({len(quake_entries)} textures)",
            quake_entries, self._cache, self._drag,
            self._on_rygel_select, self._executor)
        paned.add(right_browser, minsize=540, width=580)

        # Wire browsers as peers so selecting one scrolls the other
        left_browser.set_peer(right_browser)
        right_browser.set_peer(left_browser)

        ttk.Separator(self, orient='horizontal').pack(fill='x')

        # ── Action panel (bottom) ──────────────────────────────────────────
        self._action_panel = ActionPanel(
            self,
            on_use_qrp=self._on_use_qrp,
            on_use_combined=self._on_use_combined,
            on_use_rygel=self._on_use_rygel,
            output_dir=self._output_dir,
        )
        self._action_panel.pack(fill='x', side='bottom')

    def _make_preview(self, parent: tk.Widget) -> tk.Widget:
        frame = tk.Frame(parent, bg=BG)
        tk.Label(frame, text="PREVIEW", bg=BG2, fg=TEXT_COLOR,
                 font=('Helvetica', 9, 'bold'), pady=4).pack(fill='x')
        if _GL_AVAILABLE:
            try:
                gl = GLPreview(frame, width=300, height=300)
                gl.pack(fill='both', expand=True)
                self._gl_view = gl
                return frame
            except Exception as exc:
                print(f"[GL] Could not create GL preview: {exc}")
        # fallback — pure PIL composite
        pil = PilPreview(frame, width=300, height=300)
        pil.pack(fill='both', expand=True)
        self._gl_view = pil
        return frame

    def _on_work_change(self) -> None:
        paths = self._work_area.get_paths()
        self._gl_view.set_paths(paths)

    def _on_qrp_select(self, entry: TextureEntry, replace: bool = False) -> None:
        self._current_entry = entry
        self._auto_assign(entry, replace=replace)

    def _on_rygel_select(self, entry: TextureEntry, replace: bool = False) -> None:
        self._current_entry = entry
        self._auto_assign(entry, replace=replace)

    def _auto_assign(self, entry: TextureEntry, replace: bool = False) -> None:
        """Fill empty work area slots (or replace all if replace=True)."""
        slots = self._work_area.slots
        role_map = [
            ('diffuse', entry.diffuse),
            ('norm',    entry.norm),
            ('gloss',   entry.gloss),
            ('luma',    entry.luma),
        ]
        changed = False
        for role, path in role_map:
            if replace:
                # Always overwrite; clear slot if no path for this role
                if path:
                    slots[role].accept_drop(entry, role, path)
                else:
                    slots[role]._on_clear(None)
                changed = True
            elif path and slots[role].assigned_path is None:
                slots[role].accept_drop(entry, role, path)
                changed = True
        if changed:
            self._on_work_change()

    def _on_close(self) -> None:
        self._executor.shutdown(wait=False)
        self.destroy()

    # ── Export callbacks ───────────────────────────────────────────────────
    def _export_entry(self, entry: Optional[TextureEntry], label: str) -> None:
        if entry is None:
            self._action_panel.set_status("Select a texture first.", ok=False)
            return
        paths: Dict[str, Optional[str]] = {
            'diffuse': entry.diffuse,
            'norm':    entry.norm,
            'gloss':   entry.gloss,
            'luma':    entry.luma,
        }
        n, msg = write_texture_set(paths, self._output_dir)
        self._action_panel.set_status(f"[{label}]  {msg}", ok=n > 0)

    def _on_use_qrp(self) -> None:
        # Find the QRP entry matching the current selection's base name
        entry = self._current_entry
        if entry:
            entry = self._qrp_by_name.get(entry.name, entry)
        self._export_entry(entry, "QRP")

    def _on_use_rygel(self) -> None:
        entry = self._current_entry
        if entry:
            entry = self._rygel_by_name.get(entry.name, entry)
        self._export_entry(entry, "Rygel")

    def _on_use_combined(self) -> None:
        paths = self._work_area.get_paths()
        n, msg = write_texture_set(paths, self._output_dir)
        self._action_panel.set_status(f"[Combined]  {msg}", ok=n > 0)


# ── Entry point ───────────────────────────────────────────────────────────────
def main() -> None:
    parser = argparse.ArgumentParser(description='quake-fu texture set merger')
    parser.add_argument('--qrp',    default=DEFAULT_QRP_DIR,
                        help='Path to QRP texture directory')
    parser.add_argument('--quake',  default=DEFAULT_QUAKE_DIR,
                        help='Path to Rygel (quake) texture directory')
    parser.add_argument('--output', default=DEFAULT_OUTPUT_DIR,
                        help='Path to output directory')
    args = parser.parse_args()

    for label, path in (('QRP', args.qrp), ('Rygel', args.quake)):
        if not os.path.isdir(path):
            print(f"Warning: {label} directory not found: {path}", file=sys.stderr)

    app = App(qrp_dir=args.qrp, quake_dir=args.quake, output_dir=args.output)
    app.mainloop()


if __name__ == '__main__':
    main()
