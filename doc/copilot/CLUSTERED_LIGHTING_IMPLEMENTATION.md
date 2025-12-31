# Clustered/Voxel-Forward Lighting Implementation

## Overview

This document describes the implementation of clustered/voxel-forward lighting in Quetoo. The system uses a 3D grid that maps world space into voxel cells, with each cell storing a list of lights that affect it. This dramatically reduces lighting overhead by only processing lights that actually affect each fragment.

**Note:** The light grid data is stored in the same voxels structure used for global illumination (`BSP_LUMP_VOXELS`), not in a separate `BSP_LUMP_LIGHTGRID` lump. The implementation reuses the existing voxel grid infrastructure.

## Architecture

### Data Flow

1. **Compile-time (quemap)**:
   - Light sources are analyzed against the voxel grid
   - For each voxel cell, determine which lights overlap it using sphere-box intersection test
   - Each voxel tracks its lights via a hash table
   - Generate two arrays:
     - `meta`: Per-cell (offset, count) pairs into the index array
     - `indices`: Flattened list of light indices
   - Write light grid data to BSP file in `BSP_LUMP_VOXELS` lump (combined with GI data)

2. **Load-time (renderer)**:
   - Load voxel data from BSP file (includes light indices)
   - Upload to GPU as:
     - 3D integer texture (RG32I) for meta data (offset, count)
     - Texture buffer (R32I) for light indices
     - Voxel diffuse and fog textures for GI
   - Light parameters are uploaded separately in the active lights system

3. **Runtime (shaders)**:
   - Fragment determines its voxel cell based on world position
   - Fetch (offset, count) from 3D meta texture using `voxel_light_data()`
   - Loop through indices to get light IDs using `voxel_light_index()`
   - Fetch light parameters from the existing light uniform arrays
   - Apply lighting calculations

## File Changes

### BSP File Format (`src/collision/cm_bsp.c`, `src/collision/cm_bsp.h`)

**No changes required** - Light grid data is stored within the existing `BSP_LUMP_VOXELS` lump structure.

The `bsp_voxels_t` structure contains:
- Grid dimensions (size.x, size.y, size.z)
- Number of light indices
- Diffuse color data for each voxel
- Fog data for each voxel
- Light index metadata (offset, count) for each voxel
- Flattened light index array

### Quemap Compiler (`src/tools/quemap/voxel.c`)

**Key function: `LightVoxel_()`**
- Iterates through all lights for each voxel
- Uses **sphere-box intersection test** to determine light-voxel overlap
  - Finds closest point on voxel bounding box to light origin via `Box3_ClampPoint()`
  - Measures distance from light to closest point
  - Light affects voxel if distance < light radius
- Calls `IlluminateVoxel()` to add light to voxel's hash table
- Attenuation still uses center-to-center distance for smooth falloff

**Bug Fix (December 31, 2024):**
Previously used point-to-point distance (light origin to voxel center), which caused hard lighting boundaries at voxel edges when lights partially overlapped voxels. Fixed by using proper sphere-box intersection test.

**Function: `EmitVoxels()`**
- Writes voxel data to BSP file including:
  - Diffuse and fog color arrays
  - Light index metadata (offset, count per voxel)
  - Flattened light index array
- All data written to `BSP_LUMP_VOXELS`

### Renderer - BSP Model Loading (`src/client/renderer/r_bsp_model.c`)

**Function: `R_LoadBspVoxels()`**
- Loads voxel data from `BSP_LUMP_VOXELS`
- Extracts light index metadata and indices
- Calculates padded bounds to center fixed-size voxel grid around irregular world
- Creates GPU textures for:
  - Voxel diffuse (3D texture for GI)
  - Voxel fog (3D texture)
  - Voxel light metadata (3D texture, RG32I format)
  - Voxel light indices (texture buffer, R32I format)

**Padding calculation:**
```c
const vec3_t voxels_size = Vec3_Scale(Vec3i_CastVec3(out->size), BSP_VOXEL_SIZE);
const vec3_t world_size = Box3_Size(mod->bsp->inline_models->visible_bounds);
const vec3_t padding = Vec3_Scale(Vec3_Subtract(voxels_size, world_size), 0.5f);
out->bounds = Box3_Expand3(mod->bsp->inline_models->visible_bounds, padding);
```

