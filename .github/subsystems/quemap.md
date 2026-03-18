# Map Compiler (quemap)

The Quetoo map compiler (quemap) converts human-readable .map files into optimized .bsp files for the engine.

## Location

`src/tools/quemap/` - Map compiler tool

## Core Purpose

- Compile .map files (text) → .bsp files (binary)
- Build BSP tree for fast spatial queries
- Calculate visibility (PVS/PHS)
- Bake lightmaps and compute lighting
- Generate voxel grid for clustered lighting
- Optimize geometry for rendering

## Compilation Stages

### 1. BSP Phase (-bsp)

**Reads**: `.map` file (Quake map format)  
**Writes**: `.bsp` file (BSP tree, geometry)  
**Command**: `quemap -bsp maps/mymap.map`

**Process**:
1. Parse .map file (brushes, entities)
2. Construct CSG tree (combine/subtract brushes)
3. Build BSP tree for spatial partitioning
4. Extract faces (polygons) from brushes
5. Generate vertices, indices for rendering
6. Optimize T-junctions (prevent visual cracks)
7. Write initial .bsp file

**Output**: Compiled geometry, playable but unlit.

### 2. Visibility Phase (-vis)

**Reads**: `.bsp` file  
**Writes**: `.bsp` file (adds PVS/PHS data)  
**Command**: `quemap -vis maps/mymap.bsp`

**Process**:
1. Divide world into areas (BSP leafs)
2. For each area, calculate which other areas are visible (PVS)
3. Calculate which areas are "hearable" for sound (PHS)
4. Store visibility data in BSP

**Potentially Visible Set (PVS)**:
- Server only sends entities in client's PVS
- Renderer only draws faces in PVS
- Huge performance win vs. drawing everything

**Potentially Hearable Set (PHS)**:
- Server only sends sounds in client's PHS
- Reduces network bandwidth

**Performance**: Slow (can take minutes/hours for large maps)

### 3. Lighting Phase (-light)

**Reads**: `.bsp` file  
**Writes**: `.bsp` file (adds lightmaps, lights, voxels)  
**Command**: `quemap -light maps/mymap.bsp`

**Process**:
1. Parse light entities from map
2. Generate light samples across all surfaces
3. Calculate direct lighting (from light to surface)
4. Calculate bounce lighting (indirect illumination)
5. Build lightmap textures (baked lighting)
6. Generate dynamic light definitions
7. **Build voxel grid and assign lights to voxels** (clustered lighting)
8. Write final .bsp file

**Output**: Fully lit map ready to play.

## Key Files

### main.c
Entry point, command-line parsing:
```bash
quemap -bsp -light -vis maps/mymap.map    # All stages
quemap -bsp maps/mymap.map                 # BSP only
quemap -light -bounce 8 maps/mymap.bsp     # Lighting with 8 bounces
```

### map.c / map.h
.map file parsing:
- Read brushes (convex volumes)
- Read entities (spawn points, items, lights, etc.)
- Parse key-value pairs

### brush.c / brush.h
Brush handling:
- Brushes are convex hulls (6+ planes)
- CSG operations (union, subtract, intersect)
- Face extraction

### csg.c / csg.h
Constructive Solid Geometry:
- Combine overlapping brushes
- Subtract detail brushes from world
- Generate final brush set

### bsp.c / bsp.h
BSP tree construction:
- Recursively partition space with planes
- Choose optimal split planes (balance tree, minimize splits)
- Generate BSP nodes and leafs

### face.c / face.h
Face (polygon) management:
- Extract faces from brushes
- Clip faces to BSP tree
- Generate vertices, texture coordinates

### tjunction.c / tjunction.h
T-junction fixing:
- Find vertices that lie on edges (T-junctions)
- Split edges to add vertices
- Prevents cracks in geometry

### portal.c / portal.h
Portal calculation (for VIS):
- Portals are polygons connecting BSP leafs
- Used for visibility calculation

### vis (qbsp.c)
Visibility calculation:
- Potentially Visible Set per leaf
- Potentially Hearable Set per leaf
- Portal flow algorithm

### light.c / light.h
Lighting system:
- Direct lighting (light → surface)
- Bounce lighting (radiosity)
- Lightmap generation
- Dynamic light definitions

### voxel.c / voxel.h
**Voxel grid for clustered lighting** (see `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md`):
- Divide world into 32x32x32 unit voxels
- For each voxel, determine which lights affect it
- Use sphere-box intersection test (not just point-to-center)
- Store light indices per voxel
- Runtime shaders look up voxel to get light list

**Critical**: Proper sphere-box intersection prevents lighting artifacts at voxel boundaries.

### material.c / material.h
Material properties:
- Parse .mat files (material definitions)
- Surface flags, contents flags
- Texture names

### texture.c / texture.h
Texture handling:
- Load textures for lightmap calculations
- Calculate texture coordinates

### writebsp.c / writebsp.h
BSP file writing:
- Serialize all data to .bsp format
- Write lumps (entities, vertices, faces, lights, voxels, etc.)

## BSP File Format

See `src/collision/cm_bsp.h` for format specification.

**Structure**:
```
Header (version, lump offsets/sizes)
Lump 0: Entities (text)
Lump 1: Materials
Lump 2: Planes
Lump 3: Brush sides
Lump 4: Brushes
Lump 5: Vertices
Lump 6: Elements (indices)
Lump 7: Faces
Lump 8: Nodes
Lump 9: Leaf brushes
Lump 10: Leafs
Lump 11: Draw elements
Lump 12: Blocks (frustum culling)
Lump 13: Models (inline models)
Lump 14: Lights
Lump 15: Voxels
```

