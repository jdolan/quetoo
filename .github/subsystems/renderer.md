# Renderer System (client/renderer/)

OpenGL 4.1 Core Profile renderer with BSP rendering, dynamic lighting, shadow mapping, and modern shader effects.

## Location

`src/client/renderer/` - Client-side rendering subsystem

## Core Purpose

- Render 3D world (BSP geometry, entities, particles)
- Dynamic lighting with clustered forward shading
- Shadow mapping (cubemap shadows per light)
- Material system (diffuse, normal, specular, parallax)
- Post-processing effects
- 2D UI rendering

## Key Architecture

**See `doc/copilot/` for extensive renderer documentation from previous optimization work:**
- `SESSION_SUMMARY.md` - Overview of renderer architecture and optimizations
- `CLUSTERED_LIGHTING_IMPLEMENTATION.md` - Voxel-based light culling system
- `SHADOWMAP_OPTIMIZATION.md` - Shadow rendering techniques
- `PARALLAX_FINAL_RESULTS.md` - Parallax occlusion mapping

## Important Files

### Core
- `r_main.c` - Initialization, main render loop, frame setup
- `r_context.c` - OpenGL context management, extensions
- `r_program.c` - GLSL shader program management
- `r_types.h` - All renderer data structures

### BSP Rendering
- `r_bsp_model.c` - BSP/voxel loading from .bsp files
- `r_bsp_draw.c` - BSP geometry rendering pipeline
- `r_bsp_light.c` - BSP lightmap management
- `r_cull.c` - Frustum culling (BSP faces, entities)

### Lighting & Shadows
- `r_light.c` - Dynamic light management
- `r_shadow.c` - Shadow map rendering (6-pass cubemaps)
- Uses **voxel-based clustered forward lighting** (compile-time light culling)

### Materials & Textures
- `r_material.c` - Material loading and management
- `r_image.c` - Texture loading (TGA, PNG, JPG)
- `r_atlas.c` - Texture atlas for small images

### Entity Rendering
- `r_entity.c` - Entity management and rendering
- `r_model.c` - Model loading (MD3, OBJ formats)
- `r_animation.c` - Skeletal animation (MD3 tags)
- `r_mesh_model.c` - Mesh model loading
- `r_mesh_draw.c` - Mesh rendering

### Effects
- `r_depth_pass.c` - Depth pre-pass
- `r_draw_2d.c` - 2D/UI rendering (HUD, console, menus)
- `r_draw_3d.c` - 3D helper functions (debug lines, boxes)

### Shaders (shaders/)
- `bsp_vs.glsl` / `bsp_fs.glsl` - BSP geometry shaders
- `mesh_vs.glsl` / `mesh_fs.glsl` - Entity/model shaders
- `shadow_vs.glsl` / `shadow_fs.glsl` - Shadow map generation
- `common.glsl` - Shared functions (voxel lookup, lighting)

## Clustered Forward Lighting

**Voxel-based light culling** (compile-time):
1. Map compiler (quemap) divides world into 32-unit voxels
2. Each voxel stores indices of lights affecting it
3. Shaders look up voxel at fragment position
4. Only test lights assigned to that voxel (10-20 lights instead of 512+)

**Result**: 5-10x faster lighting vs testing all lights.

See `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md` for implementation details.

## Shadow Mapping

**Per-light cubemap shadows**:
- Each light gets 6 shadow maps (one per cubemap face)
- Rendered in 6 passes (geometry shader removed for performance)
- Early culling: distance + sphere tests before rendering
- Depth clamping for better quality

**Optimizations**:
- 55-85% faster than previous implementation
- See `doc/copilot/SHADOWMAP_OPTIMIZATION.md`

## Material System

Materials define surface appearance:
```c
typedef struct {
    char name[64];
    r_image_t *diffusemap;      // Base color/texture
    r_image_t *normalmap;       // Normal map (bump detail)
    r_image_t *glossmap;        // Specular/gloss (roughness)
    r_image_t *tintmap;         // Color tint modulation
    vec3_t tint;                // Tint color
    float roughness;            // Surface roughness (0=shiny, 1=matte)
    float hardness;             // Specular hardness
    float parallax;             // Parallax height scale
    uint32_t flags;             // Render flags
} r_material_t;
```

## Rendering Pipeline

**Each frame**:
1. **Setup**: Clear buffers, update uniforms (view matrix, time, etc.)
2. **Shadow pass**: Render scene from each light's perspective (depth only)
3. **Depth pre-pass**: Render opaque geometry (depth only)
4. **Opaque pass**: Render opaque geometry with lighting
5. **Transparent pass**: Render transparent geometry (sorted back-to-front)
6. **Post-processing**: Bloom, color grading, etc. (TODO)
7. **2D pass**: Render HUD, console, menus

