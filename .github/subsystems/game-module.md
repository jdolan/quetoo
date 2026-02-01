# Game Module (game/default/)

The server-side game module implements all gameplay logic: weapons, items, scoring, teams, physics, and AI.

## Location

`src/game/default/` - Default game module (compiled as `game.so`/`game.dll`)

## Core Purpose

- Implement gameplay rules and mechanics
- Handle entity spawning and behavior (items, projectiles, triggers)
- Player physics (movement, swimming, ladder climbing)
- Weapons and combat system
- AI/bot system
- Scoring and team management

## Architecture

**Dynamically loaded module**: Server loads `game.so` at runtime via dlopen/LoadLibrary.

**API versioning**: Game module must match server's GAME_API_VERSION (currently 13).

**Isolation**: Game module cannot directly access server internals - only through `gi` (game import) API.

## Key Files

### g_main.c / g_main.h
Module initialization and main game loop:
- `G_LoadGame()` - Entry point, returns `game_export_t` API
- `G_Init()` - Initialize game (called by server)
- `G_Shutdown()` - Cleanup on server shutdown
- `G_Frame()` - Run one game tick (called 60 times/second)
- Game cvars registration

### g_entity.c / g_entity.h
Entity lifecycle management:
- `G_Spawn()` - Allocate new entity slot
- `G_FreeEntity()` - Return entity to free pool
- `G_InitEntity()` - Initialize entity from BSP entity definition
- Entity slot management (MAX_ENTITIES = 1024)

**Entity slot reuse**: Entities have `spawn_id` that increments on reuse to detect stale references.

### g_physics.c / g_physics.h
Physics simulation:
- `G_RunEntity()` - Run physics for one entity
- `G_Physics_Toss()` - Projectile/grenade physics
- `G_Physics_Pusher()` - Moving platforms (func_door, func_plat)
- `G_Physics_None()` - Static entities
- `G_ClipVelocity()` - Bounce/slide physics

**Physics types**:
- `PHYSICS_NONE` - No movement (static items, decorations)
- `PHYSICS_PUSH` - Pusher (doors, platforms, triggers)
- `PHYSICS_TOSS` - Ballistic (grenades, gibs, dropped items)

### g_combat.c / g_combat.h
Damage and death:
- `G_Damage()` - Apply damage to entity
- `G_Killed()` - Handle entity death
- `G_RadiusDamage()` - Explosion/splash damage
- Damage type handling (bullets, explosions, lava, falling)
- Kill attribution (who killed who)

### g_weapon.c / g_weapon.h
Weapon implementation:
- `G_FireWeapon()` - Weapon firing logic
- Weapon-specific functions: `G_FireBlaster()`, `G_FireShotgun()`, `G_FireRocket()`, etc.
- Weapon switching, reload, cooldown
- Ammo management

**Weapons**:
- Blaster (infinite ammo starter)
- Shotgun (spread shot)
- Super shotgun (more spread, more damage)
- Machinegun (rapid fire)
- Grenade launcher (bouncing projectiles)
- Rocket launcher (direct rockets)
- Hyperblaster (rapid plasma)
- Lightning (beam weapon)
- Railgun (instant hitscan, wall penetration)
- BFG (huge splash damage)

### g_ballistics.c / g_ballistics.h
Projectile handling:
- `G_BulletProjectile()` - Hitscan weapons (machinegun, shotgun)
- `G_Projectile()` - Ballistic projectiles (rockets, grenades)
- Hit detection, damage application
- Impact effects (sparks, blood, etc.)

### g_item.c / g_item.h
Item system (health, armor, ammo, weapons):
- `G_ItemByName()` - Look up item definition
- `G_AddAmmo()`, `G_AddArmor()`, `G_AddHealth()` - Give items to player
- `G_TouchItem()` - Player picks up item
- Item respawn logic

