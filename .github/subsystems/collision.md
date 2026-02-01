# Collision System (cm_*)

The collision model system provides BSP loading, spatial queries, and collision detection for both client and server.

## Location

`src/collision/` - Shared library used by both client and server

## Core Purpose

- Load and parse `.bsp` files (Quake II format with Quetoo extensions)
- Provide fast spatial queries (traces, point contents, entity boxes)
- Manage collision detection for player movement and projectiles
- Handle BSP entity parsing from map files

## Key Files

### cm_bsp.c / cm_bsp.h
BSP file loading and format definitions:
- `Cm_LoadBSP()` - Main BSP loader, parses all lumps
- BSP format constants (MAX_BSP_*, BSP_VERSION, etc.)
- Lump definitions (BSP_LUMP_ENTITIES, BSP_LUMP_VOXELS, etc.)
- Data structures for all BSP lumps

**Important BSP lumps**:
- `BSP_LUMP_ENTITIES` - Map entity definitions (spawns, items, triggers)
- `BSP_LUMP_MATERIALS` - Surface materials (textures, properties)
- `BSP_LUMP_PLANES` - Clipping planes for BSP tree
- `BSP_LUMP_BRUSHES` - Convex hulls for solid geometry
- `BSP_LUMP_NODES` - BSP tree nodes for spatial partitioning
- `BSP_LUMP_LEAFS` - BSP tree leaves (areas of space)
- `BSP_LUMP_VOXELS` - Voxel grid for clustered lighting
- `BSP_LUMP_LIGHTS` - Dynamic light definitions
- `BSP_LUMP_BLOCKS` - Spatial blocks for frustum culling

### cm_trace.c / cm_trace.h
Collision tracing (raycasts and box traces):
- `Cm_BoxTrace()` - Main trace function for swept AABB tests
- `Cm_TransformedBoxTrace()` - Trace against rotated/translated models
- Traces return `cm_trace_t` with hit info (fraction, plane, surface, contents)

**Trace usage pattern**:
```c
const cm_trace_t tr = Cm_BoxTrace(start, end, mins, maxs, 0, CONTENTS_MASK_SOLID);
if (tr.fraction < 1.0) {
    // Hit something at tr.end
    // tr.plane has surface normal
    // tr.surface has material properties
}
```

### cm_test.c / cm_test.h
Point and box testing:
- `Cm_PointContents()` - What contents is this point in? (solid, water, lava, etc.)
- `Cm_BoxContents()` - What contents does this box overlap?
- `Cm_HullContents()` - For BSP model hulls

