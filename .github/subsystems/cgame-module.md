# Client Game Module (cgame/default/)

The client-side game module implements client-only features: HUD, view effects, entity interpolation, prediction, particles, and client-side entity state.

## Location

`src/cgame/default/` - Default cgame module (compiled as `cgame.so`/`cgame.dll`)

## Core Purpose

- Render HUD (health, armor, ammo, scores)
- Client-side prediction (smooth movement despite latency)
- Entity interpolation (smooth animation between server frames)
- View effects (damage flash, underwater tint, weapon bob)
- Particle effects (blood, explosions, sparks)
- Client-side sound (footsteps, weapon sounds)
- Temporary entities (muzzle flash, projectile trails)

## Architecture

**Dynamically loaded**: Client loads `cgame.so` at runtime, similar to game module.

**API versioning**: Must match client's CGAME_API_VERSION (currently 23).

**Isolation**: Cannot access client internals except through `cgi` (cgame import) API.

## Key Files

### cg_main.c / cg_main.h
Module initialization and main loop:
- `Cg_LoadCgame()` - Entry point, returns `cg_export_t`
- `Cg_Init()` - Initialize cgame
- `Cg_Shutdown()` - Cleanup
- `Cg_UpdateMedia()` - Load/reload assets (models, sounds)
- `Cg_Interpolate()` - Interpolate entity positions between frames
- `Cg_Frame()` - Main cgame frame

### cg_entity.c / cg_entity.h
Entity state management:
- `Cg_AddEntity()` - Add entity to render view
- `Cg_AddEntities()` - Process all entities for this frame
- Entity interpolation (lerp between old and new state)
- Bounding box visualization (`cg_draw_bbox`)

**Entity slot consistency**: Validates `spawn_id` to detect entity reuse (see `doc/copilot/ENTITY_STATE_BUG_FIX.md`).

### cg_predict.c / cg_predict.h
**Client-side prediction** (most important for feel):
- `Cg_PredictMovement()` - Simulate local player movement
- Uses same `Pmove()` as server (bg_pmove.c)
- Predicts 50-100ms ahead to hide latency
- Server corrections reconcile differences

**How it works**:
1. Client sends movement input to server
2. Client immediately simulates movement locally (prediction)
3. Server eventually sends authoritative position
4. If mismatch, client smoothly corrects (error is usually tiny)

### cg_view.c / cg_view.h
View/camera management:
- `Cg_UpdateView()` - Calculate camera position, angles, FOV
- Weapon bobbing (viewmodel sway while moving)
- View kicks (recoil, damage impact)
- Roll angles (leaning on slopes)
- Underwater distortion

### cg_hud.c / cg_hud.h
Heads-up display:
- `Cg_DrawHud()` - Main HUD rendering
- Health/armor bars
- Ammo counters
- Crosshair
- Pickup notifications ("You got the rocket launcher")
- Death messages ("Player1 fragged Player2")

### cg_score.c / cg_score.h
Scoreboard:
- `Cg_DrawScores()` - Show player scores, pings, teams
- Updated from player stats sent by server

### cg_input.c / cg_input.h
Input handling:
- `Cg_Move()` - Convert raw input to user_cmd_t
- Mouse/keyboard → movement vectors and angles
- Send commands to server via `cgi.SendCmd()`

### cg_media.c / cg_media.h
Asset management:
- `Cg_LoadMedia()` - Load models, sounds, images
- Pre-cache common assets (weapon models, particles)
- Asset references stored in `cg_media_t` struct

### cg_client.c / cg_client.h
Per-client state:
- `Cg_UpdateClient()` - Update client info (name, skin, team)
- Parse userinfo strings
- Load client-specific models/skins

### cg_effect.c / cg_effect.h
Particle effects system:
- `Cg_AddEffect()` - Spawn particle effect
- Blood spurts, explosions, smoke, sparks
- Particle physics (gravity, drag, bounce)
- Particle rendering (sprites, trails)

### cg_entity_effect.c / cg_entity_effect.h
Entity-based effects:
- Smoke trails from rockets
- Beam effects (lightning gun)
- Powerup glows (quad damage aura)
- Bubbles in water