**Item types**:
- Health (small, medium, mega)
- Armor (shards, jacket, body, combat)
- Weapons (all weapon pickups)
- Ammo (shells, bullets, grenades, rockets, etc.)
- Power-ups (quad damage, adrenaline)

### g_client.c / g_client.h
Player/client management:
- `G_ClientConnect()` - Player joins game
- `G_ClientDisconnect()` - Player leaves
- `G_ClientBegin()` - Player spawns
- `G_ClientThink()` - Process player input (movement, attacks)
- `G_ClientUserInfoChanged()` - Name, skin, team changes

### g_client_view.c / g_client_view.h
Player view and effects:
- `G_ClientEndFrame()` - Update HUD, effects
- View angles, bobbing, kick
- Blend screen effects (damage flash, quad color, etc.)

### g_client_stats.c / g_client_stats.h
Player HUD stats:
- Health, armor display
- Ammo counters
- Score/frags
- Team status
- Stats are sent to client for HUD rendering

### g_client_chase.c / g_client_chase.h
Spectator/chase cam:
- Following other players when dead/spectating
- Chase cam mode toggling

### bg_pmove.c / bg_pmove.h
**Player movement** (shared with client for prediction):
- `Pmove()` - Player physics simulation
- Walking, running, jumping, swimming, ladder climbing
- Collision detection and sliding
- Water physics, falling damage

**This file is compiled into BOTH game and cgame modules** for client-side prediction.

### g_ai*.c / g_ai*.h
Bot AI system (see AI subsystem doc for details):
- Pathfinding, navigation
- Goal selection (attack, defend, pickup items)
- Combat AI (aiming, dodging, weapon selection)

### g_entity_*.c / g_entity_*.h
Entity type implementations:

**g_entity_func.c** - Functional entities:
- `G_func_door()` - Doors (sliding, rotating)
- `G_func_plat()` - Elevators/platforms
- `G_func_train()` - Moving trains
- `G_func_button()` - Buttons/switches
- `G_func_wall()` - Breakable walls

**g_entity_trigger.c** - Trigger entities:
- `G_trigger_multiple()` - Generic trigger
- `G_trigger_push()` - Jump pads
- `G_trigger_hurt()` - Damage zones (lava, slime)
- `G_trigger_teleport()` - Teleporters

**g_entity_target.c** - Target entities (used by triggers):
- `G_target_print()` - Print message
- `G_target_explosion()` - Create explosion
- `G_target_speaker()` - Ambient sound

**g_entity_misc.c** - Miscellaneous entities:
- `G_misc_model()` - Static 3D models
- `G_misc_sound()` - Positioned sounds
- `G_misc_emit()` - Particle emitters

**g_entity_info.c** - Info entities:
- `G_info_player_start()` - Spawn points
- `G_info_player_deathmatch()` - Deathmatch spawns
- `G_info_player_team1/team2()` - Team spawns

### g_cmd.c / g_cmd.h
Player console commands:
- `G_Say_f()` - Chat messages
- `G_Kill_f()` - Suicide
- `G_Team_f()` - Join team
- `G_Spectate_f()` - Become spectator
- `G_Score_f()` - Show scoreboard
- `G_Use_f()` - Use/activate

### g_util.c / g_util.h
Utility functions:
- `G_Find()` - Find entity by classname/targetname
- `G_UseTargets()` - Activate trigger targets
- `G_SetMoveDir()` - Calculate movement direction from angles
- Vector/string utilities

### g_sound.c / g_sound.h
Game sound system:
- `G_Sound()` - Play positioned sound
- Sound index management
- Attenuation types (NONE, IDLE, STATIC, NORM)

### g_map_list.c / g_map_list.h
Map rotation/voting:
- Parse .maps files (map rotation lists)
- Map voting system
- Automatic map rotation

## Game Module API

### Imports from Server (`gi`)

Game calls these server functions:

