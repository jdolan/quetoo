# Quetoo Documentation Index

Complete guide to Quetoo engine documentation for developers and AI assistants.

## Quick Start

**New to Quetoo?** Start here:
1. [Main Instructions](../copilot-instructions.md) - Overview, build system, architecture
2. Pick a subsystem below based on what you're working on

## Core Documentation

### Main Instructions
**[copilot-instructions.md](../copilot-instructions.md)** - Primary reference
- Build, test, and lint commands
- High-level architecture overview
- Key coding conventions
- Platform considerations
- Links to all subsystem docs

## Subsystem Documentation

### Engine Core

**[Collision System](collision.md)** - BSP and spatial queries
- BSP file loading and format
- Collision tracing (raycasts, box traces)
- Point/contents queries
- Entity vs world collision

**[Network Layer](network.md)** - UDP/TCP networking
- Reliable messaging (netchan)
- Delta compression
- Protocol details
- Master server communication

**[Common Systems](common.md)** - Shared utilities
- Console and cvars
- Filesystem (PhysicsFS)
- Memory management
- Threading

### Server-Side

**[Server System](server.md)** - Game hosting
- Client connection management
- Entity snapshot system
- PVS (Potentially Visible Set)
- Admin controls

**[Game Module](game-module.md)** - Gameplay logic
- Weapons and combat
- Items and pickups
- Entity types (triggers, doors, etc.)
- Player physics
- Scoring and teams

**[AI / Bots](ai.md)** - Bot system
- Navigation and pathfinding
- Combat AI
- Goal-driven behavior
- Skill levels

### Client-Side

**[CGame Module](cgame-module.md)** - Client presentation
- HUD rendering
- Client-side prediction
- Entity interpolation
- Particle effects
- View effects

**[Renderer](renderer.md)** - OpenGL rendering
- BSP rendering pipeline
- Clustered forward lighting (voxel-based)
- Shadow mapping
- Material system
- Shader conventions

### Tools

**[Map Compiler (quemap)](quemap.md)** - BSP compilation
- BSP tree generation
- VIS calculation
- Lightmap baking
- Voxel grid generation
- Workflow and optimization

## Previous AI Session Documentation

Extensive documentation from prior Copilot optimization sessions (Nov-Dec 2024):

### Entry Points
- **[doc/copilot/README.md](../../doc/copilot/README.md)** - Overview of all session docs
- **[doc/copilot/SESSION_SUMMARY.md](../../doc/copilot/SESSION_SUMMARY.md)** - Comprehensive session summary
- **[doc/copilot/SESSION_CONTEXT.md](../../doc/copilot/SESSION_CONTEXT.md)** - Latest session context
- **[doc/copilot/INDEX.md](../../doc/copilot/INDEX.md)** - Index of all optimization/bugfix docs

### Major Topics

**Renderer Optimizations**:
- Shadow mapping: 55-85% faster
- Parallax occlusion: 45-50% faster
- Parallax self-shadow: 99% cost reduction
- Combined: ~2x frame rate improvement

**Key Documents**:
- [SHADOWMAP_OPTIMIZATION.md](../../doc/copilot/SHADOWMAP_OPTIMIZATION.md)
- [PARALLAX_FINAL_RESULTS.md](../../doc/copilot/PARALLAX_FINAL_RESULTS.md)
- [CLUSTERED_LIGHTING_IMPLEMENTATION.md](../../doc/copilot/CLUSTERED_LIGHTING_IMPLEMENTATION.md)

**Critical Bug Fixes**:
- Frustum culling (half FOV fix)
- Entity state management (spawn_id validation)
- Voxel boundary lighting artifacts

## Documentation by Task

### I want to...

**Build and test the engine**
→ [copilot-instructions.md](../copilot-instructions.md) - Build System section

**Understand the renderer**
→ [renderer.md](renderer.md) + [doc/copilot/SESSION_SUMMARY.md](../../doc/copilot/SESSION_SUMMARY.md)

**Add a new weapon**
→ [game-module.md](game-module.md) - Weapons section

**Fix network issues**
→ [network.md](network.md) - Protocol and debugging

**Improve bot AI**
→ [ai.md](ai.md) - Combat and pathfinding

**Create visual effects**
→ [cgame-module.md](cgame-module.md) - Particle system

**Compile a map**
→ [quemap.md](quemap.md) - Workflow section

**Optimize rendering**
→ [doc/copilot/](../../doc/copilot/) - Optimization docs

**Understand collision**
→ [collision.md](collision.md) - Traces and queries

**Add console commands**
→ [common.md](common.md) - Console section

## Documentation Standards

All documentation follows these principles:
- **Big picture first** - Architecture before details
- **Practical examples** - Real code patterns, not theory
- **Integration points** - How systems connect
- **Common issues** - What goes wrong and how to fix it
- **No obvious advice** - Assumes developer competence

## Contributing

When adding new documentation:
1. Follow existing format (markdown with clear sections)
2. Include practical code examples
3. Document common issues and debugging
4. Update this index
5. Link from main copilot-instructions.md if major subsystem

## Quick Reference

| System | Location | Primary Files |
|--------|----------|---------------|
| Collision | `src/collision/` | cm_bsp.c, cm_trace.c |
| Network | `src/net/` | net_chan.c, net_message.c |
| Server | `src/server/` | sv_main.c, sv_world.c |
| Game | `src/game/default/` | g_main.c, g_physics.c |
| CGame | `src/cgame/default/` | cg_main.c, cg_predict.c |
| Renderer | `src/client/renderer/` | r_main.c, r_bsp_draw.c |
| Common | `src/common/` | console.c, cvar.c, filesystem.c |
| AI | `src/game/default/g_ai*.c` | g_ai_main.c, g_ai_node.c |
| Quemap | `src/tools/quemap/` | main.c, light.c, voxel.c |

---

**For AI assistants**: Always read relevant documentation before making changes. The doc/copilot/ session logs contain critical context about renderer optimizations and past debugging efforts.
