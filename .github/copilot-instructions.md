# Quetoo Game Engine - Copilot Instructions

Quetoo is a free first-person shooter game engine and game derived from id Software's Quake II. It's written in C with OpenGL rendering, supporting macOS, Linux, BSD, and Windows.

## Build System

### Building the Engine

```bash
# Initial setup (after git clone)
autoreconf -i

# Configure (basic build)
./configure

# Configure with tests and master server
./configure --with-tests --with-master

# macOS with Homebrew (non-standard prefix)
./configure --with-homebrew=/opt/homebrew

# Build
make -j$(nproc)

# Install
sudo make install
```

### Installing Game Data

The engine requires game data from a separate repository:

```bash
git clone https://github.com/jdolan/quetoo-data.git
sudo ln -s quetoo-data/target /usr/local/share/quetoo
```

### Build System Details

- **Autotools-based**: Uses autoconf/automake with libtool
- **Subdirectories**: `deps/` (dependencies), `src/` (source), built in order
- **Platform detection**: Handled in `configure.ac` with per-platform CFLAGS/LDFLAGS
- **Git revision**: Automatically embedded in builds via `AC_DEFINE_UNQUOTED(REVISION, ...)`

## Testing

### Running Tests

Tests use the Check unit testing framework:

```bash
# Build with tests
./configure --with-tests
make -j$(nproc)

# Run all tests
make check

# Run specific test binary
./src/tests/check_vector
./src/tests/check_collision
```

Test files are in `src/tests/` with pattern `check_*.c`.

## Project Architecture

### High-Level Structure

```
src/
├── shared/      # Shared data structures, math, utilities (used by all modules)
├── common/      # Common engine code (console, cvars, filesystem, etc.)
├── collision/   # BSP collision detection and map loading
├── net/         # Network layer (client-server communication)
├── server/      # Game server (physics, game logic host)
├── game/        # Server-side game logic module (.so/.dll)
├── client/      # Client application (rendering, input, sound)
├── cgame/       # Client-side game logic module (.so/.dll)
├── ai/          # AI bot system
├── main/        # Main executable entry points
└── tools/       # Map compiler (quemap), other utilities
```

### Module System

The engine uses a modular architecture with dynamically loaded game logic:

- **game module** (`game/.so`): Server-side game logic (weapons, entities, scoring)
- **cgame module** (`cgame/.so`): Client-side game logic (HUD, view effects, prediction)
- Both modules communicate with engine via fixed API (`g_import_t`, `cgi_t`)

### Rendering Architecture

**Key components** (all in `src/client/renderer/`):