This padding ensures the voxel grid (which has fixed-size cells) properly encompasses the world geometry.

### Renderer - Types (`src/client/renderer/r_types.h`)

**Texture slots for voxel light data:**
- `TEXTURE_VOXEL_LIGHT_DATA` - 3D texture (RG32I) for metadata
- `TEXTURE_VOXEL_LIGHT_INDICES` - Texture buffer (R32I) for indices

**r_bsp_voxels_t structure** (within r_bsp_model_t):
```c
vec3i_t size;                    // Grid dimensions
size_t num_voxels;               // Total voxel count
size_t num_light_indices;        // Total light index count
box3_t bounds;                   // Padded world-space bounds
GLuint light_data_texture;       // 3D texture for (offset, count)
GLuint light_indices_buffer;     // TBO for light indices
r_image_t *diffuse;              // Voxel GI diffuse texture
r_image_t *fog;                  // Voxel fog texture
```

### Renderer - BSP Drawing (`src/client/renderer/r_bsp_draw.c`)

The renderer binds the voxel light textures when drawing BSP geometry. Light parameters come from the existing active lights system, not from separate TBOs.

### Shaders

### Shaders

#### Common Functions (`shaders/common.glsl`)

**Voxel lookup functions:**
- `voxel_xyz(vec3 position)`: Convert world position to voxel cell coordinates
  - Subtracts padded voxel bounds minimum
  - Divides by `BSP_VOXEL_SIZE` (32.0)
  - Clamps to valid grid range
- `voxel_light_data(ivec3 cell)`: Fetch (offset, count) metadata for a cell
- `voxel_light_index(int index)`: Fetch light index from flattened array

**Note:** The padding calculated in quemap matches the padding used in the renderer, ensuring correct voxel-to-world mapping.

#### Uniforms (`shaders/uniforms.glsl`)

**Voxel-related uniforms:**
```glsl
uniform isampler3D texture_voxel_light_data;    // RG32I: (offset, count)
uniform isamplerBuffer texture_voxel_light_indices; // R32I: light indices
```

The `voxels_t` uniform block contains bounds and size information used for coordinate calculations.

#### BSP Fragment Shader (`shaders/bsp_fs.glsl`)

**Modified** `light_and_shadow()`:
- Gets voxel cell for fragment position: `ivec3 voxel = voxel_xyz(vertex.model)`
- Fetches light metadata: `ivec2 data = voxel_light_data(voxel)` 
- Loops through voxel's light indices
- Fetches each light from existing light arrays and applies lighting
- Also processes dynamic lights from `dynamic_lights[]` array

**Light parameters** come from the existing active lights system (uniforms), not from dedicated TBOs.

#### Mesh Fragment Shader (`shaders/mesh_fs.glsl`)

**Same voxel light grid integration as bsp_fs.glsl:**
- Uses `voxel_xyz()`, `voxel_light_data()`, and `voxel_light_index()`
- Processes voxel lights plus dynamic lights

## How It Works

### Voxel Space Calculation

The voxel grid uses a padded bounding box to center fixed-size voxel cells around irregular world geometry:

**In quemap (voxel.c):**
```c
// Transform world bounds to voxel-normalized space
voxels.matrix = translate(-world.mins) * scale(1/BSP_VOXEL_SIZE)
voxels.stu_bounds = transform(voxels.matrix, world.visible_bounds)

// Size includes padding: floor(extent) + 2
size = floor(stu_bounds.max - stu_bounds.min) + 2

// Padding in STU space (negative, shifts voxels inward)
padding_stu = (stu_bounds_size - size) * 0.5

// Voxel center in STU space
stu = stu_bounds.mins + padding_stu + (s, t, u) + 0.5
world_origin = inverse_transform(stu)
```

**In renderer (r_bsp_model.c):**
```c
// Calculate padding in world space
voxels_size = size * BSP_VOXEL_SIZE  
padding_world = (voxels_size - world_size) * 0.5
padded_bounds = expand(visible_bounds, padding_world)
```

**In shader (common.glsl):**
```glsl
// Padded bounds uploaded as voxels.mins/maxs
ivec3 cell = ivec3(floor((position - voxels.mins.xyz) / BSP_VOXEL_SIZE))
```