```c
typedef struct {
    // Console output
    void (*Print)(const char *fmt, ...);
    void (*Debug)(debug_t debug, const char *func, const char *fmt, ...);
    void (*Warn)(const char *func, const char *fmt, ...);
    void (*Error)(const char *func, const char *fmt, ...) __attribute__((noreturn));
    
    // Memory
    void *(*Malloc)(size_t size, mem_tag_t tag);
    void (*Free)(void *p);
    
    // Filesystem
    int64_t (*LoadFile)(const char *filename, void **buffer);
    
    // Console variables
    cvar_t *(*Cvar)(const char *name, const char *value, uint32_t flags, const char *desc);
    
    // Commands
    void (*AddCmd)(const char *name, Cmd_ExecuteFunc_t func, const char *desc);
    
    // Collision
    cm_trace_t (*Trace)(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, 
                        const g_entity_t *skip, int32_t contents);
    int32_t (*PointContents)(vec3_t point);
    
    // Entity management
    void (*LinkEntity)(g_entity_t *ent);
    void (*UnlinkEntity)(g_entity_t *ent);
    int32_t (*BoxEntities)(box3_t bounds, g_entity_t **list, int32_t len, uint32_t type);
    
    // Multicast
    void (*Unicast)(const g_entity_t *ent, bool reliable);
    void (*Multicast)(vec3_t origin, multicast_t to);
    
    // Positioned events
    void (*PositionedSound)(vec3_t origin, g_entity_t *ent, uint16_t index, atten_t atten);
    
    // Message writing (for multicast/unicast)
    void (*WriteByte)(int32_t c);
    void (*WriteShort)(int32_t c);
    void (*WriteLong)(int32_t c);
    void (*WriteString)(const char *s);
    void (*WritePosition)(vec3_t pos);
    void (*WriteAngle)(float f);
    void (*WriteAngles)(vec3_t angles);
    
    // Config strings (level name, sky, etc.)
    void (*SetConfigString)(uint16_t index, const char *string);
} g_import_t;
```

### Exports to Server (`ge`)

Server calls these game functions:

```c
typedef struct {
    uint16_t api_version;
    
    // Lifecycle
    void (*Init)(void);
    void (*Shutdown)(void);
    
    // Entity spawning
    void (*SpawnEntity)(g_entity_t *ent);
    
    // Main game loop
    void (*Frame)(void);
    
    // Client events
    bool (*ClientConnect)(g_entity_t *ent, char *userinfo);
    void (*ClientBegin)(g_entity_t *ent);
    void (*ClientUserInfoChanged)(g_entity_t *ent, const char *userinfo);
    void (*ClientDisconnect)(g_entity_t *ent);
    void (*ClientCommand)(g_entity_t *ent);
    void (*ClientThink)(g_entity_t *ent, user_cmd_t *cmd);
} game_export_t;
```

## Entity Structure

Game extends base `g_entity_t` with additional fields:

```c
struct g_entity_s {
    // Base (shared with server)
    const cm_entity_t *def;        // BSP entity definition
    const char *classname;
    entity_state_t s;              // Network state
    
    // Game-specific
    void (*Think)(g_entity_t *self);              // Think function
    void (*Touch)(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace);
    void (*Use)(g_entity_t *self, g_entity_t *other, g_entity_t *activator);
    
    vec3_t velocity;               // Current velocity
    vec3_t avelocity;             // Angular velocity
    int32_t mass;                 // For physics
    
    float next_think;             // Time of next Think() call
    
    g_entity_t *ground_entity;    // What we're standing on
    g_entity_t *owner;            // Who created this (projectiles)
    g_entity_t *enemy;            // Current target (AI)
    
    int32_t health;               // Hit points
    int32_t max_health;
    int32_t damage;               // Damage dealt by this entity
    
    const char *target;           // Targetname to activate
    const char *targetname;       // Name for targeting
    const char *killtarget;       // Target to kill on use
    
    uint32_t timestamp;           // For timing (item respawn, etc.)
    
    g_client_t *client;           // If this is a player
    
    // ... many more fields
};
```