- **r_main.c**: Renderer initialization, frame setup, main render loop
- **r_bsp_model.c**: BSP/brush model loading from Quake II `.bsp` files
- **r_bsp_draw.c**: BSP rendering pipeline (geometry submission)
- **r_entity.c**: Entity (dynamic model) management
- **r_cull.c**: Frustum culling for BSP and entities
- **r_shadow.c**: Dynamic shadow mapping (6-pass cubemaps per light)
- **r_light.c**: Dynamic lighting system
- **r_material.c**: Material/texture management
- **shaders/**: GLSL shaders (bsp_*.glsl, mesh_*.glsl, etc.)

### Clustered Forward Lighting (Voxel-based)

The renderer uses a **compile-time voxel grid** for light culling:

1. **quemap** (map compiler) divides world into voxels and assigns lights to each voxel
2. Light assignments stored in BSP file (`BSP_LUMP_VOXELS`)
3. At runtime, shaders look up voxel data to determine which lights affect each fragment
4. Dramatically reduces per-fragment lighting cost vs. testing all lights

**Key files**:
- `src/tools/quemap/voxel.c` - Voxel grid building and light assignment
- `src/client/renderer/r_bsp_model.c` - Voxel data loading
- `src/client/renderer/shaders/common.glsl` - Voxel lookup functions
- `src/client/renderer/shaders/bsp_fs.glsl` - Fragment shader with voxel lighting
- `src/collision/cm_bsp.h` - BSP file format (`bsp_lump_t`, `BSP_LUMP_VOXELS`)

See `doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md` for detailed implementation.

### Network Architecture

- **Client-server model**: Even single-player runs local server
- **Protocol**: Quake II-derived UDP protocol with delta compression
- **Entity updates**: Delta-compressed entity state snapshots
- **Prediction**: Client-side movement prediction with server reconciliation

### BSP/Map Format

- **Quake II BSP format**: Enhanced with additional lumps (voxels, lights, etc.)
- **Map compiler** (`src/tools/quemap/`): Compiles `.map` → `.bsp`
  - `-bsp`: BSP tree generation
  - `-vis`: Visibility (PVS) calculation
  - `-light`: Static and dynamic light baking, voxel light assignment

## Key Conventions

### Code Style

- **Naming**: `Snake_case` for types, `camelCase` for functions/variables
- **Structs**: Typedef'd with `_t` suffix (e.g., `vec3_t`, `entity_state_t`)
- **OpenGL**: Modern OpenGL 4.1 Core Profile (macOS compatibility)
- **Prefixes**: 
  - `r_` = renderer
  - `cg_` = client game
  - `g_` = server game
  - `sv_` = server
  - `cl_` = client
  - `cm_` = collision model

### Memory Management

- **Manual allocation**: `Mem_Malloc()`, `Mem_Free()` wrappers around system allocator
- **Tagged allocation**: Memory pools with tags for subsystem cleanup
- **No automatic cleanup**: Explicitly free all allocated memory

### Vector Math

- **vec3_t**: Float[3] array, passed by reference
- **Macros**: `VectorCopy()`, `VectorAdd()`, `VectorScale()`, etc. in `src/shared/shared.h`
- **Functions**: `Vec3_*()` functions in `src/shared/vector.c` for complex operations
- **Box3**: AABB type with min/max vectors (`src/shared/box.h`)

### Entity State Management

**Critical pattern**: Entity slots are reused. Always validate state consistency:

```c
if (ent->current.spawn_id != s->spawn_id) {
    ent->current = *s;  // Reset entity on spawn_id change
}
```

`spawn_id` increments on entity reuse to detect stale data. See `doc/copilot/ENTITY_STATE_BUG_FIX.md`.

### Shader Conventions

- **Separate compilation**: Shaders compiled at runtime, not embedded
- **Shader programs**: Named by stage (e.g., `bsp`, `mesh`, `shadow`)
- **Uniforms**: Bound per-frame or per-draw as needed
- **Vertex attributes**: Position, normal, tangent, texcoords, color
- **Texture units**: Material, normalmap, glossmap, lightmap, etc.

### LOD and Performance

Use distance-based quality scaling in shaders:

```glsl
if (fragment.lod > LOD_THRESHOLD) {
    // Skip expensive operations at distance
    return;
}

int samples = int(mix(MAX_SAMPLES_CLOSE, MIN_SAMPLES_FAR, 
                     min(fragment.lod * SCALE, 1.0)));
```

## Optimization Patterns

### Parallax Occlusion Mapping

- **Sample count**: 24-32 samples (industry standard, not 96+)
- **Distance disable**: Skip POM beyond LOD 4.0
- **TBN in vertex shader**: Amortize matrix math across fragments

See `doc/copilot/PARALLAX_FINAL_RESULTS.md`.

### Shadow Mapping

- **Multi-pass rendering**: 6 passes per light (one per cubemap face)
- **Avoid geometry shaders**: Slower than multiple draw calls on most hardware
- **Early culling**: Distance check before expensive bounds tests
- **Depth clamp**: Use `GL_DEPTH_CLAMP` instead of manual clamping

See `doc/copilot/SHADOWMAP_OPTIMIZATION.md`.

### Frustum Culling

**Critical**: Use half FOV, not full FOV:

```c
const float fov_y = Radians(cgi.view->fov_y * 0.5);  // HALF FOV
```

See `doc/copilot/FRUSTUM_BUG_FIX.md`.

## Platform Considerations

### macOS

- **OpenGL 4.1 Core Profile**: Maximum version available on macOS
- **Homebrew dependencies**: Use `--with-homebrew` configure flag
- **Framework linking**: Special handling for SDL2.framework, etc.

### Linux/BSD

- **Standard paths**: Works with system-installed dependencies
- **Package managers**: apt, yum, pacman, pkg

### Windows

- **MinGW cross-compile**: From Linux/macOS (see `mingw-cross/`)
- **Visual Studio**: Projects in `Quetoo.vs15/` (may be out of date)

## Documentation

### Previous AI Sessions

Extensive documentation from prior Copilot sessions in `doc/copilot/`:

- **Start here**: `SESSION_SUMMARY.md` - Overview of November 2024 optimization session
- **Quick reference**: `INDEX.md` - All optimization/bugfix docs indexed
- **Latest context**: `SESSION_CONTEXT.md` - Most recent work (voxel lighting)

**Always read relevant docs before modifying renderer code.**

### Finding Documentation

```bash
# List all copilot session docs
ls doc/copilot/*.md

# Search for specific topics
grep -r "shadow" doc/copilot/
grep -r "entity" doc/copilot/
```

### Performance Results from Previous Work

- Shadow mapping: 55-85% faster
- Parallax occlusion: 45-50% faster
- Parallax self-shadow: 99% cost reduction (4x → ~2x frame rate overall)

## Common Tasks

### Adding a New Renderer Feature

1. Add data structures to `src/client/renderer/r_types.h`
2. Implement in appropriate `r_*.c` file
3. Add shader code if needed in `src/client/renderer/shaders/`
4. Update initialization in `r_main.c::R_Init()`
5. Update shutdown in `r_main.c::R_Shutdown()`

### Modifying the BSP Format

1. Update `src/collision/cm_bsp.h` (lump IDs, structures)
2. Update quemap writer: `src/tools/quemap/bsp.c::WriteBSPFile()`
3. Update engine reader: `src/collision/cm_bsp.c` or `src/client/renderer/r_bsp_model.c`
4. Increment `BSP_VERSION` if format breaks compatibility

### Adding a Console Variable

```c
// In initialization function
cvar_t *my_var = cgi.AddCvar("r_my_feature", "1", CVAR_ARCHIVE, "Description");

// Access in code
if (my_var->integer) {
    // ...
}
```

### Debugging

- **Console commands**: `developer 1` for verbose output, `r_show_*` cvars for debug visualization
- **In-game console**: `~ ` key opens console
- **Server/client logs**: Check stdout/stderr or log files

## Dependencies

### Required Libraries

From `README.md` and `configure.ac`:

- **ObjectivelyMVC**: Custom UI framework
- **PhysicsFS**: Virtual filesystem
- **OpenAL**: 3D audio
- **libsndfile**: Audio format loading
- **glib2**: Utility library
- **ncurses**: Text-mode UI (server console)
- **SDL2**: Window/input/context management
- **libcurl**: HTTP downloads (updater)

All dependencies are in `deps/` or system-installed via pkg-config.

## Subsystem Documentation

Detailed documentation for each major subsystem:

- **[Collision System](.github/subsystems/collision.md)** - BSP loading, traces, spatial queries
- **[Network Layer](.github/subsystems/network.md)** - UDP/TCP, netchan, delta compression
- **[Server System](.github/subsystems/server.md)** - Game hosting, client management, world state
- **[Game Module](.github/subsystems/game-module.md)** - Server-side gameplay logic, weapons, items, physics
- **[CGame Module](.github/subsystems/cgame-module.md)** - Client-side presentation, HUD, prediction, effects
- **[Renderer](.github/subsystems/renderer.md)** - OpenGL 4.1 rendering, lighting, shadows, materials
- **[Common Systems](.github/subsystems/common.md)** - Console, cvars, filesystem, memory, threading
- **[AI / Bots](.github/subsystems/ai.md)** - Bot AI, pathfinding, combat, goals
- **[Map Compiler](.github/subsystems/quemap.md)** - BSP compilation, VIS, lighting, voxel generation

## Links

- **Website**: http://quetoo.org
- **Repository**: https://github.com/jdolan/quetoo
- **Game data**: https://github.com/jdolan/quetoo-data
- **Discord**: https://discord.gg/unb9U4b