The padding ensures that voxel centers align correctly with the world geometry regardless of the world's size relative to the fixed voxel dimensions.

### Light-Voxel Assignment (Sphere-Box Intersection)

For each voxel during lighting in quemap:

```c
// Find closest point on voxel box to light origin
vec3_t closest_point = Box3_ClampPoint(voxel->bounds, light->origin);
float dist_to_box = Vec3_Distance(light->origin, closest_point);

// Light affects voxel if sphere overlaps box
if (dist_to_box < light->radius) {
  // Add light to voxel's list
  IlluminateVoxel(voxel, light, lumens);
}
```

This ensures lights are assigned to **all** voxels they overlap, eliminating hard boundaries. Attenuation still uses center-to-center distance for smooth falloff:

```c
float dist_to_center = Vec3_Distance(light->origin, voxel->origin);
float atten = clamp(1.0 - dist_to_center / light->radius, 0.0, 1.0);
float lumens = atten * atten * scale;
```

### Voxel Light Lookup (Runtime)

For each fragment in the shader:

1. Calculate voxel cell: `ivec3 voxel = voxel_xyz(vertex.model)`
2. Fetch metadata: `ivec2 data = voxel_light_data(voxel)` → (offset, count)
3. Loop through lights:
   ```glsl
   for (int i = 0; i < data.y; i++) {
     int light_index = voxel_light_index(data.x + i);
     // Fetch light parameters from active_lights array
     // Apply lighting calculation
   }
   ```

### Memory Layout

**BSP File voxels lump** (`BSP_LUMP_VOXELS`):
```
[header: bsp_voxels_t]
  - size (vec3i_t): grid dimensions
  - num_light_indices (int32_t)
[diffuse: num_voxels * sizeof(color32_t)]
  - RGBA color data for GI
[fog: num_voxels * sizeof(color32_t)]
  - RGBA fog data
[meta: num_voxels * 2 * sizeof(int32_t)]
  - offset, count pairs for light indices
[indices: num_light_indices * sizeof(int32_t)]
  - flattened light index array
```

**GPU Resources**:
- Diffuse texture: `GL_TEXTURE_3D`, `GL_RGBA8`, size (vx, vy, vz)
- Fog texture: `GL_TEXTURE_3D`, `GL_RGBA8`, size (vx, vy, vz)
- Light data texture: `GL_TEXTURE_3D`, `GL_RG32I`, size (vx, vy, vz)
- Light indices TBO: `GL_TEXTURE_BUFFER`, `GL_R32I`, size num_light_indices

## Performance Characteristics

### Benefits

1. **Reduced fragment shader cost**: Only process lights that affect each voxel cell
2. **Scalable**: Performance largely independent of total light count
3. **Predictable**: Each fragment processes at most ~8-16 lights (typical)
4. **Cache-friendly**: Adjacent fragments use same voxel cell data
5. **Shared infrastructure**: Reuses voxel grid from GI system

### Considerations

1. **Memory**: Voxel data stored in BSP file (size depends on world bounds and light count)
2. **Static assignment**: Light-voxel assignments baked at compile time
3. **Shadow support**: Lights can use shadow maps from existing shadow system

## Known Issues and Fixes

### Voxel Boundary Artifacts (FIXED: December 31, 2024)

**Problem:** Hard lighting transitions visible at voxel boundaries where lights would suddenly start/stop contributing.

**Cause:** The `LightVoxel_()` function used point-to-point distance test (light origin to voxel center) to determine if a light should affect a voxel. This incorrectly excluded lights that partially overlapped voxel boxes.

**Fix:** Changed to sphere-box intersection test using `Box3_ClampPoint()` to find the closest point on the voxel's bounding box to the light origin, then measuring that distance. Light now affects voxel if `distance < light_radius`, ensuring all overlapping voxels are included.

**File:** `src/tools/quemap/voxel.c`, function `LightVoxel_()`

## Future Enhancements

1. **Dynamic lights**: Currently only static (baked) lights use voxel grid; dynamic lights processed separately
2. **Light importance**: Could sort lights by contribution within each voxel
3. **Adaptive grid**: Variable resolution voxel grid based on geometry density
4. **Temporal coherence**: Could cache/reuse previous frame voxel data for animated lights

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