## Game Loop

**Every 16.67ms (60 Hz)**:

1. `G_Frame()` called by server
2. For each active entity:
   - Process think function if `next_think` time reached
   - Run physics based on `physics` type
   - Check triggers/touches
3. Run AI for bots
4. Update scores, stats
5. Check for end-of-level conditions (frag/time limit)

## Spawn Flow

**Map loads**:
1. Server parses BSP entities
2. For each entity definition, server calls `ge->SpawnEntity()`
3. Game looks up spawn function by classname:
   - `"weapon_rocketlauncher"` → `G_weapon_rocketlauncher()`
   - `"func_door"` → `G_func_door()`
   - etc.
4. Spawn function sets up entity fields
5. Entity linked into world with `gi.LinkEntity()`

**Player spawns**:
1. Client joins: `G_ClientConnect()` → `G_ClientBegin()`
2. Find spawn point: `G_SelectSpawnPoint()`
3. Initialize player: give weapons, set origin
4. Link into world

## Physics System

### Movement Types

**PHYSICS_NONE**: Static entities (items, decorations)
- No movement
- Only runs Think() function

**PHYSICS_PUSH**: Movers (doors, platforms)
- Moves along path or to target position
- Pushes players/objects
- Blocks movement when obstructed

**PHYSICS_TOSS**: Ballistic projectiles
- Gravity applied
- Bounces off surfaces
- Detonates on impact or timeout

### Player Movement

Handled by `Pmove()` (bg_pmove.c):
```c
// Process player input
user_cmd_t cmd;  // From client
pmove_t pm = {
    .s = client->ps,              // Player state (position, velocity, etc.)
    .cmd = cmd,                   // Input
    .ground_entity = ground,
    // ...
};

Pmove(&pm);  // Updates ps with new position/velocity

// Check for landing damage, water sounds, etc.
```

**Movement features**:
- Walking/running (max speed ~310 units/sec)
- Crouching (max speed ~165 units/sec)
- Jumping (velocity boost)
- Swimming (different physics in water)
- Ladder climbing (special movement mode)
- Air control (limited strafe while airborne)

## Damage System

```c
void G_Damage(g_entity_t *target, g_entity_t *inflictor, g_entity_t *attacker,
              vec3_t dir, vec3_t point, vec3_t normal, int32_t damage, 
              int32_t knockback, int32_t dflags, uint32_t mod);
```

- **target**: Who gets damaged
- **inflictor**: What caused damage (rocket projectile)
- **attacker**: Who fired weapon (player)
- **damage**: Base damage amount
- **knockback**: Velocity applied
- **mod**: Means of death (MOD_ROCKET, MOD_SHOTGUN, etc.)

**Damage reduction**:
1. Check god mode / invulnerability
2. Apply armor absorption (armor takes 50-80% of damage)
3. Subtract from health
4. If health ≤ 0, call `G_Killed()`

## Combat Mechanics

### Hitscan Weapons
Instant hit, no projectile:
```c
const cm_trace_t tr = gi.Trace(start, end, NULL, NULL, self, CONTENTS_MASK_CLIP_PROJECTILE);
if (tr.fraction < 1.0) {
    G_Damage(tr.ent, self, self, forward, tr.end, tr.plane.normal, 
             damage, knockback, 0, MOD_MACHINEGUN);
}
```

### Projectile Weapons
Spawns entity that flies:
```c
g_entity_t *rocket = G_Spawn();
rocket->owner = self;
rocket->solid = SOLID_PROJECTILE;
rocket->physics = PHYSICS_TOSS;
rocket->touch = G_RocketTouch;  // Explode on impact
Vec3_Copy(start, rocket->s.origin);
Vec3_Scale(forward, speed, rocket->velocity);
gi.LinkEntity(rocket);
```

