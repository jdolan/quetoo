# Quemap GUI

A zero-install Windows front-end for bulk-compiling Quetoo maps with `quemap`.
It's the Windows-friendly equivalent of running `make` in `quetoo-data`.

## Why this exists

`quetoo-data`'s `Makefile` bulk-compiles maps (`make` finds "dirty" `.map`
files and rebuilds their `.bsp`s), but it needs `make` + `python3` + `quemap`
on `PATH`, which doesn't work out-of-the-box on Windows. This tool does the
same job natively, with no `make` and nothing for users to install.

## Running it

1. **Quetoo root** — point this at your Quetoo install/build dir. The tool
   looks for `bin\quemap.exe` first, then searches recursively.
2. **Map(s)** — pick a single `.map` (File…), several (multi-select), or a
   folder (Folder…) to compile every `.map` under it.
3. **Compile** — streams `quemap` output live and shows overall progress.

Options:
- **Only changed (dirty) maps** — skip maps whose `.bsp` is newer than the
  `.map` (same behavior as `make`). On by default.
- **Include subfolders** — when a folder is selected, recurse into subdirectories
  (on by default) or process only the top level. Applies to both tabs.

## How it invokes quemap

For each map it derives the game directory (the parent of the map's `maps`
folder) and runs:

```
quemap.exe -w <gamedir> -bsp [--antialias] maps/<name>.map
```

Notes that come straight from `quemap`'s `main.c`:
- The map argument **must** start with `maps/`, so maps have to live under a
  `maps` folder. The tool finds that folder automatically.
- `-w` sets the game directory; assets (textures, etc.) resolve relative to it.
- `-bsp` **also runs the lighting stage** — there is no separate `-light`
  flag. So every compile bakes lighting, just like `make`.

Maps are compiled one at a time (quemap is already multi-threaded per map).

## Convert tab (Quake3 <-> Valve-220)

Converts brush-side texture coordinates between the standard Quake3
shift/rotate/scale projection and the Valve-220 explicit S/T axis projection.
The texture-axis math is ported 1:1 from `map-fu` / quemap's `texture.c`, so the
output matches the `map-fu convert` script. No Python required.

- **Map(s)** — single file, multi-select, or a folder.
- **Direction** — Quake3 -> Valve-220, or Valve-220 -> Quake3.
- **Output** — write a new `<name>.valve.map` / `<name>.quake3.map`, or overwrite
  in place.
- **Snap offsets + rotation to grid** (optional) — force each face's texture
  offset to the nearest multiple of the offset increment (default `0.125`) and
  its rotation to the nearest multiple of the rotation increment (default `1`°).
  Always-snap (not threshold-based): editors leave sub-unit drift in derived UV
  offsets via texture lock, and this cleans it up. Scale is never touched (legit
  fitted scales like `0.578125` = 37/64 must be preserved). Snapping runs through
  the standard shift/rotate params, so it works regardless of source/target
  format. The count of snapped faces is reported.

Notes:
- Standard -> Valve is exact. Valve -> standard is exact for any projection a
  standard face can represent; a genuinely sheared/non-axial Valve face can't be,
  so it's **reset to default alignment and a warning is logged**.
- Texture values are rounded to 6 decimals. On values that sit exactly on the
  6-decimal boundary the last digit may differ from `map-fu` by 1e-6 — a float
  epsilon quemap reads identically. Geometry/flags/comments are preserved
  verbatim, and the `// Format:` header is updated.
- Folder + "new file" mode emits siblings; re-running a folder will also pick up
  previously generated `*.valve.map` / `*.quake3.map` files.

## Building

Requires nothing but Windows — `build.bat` uses the in-box .NET Framework
compiler:

```
build.bat
```

This produces `QuemapGui.exe`, which runs on any Windows 10/11 machine
(.NET Framework 4.8 is part of the OS). You can also add `QuemapGui.cs` to a
Visual Studio WinForms project if you prefer.