### cg_entity_event.c / cg_entity_event.h
**Entity events** (critical for feedback):
- `Cg_EntityEvent()` - Process entity event from server
- Footsteps, weapon fire, item pickup sounds
- Impact effects (bullet holes, scorch marks)
- Gibs (body parts on death)

**Event types**:
- `EV_FOOTSTEP` - Play footstep sound
- `EV_ITEM_PICKUP` - Flash icon, play sound
- `EV_TELEPORT` - Teleport effect
- `EV_CLIENT_DAMAGE` - Blood, pain sound
- `EV_CLIENT_DEATH` - Death animation, gib

### cg_entity_trail.c / cg_entity_trail.h
Projectile trails:
- Rocket smoke trails
- Grenade arc trails
- Hyperblaster plasma trails
- Trail segments, fading

### cg_temp_entity.c / cg_temp_entity.h
Temporary entities (short-lived effects):
- `Cg_ParseTempEntity()` - Server sends temp entity
- Explosions, bullet impacts, rail slugs
- One-shot effects that don't need entity slots

### cg_light.c / cg_light.h
Dynamic lights:
- `Cg_AddLight()` - Add dynamic light to scene
- Muzzle flashes, explosions, power-ups
- Light intensity/color/radius

### cg_sprite.c / cg_sprite.h
Sprite effects:
- 2D/3D sprites (lens flares, particles)
- Used for particles, explosions, beams

### cg_flare.c / cg_flare.h
Lens flares:
- Sun/light source flares
- Occlusion testing (hide flare behind walls)

### cg_muzzle_flash.c / cg_muzzle_flash.h
Weapon muzzle flashes:
- Different flash per weapon
- Position flash at weapon barrel
- Dynamic light for flash

### cg_sound.c / cg_sound.h
Client-side sound:
- `Cg_AddSound()` - Play positioned sound
- Ambient sounds (wind, water)
- Looping sounds (power-ups, items)

### cg_weapon.c / cg_weapon.h
Weapon viewmodel rendering:
- First-person weapon model (viewmodel)
- Weapon animations (idle, fire, reload)
- Weapon switching animation

### cg_ui.c / cg_ui.h
UI integration:
- Menu system integration
- UI state management
- HUD customization

### cg_discord.c / cg_discord.h
Discord Rich Presence integration:
- Show current server, map, score in Discord
- Updates every 15 seconds

## CGame Module API

### Imports from Client (`cgi`)

```c
typedef struct {
    cl_client_t *client;          // Client state
    const cl_state_t *state;      // Connection state
    const r_context_t *context;   // Renderer context
    r_view_t *view;               // View definition (camera, FOV, etc.)
    s_stage_t *stage;             // Audio stage
    
    // Console
    void (*Print)(const char *fmt, ...);
    void (*Debug)(debug_t debug, const char *func, const char *fmt, ...);
    void (*Warn)(const char *func, const char *fmt, ...);
    void (*Error)(const char *func, const char *fmt, ...) __attribute__((noreturn));
    
    // Memory, cvars, commands (similar to game module)
    // ...
    
    // Rendering
    void (*AddEntity)(const r_entity_t *ent);
    void (*AddLight)(const r_light_t *light);
    void (*AddSprite)(const r_sprite_t *sprite);
    
    // Media
    const r_model_t *(*LoadModel)(const char *name);
    const r_image_t *(*LoadImage)(const char *name, uint32_t type);
    const s_sample_t *(*LoadSample)(const char *name);
    
    // Sound
    void (*AddSample)(const s_stage_t *stage, const s_sample_t *sample, ...);
    
    // Prediction (uses shared Pmove)
    void (*Pmove)(pmove_t *pm);
} cg_import_t;
```

### Exports to Client (`cge`)

```c
typedef struct {
    uint16_t api_version;
    
    void (*Init)(void);
    void (*Shutdown)(void);
    void (*UpdateMedia)(const char *mapname);
    void (*LoadClient)(cl_client_info_t *ci);
    void (*Interpolate)(const cl_frame_t *frame);
    void (*UsePrediction)(void);
    void (*PredictMovement)(const vec3_t angles, const user_cmd_t *cmd);
    void (*UpdateView)(const cl_frame_t *frame);
    void (*ParseMessage)(int32_t cmd);
    void (*Frame)(void);
} cg_export_t;
```