### Splash Damage
Area-of-effect:
```c
void G_RadiusDamage(g_entity_t *inflictor, g_entity_t *attacker, 
                    int32_t damage, g_entity_t *ignore, float radius) {
    // Find all entities within radius
    g_entity_t *ents[MAX_ENTITIES];
    const box3_t bounds = Box3_Expand(Box3_FromRadius(inflictor->s.origin, radius), radius);
    const int32_t num = gi.BoxEntities(bounds, ents, lengthof(ents), BOX_ALL);
    
    for (int32_t i = 0; i < num; i++) {
        // Calculate distance falloff
        const float dist = Vec3_Distance(ents[i]->s.origin, inflictor->s.origin);
        const float scale = 1.0 - (dist / radius);
        const int32_t dmg = damage * scale;
        
        // Apply damage
        G_Damage(ents[i], inflictor, attacker, ...);
    }
}
```

## Item System

Items defined in `g_items[]` array:
```c
g_item_t g_items[] = {
    {
        .classname = "item_health",
        .pickup = G_PickupHealth,
        .quantity = 10,
        .model = "models/items/health/medium/tris.md3",
        .sound = "items/s_health.wav",
        // ...
    },
    // ... all items
};
```

**Pickup flow**:
1. Player touches item (collision detection)
2. `G_TouchItem()` called
3. Check if player can pick up (has space for ammo, etc.)
4. Call item's `pickup` function
5. Play sound, show effect
6. Remove item (or start respawn timer)

**Respawn**:
- Items respawn after 5-30 seconds (depends on item type)
- Mega health: 30 sec
- Weapons: 5 sec
- Ammo: 10 sec

## Console Variables

```
g_gameplay 0          # Game mode (0=DM, 1=instagib, 2=arena, etc.)
g_teams 0             # Teams enabled
g_friendly_fire 1     # Team damage enabled
g_frag_limit 30       # Score to win
g_time_limit 20       # Minutes per map
g_spawn_farthest 1    # Spawn far from enemies
g_cheats 0            # Enable cheat commands
g_weapon_stay 0       # Weapons don't disappear after pickup
g_quad_damage_time 10 # Quad powerup duration (seconds)
```

## Debugging

```
g_developer 1         # Debug output
g_show_traces 1       # Visualize collision traces
```

## Common Patterns

### Delayed Actions
```c
void DelayedFunction(g_entity_t *self) {
    // Do something later
    gi.PositionedSound(self->s.origin, NULL, sound_index, ATTEN_NORM);
    G_FreeEntity(self);
}

// Set up delayed call
g_entity_t *timer = G_Spawn();
timer->think = DelayedFunction;
timer->next_think = g_level.time + 3.0;  // 3 seconds later
```

### Finding Entities
```c
g_entity_t *ent = NULL;
while ((ent = G_Find(ent, EOFS(targetname), "door1"))) {
    // Found entity with targetname "door1"
    ent->use(ent, NULL, activator);
}
```

### Trigger Activation
```c
void G_UseTargets(g_entity_t *ent, g_entity_t *activator) {
    if (ent->target) {
        g_entity_t *t = NULL;
        while ((t = G_Find(t, EOFS(targetname), ent->target))) {
            if (t->use) {
                t->use(t, ent, activator);
            }
        }
    }
}
```

## Common Issues

### Entity Overflow
- MAX_ENTITIES = 1024, includes players, projectiles, items, etc.
- Grenade spam can exhaust slots
- Solution: Limit projectiles per player or increase MAX_ENTITIES

### Physics Glitches
- Players stuck in geometry: Check bounding boxes
- Players falling through floor: Ensure origin is above ground
- Jerky movement: Check for conflicting velocity modifications

### Weapon Balance
- Damage values in g_weapon.c
- Test in practice with multiple players
- Consider range, rate of fire, ammo availability
