# Clustered/Voxel-Forward Lighting Implementation

## Overview

This document describes the implementation of clustered/voxel-forward lighting in Quetoo. The system uses a 3D grid that maps world space into voxel cells, with each cell storing a list of lights that affect it. This dramatically reduces lighting overhead by only processing lights that actually affect each fragment.

## Architecture

### Data Flow

1. **Compile-time (quemap)**:
   - Light sources are analyzed against the voxel grid
   - For each voxel cell, determine which lights overlap it
   - Generate two arrays:
     - `meta`: Per-cell (offset, count) pairs into the index array
     - `indices`: Flattened list of light indices
   - Write light grid data to BSP file in `BSP_LUMP_LIGHTGRID` lump

2. **Load-time (renderer)**:
   - Load light grid data from BSP file
   - Upload to GPU as:
     - 3D integer texture (RG32I) for meta data
     - Texture buffer (R32I) for indices
     - Texture buffers (RGBA32F) for light parameters

3. **Runtime (shaders)**:
   - Fragment determines its voxel cell based on world position
   - Fetch (offset, count) from 3D meta texture
   - Loop through indices to get light IDs
   - Fetch light parameters from TBOs
   - Apply lighting calculations

## File Changes

### BSP File Format (`src/collision/cm_bsp.c`, `src/collision/cm_bsp.h`)

**Added**:
- `BSP_LUMP_LIGHTGRID` enum value to `bsp_lump_id_t`
- `bsp_lightgrid_header_t` structure with grid dimensions and index count
- Entry in `bsp_lump_meta` array for lightgrid lump
- `MAX_BSP_LIGHTGRID_SIZE` constant (8MB)

**lightgrid and voxels fields in `bsp_file_t`**:
```c
int32_t lightgrid_size;
byte *lightgrid;
```

### Quemap Compiler (`src/tools/quemap/light.c`)

**Added**:
- `EmitLightGrid()` function that:
  - Iterates through all lights
  - Transforms light bounds to voxel space
  - Determines which cells each light affects
  - Builds prefix-sum index structure
  - Writes binary blob to `bsp_file.lightgrid`

### Renderer - BSP Model Loading (`src/client/renderer/r_bsp_model.c`)

**Added**:
- `R_LoadBspLightGrid()` function that:
  - Tries to load pre-computed light grid from BSP file
  - Falls back to runtime generation if not found
  - Copies data into renderer structures

**Modified**:
- `R_BSP_LUMPS` mask to include `BSP_LUMP_LIGHTGRID`
- `r_bsp_upload_light_grid()` to upload GPU resources:
  - Light grid index texture buffer (R32I)
  - Light grid meta 3D texture (RG32I) 
  - Per-light origin/radius TBO (RGBA32F)
  - Per-light color/intensity TBO (RGBA32F)

### Renderer - Types (`src/client/renderer/r_types.h`)

**Added texture slots**:
- `TEXTURE_LIGHT_GRID_META`
- `TEXTURE_LIGHT_GRID_INDICES`
- `TEXTURE_LIGHT_ORIGIN`
- `TEXTURE_LIGHT_COLOR`

**r_bsp_model_t fields** (already existed):
```c
int32_t *light_grid_meta;           // CPU-side meta array
int32_t *light_grid_indices;        // CPU-side indices
GLuint light_grid_meta_tex;         // 3D texture
GLuint light_grid_index_tex;        // TBO for indices
GLuint light_grid_light_origin_tex; // TBO for origins
GLuint light_grid_light_color_tex;  // TBO for colors
```

### Renderer - BSP Drawing (`src/client/renderer/r_bsp_draw.c`)

**Added to program structure**:
```c
GLint texture_light_grid_meta;
GLint texture_light_grid_indices;
GLint texture_light_origin;
GLint texture_light_color;
GLint use_light_grid;
```

**Modified**:
- `R_InitBspProgram()`: Get uniform locations and bind texture units
- `R_DrawOpaqueBspInlineEntities()`: Bind light grid textures before drawing
- `R_DrawBlendBspInlineEntities()`: Bind light grid textures before drawing

### Shaders

#### Common Functions (`shaders/common.glsl`)

**Added**:
- `light_grid_cell(vec3 position)`: Convert world position to cell coordinates
- `light_grid_meta(ivec3 cell)`: Fetch (offset, count) for a cell
- `light_grid_index(int index)`: Fetch light index from flattened array
- `light_grid_light(int light_index)`: Construct `light_t` from TBO data