## Frame Flow

**Every client frame (60-300 Hz depending on `cl_max_fps`)**:

1. Client receives frame from server (entity snapshots at 60 Hz)
2. `Cg_Interpolate()` - Lerp entities between last 2 server frames
3. `Cg_PredictMovement()` - Predict local player ahead of server
4. `Cg_UpdateView()` - Calculate camera position/angles
5. `Cg_AddEntities()` - Add all entities to render view
6. `Cg_AddEffects()` - Add particles, lights, sprites
7. `Cg_Frame()` - Update animations, sounds
8. Render happens after cgame frame completes

## Interpolation

Server sends updates at 60 Hz, client renders at 60-300 Hz. Interpolation smooths motion:

```c
// Lerp between two server frames
const float lerp = (cl.time - cl.frame.time) / (cl.frame.time - cl.oldframe.time);
Vec3_Mix(oldpos, newpos, lerp, interpolated_pos);
```

Result: Smooth 144 FPS rendering from 60 Hz server updates.

## Prediction

Without prediction, movement feels laggy (50-100ms delay to server). Prediction hides this:

1. Client sends input to server
2. Client immediately predicts result using same physics as server
3. Render predicted position (feels instant)
4. When server update arrives, client checks for mismatch
5. Small mismatch: smoothly correct
6. Large mismatch: snap to server (rare, indicates prediction error or lag)

**Prediction enables instant-feeling movement despite network latency.**

## Entity Events

Server sends events as part of entity state:
```c
entity_state_t.event = EV_FOOTSTEP;
```

Client detects event and plays effect:
```c
if (s->event == EV_FOOTSTEP) {
    cgi.AddSample(stage, footstep_sound, ent->origin, ATTEN_NORM, 0);
}
```

**Events are single-shot** - cleared after processing.

## Temporary Entities

Short-lived effects sent immediately (not part of entity snapshot):

```c
// Server sends
Net_WriteByte(&sv.multicast, SV_CMD_TEMP_ENTITY);
Net_WriteByte(&sv.multicast, TE_EXPLOSION);
Net_WritePosition(&sv.multicast, origin);

// Client receives
void Cg_ParseTempEntity(void) {
    const int32_t type = cgi.ReadByte();
    switch (type) {
        case TE_EXPLOSION:
            const vec3_t pos = cgi.ReadPosition();
            Cg_ExplosionEffect(pos);
            Cg_AddLight(pos, 150.0, RGB(1.0, 0.5, 0.0));
            break;
    }
}
```

## HUD System

HUD elements positioned in screen coordinates:

```c
// Draw health
const r_pixel_t x = cgi.context->width * 0.1;  // 10% from left
const r_pixel_t y = cgi.context->height * 0.9; // 90% from top
cgi.DrawString(x, y, va("%d", cl_client->health), CON_COLOR_DEFAULT);

// Draw ammo icon
const r_image_t *icon = cgi.LoadImage("pics/i_shells.tga", IMG_PIC);
cgi.DrawImage(x, y, 1.0, icon, color_white);
```

## View Effects

### Damage Flash
When damaged, screen flashes red:
```c
if (cl_client->damage_blend > 0.0) {
    cgi.view->blend = RGB_A(1.0, 0.0, 0.0, cl_client->damage_blend);
}
```

### Underwater Tint
In water, screen has blue tint:
```c
if (cgi.client->underwater) {
    cgi.view->blend = RGB_A(0.2, 0.4, 0.8, 0.4);
}
```

### Weapon Bob
Viewmodel sways when moving:
```c
float bob = speed * 0.04 * sinf(cl.time * 12.0);
cgi.view->angles[ROLL] += bob;
```

## Particle System

Particles are simple sprites with physics:

```c
typedef struct {
    vec3_t origin;
    vec3_t velocity;
    vec3_t acceleration;  // Usually gravity
    color32_t color;
    float alpha;
    float size;
    uint32_t lifetime;
    const r_image_t *image;
} cg_particle_t;
```