## Common Options

### BSP Stage
```bash
-bsp                 # BSP compilation
-novis               # Skip vis stage (for testing)
-nopvs               # No PVS generation
-noclip              # No collision brushes
-nodetail            # Ignore detail brushes
```

### Lighting Stage
```bash
-light               # Lighting compilation
-bounce <N>          # Bounce lighting passes (default 8)
-saturation <N>      # Color saturation (0.0-1.0)
-brightness <N>      # Global brightness multiplier
-contrast <N>        # Contrast adjustment
-sun <yaw> <pitch> <color> <intensity>  # Add sunlight
```

### General
```bash
-threads <N>         # Number of threads (default: CPU count)
-verbose             # Verbose output
```

## Workflow

### Quick Test Compile
```bash
# Fast, no VIS or lighting
quemap -bsp maps/mymap.map
```

Play immediately, but:
- No lighting (fullbright)
- No PVS (renders everything, slow)

### Full Compile
```bash
# All stages, production quality
quemap -bsp -vis -light -bounce 8 maps/mymap.map
```

Takes 5-60 minutes depending on map size.

### Iterative Lighting
```bash
# Adjust lighting without recompiling BSP/VIS
quemap -light -bounce 8 -brightness 1.2 maps/mymap.bsp
```

Much faster than full recompile.

## Light Entities

**light** - Omnidirectional light:
```
{
  "classname" "light"
  "origin" "256 256 128"
  "light" "300"          // Intensity
  "color" "1.0 0.8 0.6"  // RGB color
  "radius" "256"         // Max radius
}
```

**light_spot** - Spotlight:
```
{
  "classname" "light_spot"
  "origin" "256 256 256"
  "target" "spot_target"
  "angle" "45"           // Cone angle
  "light" "500"
}
```

**Sun/sky lighting**:
```bash
# Add sunlight at compile time
quemap -light -sun 45 -30 "1.0 1.0 0.9" 150 maps/mymap.bsp
```

## Voxel Lighting System

**Build time** (quemap -light):
1. Divide map into 32-unit voxels
2. For each voxel:
   - Find all lights in/near voxel
   - Test sphere-box intersection (light radius vs voxel bounds)
   - Store light indices (max 31 per voxel)
3. Write voxel data to BSP_LUMP_VOXELS

**Runtime** (renderer):
1. Fragment shader calculates which voxel it's in
2. Looks up voxel in 3D texture
3. Iterates only lights assigned to that voxel (10-20 instead of 512+)
4. Dramatic performance improvement

See `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md` for details.

## Performance

### Compile Times

**Small map** (~100 brushes):
- BSP: 1-5 seconds
- VIS: 10-30 seconds
- Light: 30-120 seconds

**Large map** (~1000 brushes):
- BSP: 10-30 seconds
- VIS: 5-30 minutes
- Light: 5-15 minutes

### Optimization Tips

- Use detail brushes (don't block VIS)
- Hint brushes guide BSP tree splits
- Skip brushes (block VIS, optimize)
- Area portals (manual VIS control)

## Common Issues

### Leak

**Error**: "Map leaks! Fix the leak before running VIS."  
**Cause**: Map not fully sealed (hole to void)  
**Fix**: 
1. Load leak file (`maps/mymap.lin`)
2. Follow line from entity to leak
3. Seal hole with brushes

### Bad BSP Tree

**Symptom**: Visible faces cut incorrectly, z-fighting  
**Cause**: Poor plane selection during BSP split  
**Fix**: Add hint brushes to guide splits

### Dark Lightmaps

**Symptom**: Map too dark even with lights  
**Cause**: Insufficient bounce passes or brightness  
**Fix**: 
```bash
quemap -light -bounce 16 -brightness 1.5 maps/mymap.bsp
```

### Voxel Boundary Artifacts

**Symptom**: Hard lighting transitions at voxel edges  
**Cause**: Incorrect sphere-box intersection (point-to-center instead of proper test)  
**Fix**: Fixed in voxel.c (see `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md`)

## Debugging

### Show BSP Tree
```
r_draw_bsp_normals 1     # Show face normals
r_draw_wireframe 1       # Wireframe mode
```

### Show Voxels
In game:
```
r_show_voxels 1          # Visualize voxel grid (if implemented)
```

### Verbose Output
```bash
quemap -bsp -verbose maps/mymap.map
```

Shows detailed progress.

## Material System

Materials defined in .mat files:
```
{
  material textures/base/concrete
  {
    texture textures/base/concrete.tga
    normalmap textures/base/concrete_norm.tga
    glossmap textures/base/concrete_gloss.tga
    roughness 0.8
    hardness 0.3
  }
}
```

Quemap reads these for:
- Texture coordinates
- Surface properties
- Lightmap calculation

## Integration

- **Game**: Loads .bsp for collision, entity spawning
- **Renderer**: Loads .bsp for geometry, voxels, lights
- **Server**: Uses entity data to spawn items, triggers

## Other Tools

### quemap-fu
Python scripts for batch processing:
- Batch compile multiple maps
- Monitor .map files, auto-compile on save
- Located in `src/tools/quemap-fu/`

### qzip
Pak file (.pk3) creation:
```bash
qzip maps/ -o mymaps.pk3
```

Creates .pk3 (zip) archive for distribution.