**Contents flags** (see shared/shared.h):
- `CONTENTS_SOLID` - Blocks movement
- `CONTENTS_WATER` - Water volume
- `CONTENTS_LAVA` - Lava (damages players)
- `CONTENTS_MIST` - Fog/mist
- `CONTENTS_DETAIL` - Detail geometry (doesn't block vis)
- `CONTENTS_TRANSLUCENT` - Glass, grates
- `CONTENTS_LADDER` - Climbable

### cm_entity.c / cm_entity.h
BSP entity parsing:
- `Cm_EntityString()` - Get raw entity string from BSP
- `Cm_Entities()` - Parse entities into array of `cm_entity_t`
- Used by server to spawn entities, by client for info_player_start, etc.

### cm_material.c / cm_material.h
Surface material properties:
- `Cm_Material()` - Get material for a surface
- Materials define texture names, surface flags, contents flags
- Used for footstep sounds, impact effects, etc.

### cm_model.c / cm_model.h
Submodel (inline BSP models) handling:
- `Cm_Model()` - Get submodel by name ("*0", "*1", etc.)
- Submodels are func_door, func_plat, etc. (brushes with entity data)
- Returns head node for tracing against the submodel

### cm_polylib.c / cm_polylib.h
Polygon manipulation utilities:
- Used internally by BSP compiler (quemap)
- Polygon splitting, clipping, winding operations

## Data Flow

### Loading a Map

1. Server calls `Cm_LoadBSP("maps/dm1.bsp")`
2. BSP file parsed into internal structures (nodes, leafs, brushes, etc.)
3. `Cm_Entities()` returns entity definitions
4. Server spawns entities based on entity data
5. Client loads same BSP for rendering

### Collision Detection

1. Game needs to move player or projectile
2. Calls `Cm_BoxTrace(start, end, bbox, ...)` with desired movement
3. Trace walks BSP tree, tests against brushes
4. Returns hit fraction, surface normal, material
5. Game adjusts movement based on result

### Point Queries

1. Game needs to know environment at a point (water? lava?)
2. Calls `Cm_PointContents(point, headnode)`
3. Walks BSP tree to find leaf
4. Returns OR'd contents of all brushes in leaf

## Important Constants

```c
#define BSP_VERSION 22              // Current BSP format version
#define BSP_BLOCK_SIZE 512.f        // Spatial block size (frustum culling)
#define BSP_VOXEL_SIZE 32.f         // Voxel size for lighting grid
#define MAX_BSP_ENTITIES 2048       // Max entity definitions
#define MAX_BSP_MODELS 0x100        // Max inline models (*0, *1, ...)
#define MAX_BSP_LIGHTS 0x100        // Max dynamic lights in BSP
```

## Contents vs Surface Flags

**Contents** - What's inside a volume (checked during traces):
- Used for: Movement blocking, liquid detection, environmental effects
- Examples: CONTENTS_SOLID, CONTENTS_WATER, CONTENTS_LAVA

**Surface flags** - Properties of a surface face:
- Used for: Rendering hints, sound properties, material behavior
- Examples: SURF_LIGHT (emits light), SURF_SKY (skybox), SURF_WARP (animated)

## Common Patterns

### Tracing from Entity to Entity
```c
vec3_t start, end;
Vec3_Copy(entity->s.origin, start);
Vec3_Copy(target->s.origin, end);

const cm_trace_t tr = Cm_BoxTrace(start, end, NULL, NULL, 0, CONTENTS_MASK_SOLID);
if (tr.fraction == 1.0) {
    // Clear line of sight
}
```

### Finding Ground
```c
vec3_t end;
Vec3_Copy(ent->s.origin, end);
end[2] -= 1.0; // Check 1 unit down

const cm_trace_t tr = Cm_BoxTrace(ent->s.origin, end, ent->mins, ent->maxs, 
                                   0, CONTENTS_MASK_SOLID);
if (tr.fraction < 1.0) {
    // On ground, tr.plane has ground normal
}
```

### Checking for Water
```c
vec3_t point;
Vec3_Copy(ent->s.origin, point);
point[2] = ent->s.origin[2] + ent->mins[2] + 1.0; // Feet position

const int32_t contents = Cm_PointContents(point, 0);
if (contents & CONTENTS_MASK_LIQUID) {
    // In water/lava
}
```

## Integration with Other Systems

### Server (sv_world.c)
Server uses collision for:
- Player movement (`SV_Move()` calls `Cm_BoxTrace()`)
- Entity placement testing
- Line of sight checks
- Projectile traces

### Client Renderer (r_bsp_model.c)
Client uses BSP for:
- Loading geometry for rendering
- Voxel data for clustered lighting
- Light definitions from BSP
- Spatial blocks for frustum culling

### Game Module (g_physics.c)
Game uses collision for:
- Player physics (`G_Physics_Move()`)
- Projectile ballistics
- Item placement
- Trigger detection

## Performance Notes

- **BSP traces are very fast** - O(log N) complexity due to tree structure
- **Box traces are slower than point traces** - Must test box corners against planes
- **Contents tests are cheap** - Simple tree walk to leaf
- **Inline models** - Each submodel must be traced separately (func_door, etc.)

## Debugging

### Console Variables
```
developer 1          # Enable debug output
cm_no_areas 1        # Disable area portals (see through everything)
```

### Trace Debugging
Add temporary debug output:
```c
const cm_trace_t tr = Cm_BoxTrace(start, end, mins, maxs, 0, mask);
Com_Debug(DEBUG_COLLISION, "Trace %.1f%% hit, plane %s\n", 
          tr.fraction * 100.0, VectorToString(tr.plane.normal));
```

## Common Issues

### Stuck in Geometry
- Trace starts inside solid - trace will return fraction 0.0
- Solution: Use `CM_CLIP_TO_BSP` flag or start trace offset from surface

### Falling Through Floor
- Often caused by incorrect bounding box (mins/maxs)
- Check that mins is negative, maxs is positive
- Standard player bbox: mins = {-16, -16, -24}, maxs = {16, 16, 32}

### No Collision with Submodels
- Inline models (*1, *2, etc.) require separate trace
- Use `Cm_TransformedBoxTrace()` with model's origin/angles

## File Format Notes

BSP files are little-endian binary with this structure:
1. Header (version, lump table)
2. Lumps (variable-size chunks of data)
3. Each lump: offset, length in header

See `cm_bsp.h` for complete format specification and `src/tools/quemap/bsp.c` for writer.