**Particle types**:
- Blood (red, falls down)
- Sparks (orange, gravity, bounces)
- Smoke (gray, rises slowly)
- Explosions (orange/red, expands outward)
- Bubbles (rises in water)

## Console Variables

```
cg_draw_crosshair 1      # Show crosshair
cg_draw_fps 0            # Show FPS counter
cg_draw_speed 0          # Show movement speed
cg_fov 110               # Field of view
cg_bob 1.0               # View bobbing amount
cg_predict 1             # Client prediction
cg_max_particles 4096    # Max particle count
cg_add_particles 1       # Enable particles
cg_add_entities 1        # Render entities
cg_add_lights 1          # Dynamic lights
```

## Performance

### Particle Limits
- Too many particles hurt performance
- Default limit: 4096
- Explosions spawn 50-100 particles each
- Grenade spam can exceed limit → particles culled

### Entity Rendering
- Server sends ~50-200 entities per frame (depends on PVS)
- Client interpolates all visible entities
- Culling happens in renderer (frustum culling)

## Common Patterns

### Adding Dynamic Light
```c
r_light_t light = {
    .origin = { x, y, z },
    .radius = 150.0,
    .color = RGB(1.0, 0.5, 0.0),  // Orange
};
cgi.AddLight(&light);
```

### Spawning Particles
```c
for (int i = 0; i < 20; i++) {
    cg_particle_t *p = Cg_AddParticle();
    Vec3_Copy(origin, p->origin);
    p->velocity[0] = Randomf() * 100.0 - 50.0;
    p->velocity[1] = Randomf() * 100.0 - 50.0;
    p->velocity[2] = Randomf() * 100.0;
    p->acceleration[2] = -800.0;  // Gravity
    p->color = color_red;
    p->size = 2.0;
    p->lifetime = 1000;  // 1 second
}
```

### Playing Positioned Sound
```c
cgi.AddSample(cgi.stage, rocket_fire_sound, 
              muzzle_origin, ATTEN_NORM, 0);
```

## Client-Side Entity Types

### Models (Third-Person)
- Player models (other players)
- Item models (health, weapons on ground)
- Projectiles (rockets, grenades)

### View Models (First-Person)
- Weapon in hand (viewmodel)
- Different model than world weapon
- Animated (idle, fire, reload)

### Sprites
- Muzzle flashes
- Explosions
- Lens flares
- Particles

## Debugging

```
cg_draw_bbox 1        # Show entity bounding boxes
cg_draw_velocity 1    # Show velocity vectors
cg_third_person 1     # Third-person camera
developer 1           # Debug output
```

## Common Issues

### Jerky Movement
- Prediction disabled or broken
- Check `cg_predict 1`
- Server framerate too low (`sv_hz < 60`)

### Missing Effects
- Effects not spawning: check event parsing
- Particles not showing: check `cg_add_particles 1`
- Sounds not playing: check sample loading

### HUD Layout
- Different resolutions need different scaling
- Use percentages, not absolute positions
- Test at 1920x1080, 2560x1440, 3840x2160

### Prediction Errors
- Client-server physics mismatch
- Usually caused by different Pmove code
- bg_pmove.c must be identical in game and cgame

## Integration Points

### Client (src/client/)
- `CL_Frame()` calls `cge->Frame()`
- `CL_ParseFrame()` calls `cge->Interpolate()`
- Client handles network, cgame handles presentation

### Renderer (src/client/renderer/)
- Cgame adds entities/lights to view
- Renderer culls and renders them
- Cgame doesn't directly touch OpenGL

### Sound (src/client/sound/)
- Cgame adds samples to stage
- Sound system mixes and plays them
- Cgame doesn't directly touch OpenAL

## Key Differences from Game Module

| Feature | Game (Server) | CGame (Client) |
|---------|---------------|----------------|
| Entities | Authoritative, simulates physics | Visual only, interpolates |
| Physics | Full simulation | Prediction only (local player) |
| Damage | Calculates damage, health | Visual feedback only |
| Items | Pickup logic | Visual effects |
| Network | Sends state | Receives state |
| Purpose | Gameplay logic | Presentation |

**Rule**: Game module is authoritative, cgame is purely for presentation and feel.