## Frustum Culling

BSP faces and entities culled against view frustum:
- **Critical**: Use half FOV, not full FOV (see `doc/copilot/FRUSTUM_BUG_FIX.md`)
- Early rejection via bounding boxes
- BSP block system for coarse culling

## Shader Conventions

### Vertex Attributes
```glsl
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec3 in_Tangent;
layout(location = 3) in vec3 in_Bitangent;
layout(location = 4) in vec2 in_Texcoord;
layout(location = 5) in vec4 in_Color;
```

### Uniforms
```glsl
uniform mat4 model;              // Model matrix
uniform mat4 view;               // View matrix
uniform mat4 projection;         // Projection matrix
uniform vec3 view_origin;        // Camera position
uniform float time;              // Game time (animations)
```

### Lighting
```glsl
// Voxel lookup for lights
ivec3 voxel = GetVoxel(fragment.position);
int light_count = GetVoxelLightCount(voxel);
for (int i = 0; i < light_count; i++) {
    int light_index = GetVoxelLight(voxel, i);
    Light light = lights[light_index];
    // ... apply lighting
}
```

## OpenGL Version

**OpenGL 4.1 Core Profile** (macOS limitation):
- No geometry shaders in hot paths (slow on macOS)
- No compute shaders
- No tessellation
- Modern enough for most effects

## Console Variables

```
r_width 1920              # Viewport width
r_height 1080             # Viewport height
r_fullscreen 0            # Fullscreen mode
r_max_fps 0               # FPS cap (0=unlimited)
r_vsync 1                 # V-sync

r_cull 1                  # Frustum culling
r_draw_bsp 1              # Render BSP
r_draw_entities 1         # Render entities
r_draw_blend 1            # Render transparent surfaces

r_lighting 1              # Dynamic lighting
r_shadows 1               # Shadow mapping
r_shadow_quality 2        # Shadow map resolution (0-3)

r_parallax 1              # Parallax occlusion mapping
r_parallax_samples 24     # POM sample count (16-32 recommended)

r_show_bsp_normals 0      # Debug: visualize normals
r_show_bsp_lightmaps 0    # Debug: show lightmaps only
r_show_bsp_textures 1     # Show textures (vs lightmaps only)
```

## Performance Optimization Patterns

### LOD-Based Quality
```glsl
// Skip expensive effects at distance
if (fragment.lod > 4.0) {
    return diffuse_only;  // No POM, self-shadow
}
```

### Early Culling
```c
// Distance check before expensive bounds test
const float dist = Vec3_Distance(light->origin, Box3_Center(bounds));
if (dist > light->radius + radius) {
    continue;
}
```

### Sample Count Reduction
- Parallax: 24-32 samples (not 96+)
- Self-shadow: 4-16 steps (LOD-based)
- Industry-standard values are sufficient

See `doc/copilot/PARALLAX_FINAL_RESULTS.md` for detailed analysis.

## Common Issues

### Performance Drops
- Check `r_shadows` - shadows are expensive
- Reduce `r_parallax_samples` to 16
- Disable `r_parallax` if still slow

### Visual Artifacts
- **Shadow acne**: Adjust polygon offset in r_shadow.c
- **Light popping**: Voxel boundary issue (fixed, see `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md`)
- **Z-fighting**: Coplanar surfaces need depth offset

### Missing Textures
- Pink/purple = missing texture
- Check material paths in .mat files
- Ensure textures in pk3 or filesystem

## Debugging

```
r_show_bsp_normals 1      # Visualize surface normals
r_show_bsp_lightmaps 1    # Show lightmaps only
r_draw_wireframe 1        # Wireframe mode
developer 1               # Debug output
```

## Integration Points

### CGame Module
- CGame adds entities, lights, sprites via `cgi.AddEntity()`, etc.
- Renderer culls and renders them
- CGame doesn't touch OpenGL directly

### BSP Compiler (quemap)
- Compiles voxel light assignments
- Generates lightmaps
- Outputs .bsp with all rendering data

## Further Reading

**For deep renderer understanding, read these in order**:
1. `doc/copilot/SESSION_SUMMARY.md` - High-level overview
2. `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md` - Lighting system
3. `doc/copilot/SHADOWMAP_OPTIMIZATION.md` - Shadow techniques
4. `doc/copilot/PARALLAX_FINAL_RESULTS.md` - Material effects
5. `doc/copilot/FRUSTUM_BUG_FIX.md` - Culling details

These docs contain detailed implementation notes, performance measurements, and lessons learned from extensive optimization work.