#### Uniforms (`shaders/uniforms.glsl`)

**Already existed**:
```glsl
uniform isampler3D texture_light_grid_meta;
uniform isamplerBuffer texture_light_grid_indices;
uniform samplerBuffer texture_light_origin;
uniform samplerBuffer texture_light_color;
uniform int use_light_grid;
```

#### BSP Fragment Shader (`shaders/bsp_fs.glsl`)

**Modified** `light_and_shadow()`:
- Check `use_light_grid` flag
- If enabled: use clustered path with `light_grid_*()` functions
- If disabled: use traditional `active_lights[]` array

**Modified** `sample_shadow_cubemap_array()`:
- Return 1.0 (no shadow) if index < 0
- Allows light grid lights to work without shadow maps

#### Mesh Fragment Shader (`shaders/mesh_fs.glsl`)

**Same changes as bsp_fs.glsl**:
- Modified `light_and_shadow()`
- Modified `sample_shadow_cubemap_array()`

## How It Works

### Voxel Space Calculation

The light grid uses the same voxel space as the global illumination system:

```c
// In quemap (tools-side):
vec3_t stu_pos = Mat4_Transform(voxels.matrix, world_pos);
vec3_t rel = Vec3_Subtract(stu_pos, voxels.stu_bounds.mins);
ivec3 cell = (iz * vy + iy) * vx + ix;
```

```glsl
// In shader (GPU-side):
vec3 rel_pos = position - voxels.mins.xyz;
ivec3 cell = ivec3(floor(rel_pos / voxels.voxel_size.xyz));
```

### Light Grid Lookup

For each fragment:

1. Calculate voxel cell: `ivec3 cell = light_grid_cell(vertex.model)`
2. Fetch metadata: `ivec2 meta = light_grid_meta(cell)` → (offset, count)
3. Loop through lights:
   ```glsl
   for (int i = 0; i < count; i++) {
     int light_id = light_grid_index(offset + i);
     light_t light = light_grid_light(light_id);
     // Apply lighting...
   }
   ```

### Memory Layout

**BSP File lightgrid lump**:
```
[header: 16 bytes]
  - size_x, size_y, size_z (int32_t each)
  - total_indices (int32_t)
[meta: num_cells * 8 bytes]
  - offset, count pairs (int32_t each)
[indices: total_indices * 4 bytes]
  - light indices (int32_t each)
```

**GPU Resources**:
- Meta texture: `GL_TEXTURE_3D`, `GL_RG32I`, size (vx, vy, vz)
- Index TBO: `GL_TEXTURE_BUFFER`, `GL_R32I`, size total_indices
- Origin TBO: `GL_TEXTURE_BUFFER`, `GL_RGBA32F`, size num_lights
- Color TBO: `GL_TEXTURE_BUFFER`, `GL_RGBA32F`, size num_lights

## Performance Characteristics

### Benefits

1. **Reduced fragment shader cost**: Only process lights that affect each cell
2. **Scalable**: Performance independent of total light count
3. **Predictable**: Each fragment processes at most ~8-16 lights
4. **Cache-friendly**: Adjacent fragments use same cell data

### Tradeoffs

1. **Memory**: Requires ~8MB for light grid in BSP file
2. **No shadows**: Light grid lights currently don't cast shadows (index = -1)
3. **Fallback path**: Still supports traditional active_lights[] for compatibility

## Future Enhancements

1. **Shadow support**: Store shadow map indices in light grid
2. **Dynamic lights**: Update light grid at runtime for moving lights
3. **Light importance**: Sort lights by contribution within each cell
4. **Temporal coherence**: Reuse previous frame results for stable lighting

## Testing

To verify the system is working:

1. Compile a map: `quemap -bsp maps/test.map`
   - Should see: "Emitted light grid: cells=X indices=Y"

2. Load the map in Quetoo
   - Renderer should print: "Loaded light grid from BSP: cells=X indices=Y"

3. Check GPU resources are created (in debug mode):
   - `light_grid_meta_tex` should be non-zero
   - `light_grid_index_tex` should be non-zero
   - `use_light_grid` uniform should be 1

4. Visual verification:
   - Lighting should look identical to traditional path
   - Performance should be better with many lights

## Implementation Date

December 28-29, 2024

## Authors

Implementation by GitHub Copilot CLI with guidance from Jay Dolan.
