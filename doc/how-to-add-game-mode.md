# How to add a game mode or modifier

This guide explains how to extend Quetoo's default game module with a new
primary mode or a reusable modifier. It assumes that you can already compile
Quetoo and that you are comfortable with basic C functions, structures, and
pointers.

The examples use the current mode runtime in `src/game/default`. They are
intended to be copied and adapted.

## The short version

A level is built from:

- exactly one **primary mode**, such as Deathmatch or CTF;
- zero or more ordered **modifiers**, such as Instagib, Arena, or Techs; and
- common components, such as teams, spawn selection, and match limits.

A mode or modifier normally consists of:

1. one `g_mode_<name>.c` file;
2. an immutable `g_mode_def_t` descriptor;
3. a `g_mode_ops_t` table containing only the hooks it needs;
4. an entry in the registry in `g_mode.c`;
5. an entry in `Makefile.am`; and
6. selection code that adds it to the level composition.

Registration makes a module known to the game. **Registration does not make it
active.** Worldspawn must select a primary mode or append a modifier before its
hooks will run.

## Choose the right kind of extension

Before writing code, decide what you are adding.

### Primary mode

Use a primary mode when it defines the main rules and win condition of the
level.

Examples:

- Deathmatch;
- Capture the Flag;
- a race mode; or
- a round-based objective mode.

There can only be one active primary mode.

### Modifier

Use a modifier when the behavior can sensibly be layered onto more than one
primary mode.

Examples:

- Instagib inventories;
- double damage;
- low gravity;
- Techs;
- a respawn queue; or
- a weapon timing variant.

Several modifiers can be active at once. Their order matters because the
primary mode runs first and modifiers run in composition order.

### Common component

Use a common component when the feature is reusable infrastructure rather
than a set of game rules.

Examples:

- team membership;
- spawn-point selection;
- match limits; or
- generic round and respawn-queue machinery.

A mode can call a common component or change its policy through a hook. Avoid
copying a reusable subsystem into every mode.

## What can a mode or modifier do?

Primary modes and modifiers receive the **same operation table**. The runtime
does not prohibit a modifier from spawning an entity, and it does not prohibit
a primary mode from changing weapon timing. The difference is how they are
selected and composed:

| Operation | Primary mode | Modifier | Important behavior |
|---|---:|---:|---|
| Own level, entity, and client data | Yes | Yes | Storage is allocated per active instance. |
| Declare cvars | Yes | Yes | All compiled descriptors register their cvars. |
| Spawn or observe entities | Yes | Yes | Use entity classes, lifecycle hooks, or `G_AllocEntity`. |
| Add items and entity types | Yes | Yes | Registrations only exist while the owner is active. |
| Run timers and per-frame logic | Yes | Yes | Use private state with `Frame`. |
| Change inventory | Yes | Yes | First `ClientInventory` handler to return true wins. |
| Influence bots | Yes | Yes | Bot transformers compose in order. |
| Change or cancel damage | Yes | Yes | Primary runs first, then modifiers. |
| Change weapon timing | Yes | Yes | Each result is passed to the next module. |
| Select spawns or own respawning | Yes | Yes | First handler that succeeds wins. |
| Assign teams | Yes | Yes | Usually belongs to a primary mode or team component. |
| End the match | Yes | Yes | `CheckRules` may be implemented by either kind. |
| Be the level's base ruleset | Yes | No | Exactly one primary is active. |
| Layer onto another ruleset | No | Yes | Up to `G_MODE_MAX_MODIFIERS` are active. |

Although both kinds *can* perform every operation, put behavior where it makes
architectural sense. A “touch the goal to win” rule belongs to a primary mode.
A periodic announcement or reusable damage rule belongs to a modifier.

## Tutorial 1: a small “Hello World” timer modifier

This first extension is intentionally small. When enabled, it prints
“Hello World” every five seconds. It demonstrates:

- a mode-owned cvar;
- private per-level state;
- `LevelBegin` and `Frame`;
- registration; and
- explicit modifier composition.

Create `src/game/default/g_mode_hello.c`:

```c
#include "g_local.h"

typedef struct {
  uint32_t next_message;
} g_hello_state_t;

static const g_mode_cvar_def_t g_hello_cvars[] = {
  {
    .name = "g_hello",
    .default_value = "0",
    .flags = CVAR_SERVER_INFO,
    .description = "Enables the Hello World modifier.",
  },
  {
    .name = "g_hello_interval",
    .default_value = "5",
    .flags = 0,
    .description = "Seconds between Hello World messages.",
  },
};

static uint32_t G_HelloInterval(void) {
  const cvar_t *interval = gi.GetCvar("g_hello_interval");
  const float seconds = interval ? Maxf(interval->value, 1.f) : 5.f;
  return (uint32_t) (seconds * 1000.f);
}

static void G_HelloLevelBegin(g_mode_t *mode, const char *map_name,
                              const cm_entity_t *props) {
  (void) map_name;
  (void) props;

  g_hello_state_t *state = G_ModeState(mode);
  const g_mode_context_t *context = G_ModeContext(mode);
  if (!state || !context || !context->level) {
    return;
  }

  state->next_message = context->level->time + G_HelloInterval();
}

static void G_HelloFrame(g_mode_t *mode) {
  g_hello_state_t *state = G_ModeState(mode);
  const g_mode_context_t *context = G_ModeContext(mode);
  if (!state || !context || !context->level ||
      context->level->time < state->next_message) {
    return;
  }

  gi.BroadcastPrint(PRINT_HIGH, "Hello World\n");
  state->next_message = context->level->time + G_HelloInterval();
}

static const g_mode_ops_t g_hello_ops = {
  .LevelBegin = G_HelloLevelBegin,
  .Frame = G_HelloFrame,
};

static const g_mode_def_t g_hello_mode = {
  .name = "hello",
  .kind = G_MODE_MODIFIER,
  .gameplay_selector = false,
  .state_size = sizeof(g_hello_state_t),
  .cvars = g_hello_cvars,
  .num_cvars = lengthof(g_hello_cvars),
  .ops = &g_hello_ops,
};

const g_mode_def_t *G_HelloModeDefinition(void) {
  return &g_hello_mode;
}
```

The arrays and descriptor are `static const`. That is allowed: they are
immutable definitions, not mutable game state. Do not store a `cvar_t *`,
score, timer, entity pointer, or similar changing value in a file-global
variable.

### Add the modifier to the build

Add the file to `game_la_SOURCES` in
`src/game/default/Makefile.am`:

```make
	g_mode_deathmatch.c \
	g_mode_hello.c \
	g_mode_instagib.c \
```

If the generated build files do not know about the new source yet, regenerate
and configure them using the same options as your normal build:

```sh
autoreconf -i
./configure
make -j"$(nproc)"
```

### Register the descriptor

Near the top of `src/game/default/g_mode.c`, declare the definition function:

```c
const g_mode_def_t *G_HelloModeDefinition(void);
```

Add one slot to `g_mode_defs`:

```c
static const g_mode_def_t *g_mode_defs[] = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL, // hello
};
```

Then assign that slot in `G_ModeInit`:

```c
g_mode_defs[6] = G_HelloModeDefinition();
```

The runtime validates duplicate names, entity class names, and item class
names during initialization. Keep every descriptor name unique and stable.

### Add the modifier to the level composition

In `G_Worldspawn` in `src/game/default/g_entity.c`, append the modifier after
the legacy gameplay modifier has been selected:

```c
const cvar_t *hello = gi.GetCvar("g_hello");
if (hello && hello->integer &&
    num_modifiers < G_MODE_MAX_MODIFIERS) {
  modifier_names[num_modifiers++] = "hello";
}
```

Place it before or after other modifiers according to the desired hook order.
This timer does not conflict with anything, but transformer and handler hooks
can be order-sensitive.

Start a new map after changing the cvar:

```text
set g_hello 1
map edge
```

The active composition is resolved during worldspawn. Changing only the cvar
in the middle of a level does not rebuild the composition.

### Optional exercise: let the modifier create an entity

A modifier has the same entity facilities as a primary mode. For practice,
add an entity pointer to `g_hello_state_t`, allocate it in `LevelBegin` with
`G_AllocEntity("hello_entity")`, and give it a `Think` callback whose
`next_think` is updated every few seconds.

For production code, remember that worldspawn activates modes before the
remaining map entities have been read. Use a registered map entity class when
placement belongs in the map, or wait until `Frame` when you need existing
spawn points. The next tutorial demonstrates both the entity-class mechanism
and a safe runtime fallback.

## Tutorial 1B: a Double Damage modifier

Hello is useful for learning lifecycle and state. Double Damage is a second
small example that changes gameplay through an ordered transformer hook.

Create `src/game/default/g_mode_double_damage.c`:

```c
#include "g_local.h"

static const g_mode_cvar_def_t g_double_damage_cvars[] = {
  {
    .name = "g_double_damage",
    .default_value = "0",
    .flags = CVAR_SERVER_INFO,
    .description = "Enables the Double Damage modifier.",
  },
  {
    .name = "g_double_damage_scale",
    .default_value = "2",
    .flags = 0,
    .description = "Damage multiplier used by Double Damage.",
  },
};

static void G_DoubleDamageModifyDamage(g_mode_t *mode, g_damage_t *damage,
                                       bool *cancel) {
  (void) mode;
  (void) cancel;

  const cvar_t *scale = gi.GetCvar("g_double_damage_scale");
  if (!damage || !scale) {
    return;
  }

  damage->damage = (int32_t) (damage->damage * Maxf(scale->value, 0.f));
}

static const g_mode_ops_t g_double_damage_ops = {
  .ModifyDamage = G_DoubleDamageModifyDamage,
};

static const g_mode_def_t g_double_damage_mode = {
  .name = "double_damage",
  .kind = G_MODE_MODIFIER,
  .gameplay_selector = false,
  .cvars = g_double_damage_cvars,
  .num_cvars = lengthof(g_double_damage_cvars),
  .ops = &g_double_damage_ops,
};

const g_mode_def_t *G_DoubleDamageModeDefinition(void) {
  return &g_double_damage_mode;
}
```

Register the source and descriptor exactly as for Hello, using the name
`double_damage`. Append it during worldspawn when its cvar is enabled:

```c
const cvar_t *double_damage = gi.GetCvar("g_double_damage");
if (double_damage && double_damage->integer &&
    num_modifiers < G_MODE_MAX_MODIFIERS) {
  modifier_names[num_modifiers++] = "double_damage";
}
```

`ModifyDamage` receives a mutable copy of the incoming request. It may change
damage, knockback, flags, attacker, or other request fields. It may also set
`*cancel = true` to reject the operation entirely.

The primary mode's damage transformer runs first, followed by modifiers in
their composition order. For integer damage, doubling and then applying a
resistance modifier can round differently from applying resistance first.
Choose the order deliberately and test it with Techs.

## Tutorial 2: a “Touch the Entity” primary mode

This mode ends the match when a player touches a glowing goal. A player may
use at most three jumps on the way to the goal. Touching it after using too
many jumps respawns the player and resets their counter.

This example is intentionally small, but it exercises more of the architecture:

- primary-mode selection;
- a mode-owned entity class;
- runtime entity creation;
- per-level state;
- per-client AoS state;
- `ClientBegin`, `ClientFrame`, and `CheckRules`; and
- an entity `Touch` callback.

The jump rule is a **challenge limit**, not a change to the physics code.
`ClientFrame` observes successful jumps using `cl->jump_time`. If a future mode
must physically prevent jumping, add a generic movement-policy operation to
the mode interface and the common movement component. Do not add a
`touch_mode` conditional inside `bg_pmove.c`.

Create `src/game/default/g_mode_touch.c`:

```c
#include "g_local.h"

typedef struct {
  bool goal_spawned;
  bool won;
  g_entity_t *goal;
} g_touch_state_t;

typedef struct {
  g_mode_client_t base;
  uint32_t last_jump_time;
  uint16_t jumps;
} g_touch_client_t;

_Static_assert(offsetof(g_touch_client_t, base) == 0,
               "touch client data must begin with g_mode_client_t");

static const g_mode_cvar_def_t g_touch_cvars[] = {
  {
    .name = "g_touch",
    .default_value = "0",
    .flags = CVAR_SERVER_INFO,
    .description = "Enables Touch the Entity mode.",
  },
  {
    .name = "g_touch_max_jumps",
    .default_value = "3",
    .flags = CVAR_SERVER_INFO,
    .description = "Maximum jumps allowed before touching the goal.",
  },
};

static g_touch_client_t *G_TouchClient(g_mode_t *mode, g_client_t *cl) {
  if (!mode || !cl || !mode->client_data) {
    return NULL;
  }

  g_touch_client_t *data = G_ModeClientData(mode, cl->ps.client);
  if (data->base.client != cl ||
      data->base.generation != mode->generation) {
    return NULL;
  }

  return data;
}

static void G_TouchGoalTouch(g_entity_t *ent, g_entity_t *other,
                             const cm_trace_t *trace) {
  (void) trace;

  if (!other || !other->client) {
    return;
  }

  g_mode_t *mode = G_ModeActive();
  if (!mode || !mode->def || q_strcmp(mode->def->name, "touch")) {
    return;
  }

  g_touch_client_t *client = G_TouchClient(mode, other->client);
  const cvar_t *max_jumps_cvar = gi.GetCvar("g_touch_max_jumps");
  int32_t max_jumps = max_jumps_cvar ? max_jumps_cvar->integer : 3;
  if (max_jumps < 0) {
    max_jumps = 0;
  }

  if (client && client->jumps > max_jumps) {
    gi.ClientPrint(other->client, PRINT_HIGH,
                   "Too many jumps (%u of %d). Try again!\n",
                   (unsigned) client->jumps, max_jumps);
    G_ClientRespawn(other->client, false);
    client->jumps = 0;
    client->last_jump_time = other->client->jump_time;
    return;
  }

  g_touch_state_t *state = G_ModeState(mode);
  if (!state || state->won) {
    return;
  }

  state->won = true;
  ent->solid = SOLID_NOT;
  gi.LinkEntity(ent);

  gi.BroadcastPrint(PRINT_HIGH, "%s touched the goal!\n",
                    other->client->persistent.net_name);
}

static void G_TouchSpawnGoal(g_mode_t *mode, g_entity_t *ent, void *data) {
  (void) data;

  const g_mode_context_t *context = G_ModeContext(mode);
  g_touch_state_t *state = G_ModeState(mode);
  if (!context || !context->items || !state) {
    G_FreeEntity(ent);
    return;
  }

  ent->solid = SOLID_TRIGGER;
  ent->move_type = MOVE_TYPE_NONE;
  ent->bounds = Box3(
    Vec3(-20.f, -20.f, -20.f),
    Vec3(20.f, 20.f, 20.f)
  );
  ent->s.model1 = context->items[POWERUP_QUAD].model_index;
  ent->s.effects = EF_ROTATE | EF_LIGHT | EF_LIGHT_PULSE;
  ent->Touch = G_TouchGoalTouch;
  gi.LinkEntity(ent);

  state->goal_spawned = true;
  state->goal = ent;
}

static const g_mode_entity_class_def_t g_touch_entities[] = {
  {
    .classname = "info_touch_goal",
    .Spawn = G_TouchSpawnGoal,
  },
};

static void G_TouchClientBegin(g_mode_t *mode, g_client_t *cl) {
  g_touch_client_t *data = G_TouchClient(mode, cl);
  if (!data) {
    return;
  }

  data->last_jump_time = cl->jump_time;
  data->jumps = 0;
}

static void G_TouchClientFrame(g_mode_t *mode, g_client_t *cl) {
  g_touch_client_t *data = G_TouchClient(mode, cl);
  if (!data || !cl->entity || cl->entity->dead) {
    return;
  }

  if (cl->jump_time != data->last_jump_time) {
    data->last_jump_time = cl->jump_time;
    data->jumps++;
  }
}

static void G_TouchFrame(g_mode_t *mode) {
  g_touch_state_t *state = G_ModeState(mode);
  const g_mode_context_t *context = G_ModeContext(mode);
  if (!state || state->goal_spawned || !context || !context->level ||
      !context->level->spawn_points.spots ||
      !context->level->spawn_points.count) {
    return;
  }

  // A mapper may place info_touch_goal explicitly. If none was placed,
  // create one at a deathmatch spawn after map entity initialization.
  const uint32_t index = context->level->spawn_points.count > 1 ? 1 : 0;
  const g_entity_t *spot = context->level->spawn_points.spots[index];
  g_entity_t *goal = G_AllocEntity("info_touch_goal");
  goal->s.origin = spot->s.origin;
  goal->s.origin.z += 48.f;
  G_TouchSpawnGoal(mode, goal, NULL);
}

static bool G_TouchCheckRules(g_mode_t *mode) {
  const g_touch_state_t *state = G_ModeState(mode);
  return state && state->won;
}

static const g_mode_ops_t g_touch_ops = {
  .Frame = G_TouchFrame,
  .ClientBegin = G_TouchClientBegin,
  .ClientFrame = G_TouchClientFrame,
  .CheckRules = G_TouchCheckRules,
};

static const g_mode_def_t g_touch_mode = {
  .name = "touch",
  .kind = G_MODE_PRIMARY,
  .state_size = sizeof(g_touch_state_t),
  .client_data_size = sizeof(g_touch_client_t),
  .cvars = g_touch_cvars,
  .num_cvars = lengthof(g_touch_cvars),
  .entity_classes = g_touch_entities,
  .num_entity_classes = lengthof(g_touch_entities),
  .ops = &g_touch_ops,
};

const g_mode_def_t *G_TouchModeDefinition(void) {
  return &g_touch_mode;
}
```

### Why the goal is created in `Frame`

Worldspawn selects and activates the mode while the BSP entity list is still
being read. At `LevelBegin`, the map's deathmatch spawn list is not ready yet.
By the first `Frame`, `G_InitSpawnPoints` has populated it.

If the map contains an `info_touch_goal`, the registered entity class spawns
it and sets `goal_spawned`. Otherwise the first frame creates a fallback goal
near a player spawn. This also demonstrates that **both a primary mode and a
modifier may call `G_AllocEntity`**.

For a real mode, explicit mapper placement is preferable. The fallback keeps
the tutorial testable on existing maps.

### Register and select the primary mode

Repeat the build and registry steps from the Hello modifier:

1. add `g_mode_touch.c` to `game_la_SOURCES`;
2. declare `G_TouchModeDefinition` in `g_mode.c`;
3. add another `NULL` registry slot; and
4. assign that slot in `G_ModeInit`.

If you add all three tutorials to the current registry, the new portion looks
like this:

```c
const g_mode_def_t *G_HelloModeDefinition(void);
const g_mode_def_t *G_DoubleDamageModeDefinition(void);
const g_mode_def_t *G_TouchModeDefinition(void);

// Existing assignments occupy slots 0 through 5.
g_mode_defs[6] = G_HelloModeDefinition();
g_mode_defs[7] = G_DoubleDamageModeDefinition();
g_mode_defs[8] = G_TouchModeDefinition();
```

The array must contain nine `NULL` slots in that case. Treat these numbers as
positions in the current fixed registry, not as permanent public IDs.

Then choose the primary mode in `G_Worldspawn`:

```c
const cvar_t *touch = gi.GetCvar("g_touch");
const char *mode_name = touch && touch->integer ?
    "touch" : G_ModePrimaryName(g_level.ctf);
```

This replaces the existing single `mode_name` declaration. The modifier list
is built exactly as before, so Instagib, Arena, Techs, or Hello can still be
composed with the Touch mode.

Build, start a map, and test:

```text
set g_touch 1
set g_touch_max_jumps 3
map edge
```

Touching the glowing goal with three or fewer jumps makes `CheckRules` return
true. The common match code then begins intermission. The mode asks for a
match-ending operation; it does not call private intermission functions.

## The descriptor, field by field

Most fields may be omitted when they are not needed.

| Field | Purpose |
|---|---|
| `name` | Stable registry and composition name. Required. |
| `kind` | `G_MODE_PRIMARY` or `G_MODE_MODIFIER`. Required. |
| `gameplay_selector` | Whether a modifier is selected by legacy `g_gameplay`. |
| `gameplay` | Legacy `g_gameplay_t` value when `gameplay_selector` is true. |
| `capabilities` | Behavior advertised to common code. |
| `state_size` | Bytes of private per-level state. |
| `entity_data_size` | Size of one private entity record. |
| `client_data_size` | Size of one private client record. |
| `objective_cvar` | Optional cvar used to enable an objective primary mode. |
| `capture_limit_cvar` | Optional cvar used by the common capture-limit path. |
| `cvars` / `num_cvars` | Declarative cvars owned by this module. |
| `entity_classes` / `num_entity_classes` | Mode-owned map entity classes. |
| `items` / `num_items` | Built-in item aliases or dynamic items. |
| `ops` | Hooks implemented by the mode. |

Compiled mode cvars are registered during `G_ModeInit`, even when that mode is
not active. Read a cvar with `gi.GetCvar` when it is needed instead of keeping
a mutable global pointer to it.

## Choosing hooks

Declare only the hooks your module needs. A missing hook means “use common
behavior” or “let another composed module answer.”

| Need | Hook |
|---|---|
| Load sounds or initialize level state | `LevelBegin` |
| Final mode cleanup | `LevelEnd` |
| Run logic every server frame | `Frame` |
| Recreate objectives or special items | `ResetItems` |
| Update one client every frame | `ClientFrame` |
| Initialize or clean up client behavior | `ClientBegin`, `ClientDisconnect` |
| Replace starting inventory | `ClientInventory` |
| Observe every entity slot allocation/free | `EntitySpawn`, `EntityFree` |
| Handle a special pickup, drop, or reset | item hooks |
| Influence generic bot planning | bot hooks |
| Change or cancel incoming damage | `ModifyDamage` |
| React after health damage was applied | `DamageApplied` |
| Change weapon firing time | `ModifyWeaponInterval` |
| Own a respawn request | `Respawn` |
| Select a special spawn point | `SelectSpawn` |
| Own team assignment | `AssignTeam` |
| End a match using custom rules | `CheckRules` |

Hooks fall into three useful categories:

- **Observers** such as `Frame`, `ClientFrame`, and `DamageApplied` run for
  every composed module that implements them.
- **Transformers** such as `ModifyDamage`, `BotDirectives`, and
  `ModifyWeaponInterval` run in order and pass their changed value onward.
- **Handlers** such as `Respawn`, `ClientInventory`, and `CheckRules` return
  true when they have fully handled the operation. Later handlers and the
  common fallback are then skipped.

Read the dispatch function in `g_mode.c` before using a less common hook. Its
return-value and ordering rules are the authoritative behavior.

## Adding private mutable state

There are three kinds of owner-scoped storage. The runtime allocates and
zeroes all of them when the mode is activated for a level.

### Mode-wide state: tracking who is “it”

Some data belongs to the mode as a whole rather than to every client. Tag has
exactly one “it” player, so storing an `is_it` boolean in every client record
would create several possible sources of truth. Store the relationship once:

```c
typedef struct {
  g_client_t *it;
  uint32_t changes;
} g_tag_state_t;

static g_tag_state_t *G_TagState(g_mode_t *mode) {
  return G_ModeState(mode);
}
```

Request exactly one record in the descriptor:

```c
.state_size = sizeof(g_tag_state_t),
```

This is “global” within the active Tag instance and level. It is not a
process-global variable. The runtime allocates, zeroes, owns, and releases it
with that mode:

```text
active Tag mode
└── g_tag_state_t
    ├── it ───────────────► one current client
    └── changes
```

A simple damage-based tag transfer can use `DamageApplied`:

```c
static void G_TagDamageApplied(g_mode_t *mode, const g_damage_t *damage,
                               int32_t damage_health, bool was_dead) {
  (void) was_dead;

  g_tag_state_t *state = G_TagState(mode);
  if (!state || !damage || damage_health <= 0 ||
      !damage->attacker || !damage->attacker->client ||
      !damage->target || !damage->target->client ||
      damage->attacker == damage->target ||
      state->it != damage->attacker->client) {
    return;
  }

  state->it = damage->target->client;
  state->changes++;
  gi.BroadcastPrint(PRINT_HIGH, "%s is it!\n",
                    state->it->persistent.net_name);
}
```

Pointers in mode-wide state must follow object lifecycles. Clear the pointer
when that client disconnects:

```c
static void G_TagClientDisconnect(g_mode_t *mode, g_client_t *cl) {
  g_tag_state_t *state = G_TagState(mode);
  if (state && state->it == cl) {
    state->it = NULL;
  }
}
```

Connect both callbacks to the Tag operations table:

```c
static const g_mode_ops_t g_tag_ops = {
  .DamageApplied = G_TagDamageApplied,
  .ClientDisconnect = G_TagClientDisconnect,
};
```

The mode's `Frame` or round component can choose another eligible client when
`it` is `NULL`. Keep the selection rule in one helper so connect, disconnect,
round-start, and admin paths cannot disagree.

Use per-client AoS data for facts that genuinely exist once per participant,
such as how long each player has been “it” or a bot's Tag strategy. Use the
single mode-wide record for unique relationships and shared counters.

The same rule applies to modifiers. A modifier may have one shared owner,
timer, vote, queue, or objective record by declaring `state_size`.

### Other mode-wide per-level state

Use this for data that exists once per active mode instance:

```c
typedef struct {
  uint32_t round;
  uint32_t next_round_time;
  uint16_t start_sound;
} g_my_mode_state_t;

static void G_MyModeLevelBegin(g_mode_t *mode, const char *map_name,
                               const cm_entity_t *props) {
  (void) map_name;
  (void) props;

  g_my_mode_state_t *state = G_ModeState(mode);
  if (!state) {
    return;
  }

  state->start_sound = gi.SoundIndex("my_mode/start");
}
```

Request it in the descriptor:

```c
.state_size = sizeof(g_my_mode_state_t),
```

Always check an accessor result before dereferencing it. A missing size,
inactive module, or lifecycle mistake should not become a crash.

### Per-client AoS data

Use one Array-of-Structures record per client when every player or bot needs
private mode data:

```c
typedef struct {
  g_mode_client_t base;
  int16_t role;
  uint32_t respawn_at;
} g_my_mode_client_t;

_Static_assert(offsetof(g_my_mode_client_t, base) == 0,
               "client data must begin with g_mode_client_t");
```

Request a slab containing one record per configured client:

```c
.client_data_size = sizeof(g_my_mode_client_t),
```

Access a record by client slot:

```c
static g_my_mode_client_t *G_MyModeClient(g_mode_t *mode, g_client_t *cl) {
  if (!mode || !cl || !mode->client_data) {
    return NULL;
  }

  g_my_mode_client_t *data = G_ModeClientData(mode, cl->ps.client);
  if (data->base.client != cl ||
      data->base.generation != mode->generation) {
    return NULL;
  }

  return data;
}
```

The runtime initializes the prefix before `ClientBegin` and clears the record
after `ClientDisconnect`. Put mod-specific persistent bot parameters here too:
for example a role, aggression setting, objective assignment, or a small
handle into other mode-owned data. This avoids adding mode fields to
`g_client_t` and avoids teaching the AI core about a particular mode.

### Per-entity AoS data

Use one record per entity slot when entities need mode-private metadata:

```c
typedef struct {
  g_mode_entity_t base;
  uint8_t objective_kind;
  uint32_t reset_at;
} g_my_mode_entity_t;

_Static_assert(offsetof(g_my_mode_entity_t, base) == 0,
               "entity data must begin with g_mode_entity_t");
```

Request the slab:

```c
.entity_data_size = sizeof(g_my_mode_entity_t),
```

Validate both the pointer and spawn ID when reading it:

```c
static g_my_mode_entity_t *G_MyModeEntity(g_mode_t *mode,
                                          const g_entity_t *ent) {
  if (!mode || !ent || !mode->entity_data) {
    return NULL;
  }

  g_my_mode_entity_t *data = G_ModeEntityData(mode, ent->s.number);
  if (data->base.entity != ent ||
      data->base.spawn_id != ent->s.spawn_id) {
    return NULL;
  }

  return data;
}
```

Entity slots are reused. The spawn-ID check prevents stale data from an older
entity in the same slot from being mistaken for current state.

The entity slab contains one record per configured entity slot and is bounded
by `MAX_ENTITIES`. The client slab is similarly bounded by `MAX_CLIENTS`.
Both use aligned AoS strides, following the familiar slot-oriented design of
`g_entity_t`.

## Adding map entity classes

A mode can register classnames that only exist while it is active:

```c
static void G_MyModeSpawnGoal(g_mode_t *mode, g_entity_t *ent, void *data) {
  g_my_mode_entity_t *mode_ent = data;

  if (mode_ent) {
    mode_ent->objective_kind = 1;
  }

  ent->solid = SOLID_TRIGGER;
  ent->bounds = Box3(
    Vec3(-16.f, -16.f, -16.f),
    Vec3(16.f, 16.f, 16.f)
  );
  gi.LinkEntity(ent);
}

static void G_MyModeDestroyGoal(g_mode_t *mode, g_entity_t *ent, void *data) {
  (void) mode;
  (void) ent;
  (void) data;
}

static const g_mode_entity_class_def_t g_my_mode_entities[] = {
  {
    .classname = "info_my_mode_goal",
    .Spawn = G_MyModeSpawnGoal,
    .Destroy = G_MyModeDestroyGoal,
  },
};
```

Add them to the descriptor:

```c
.entity_classes = g_my_mode_entities,
.num_entity_classes = lengthof(g_my_mode_entities),
```

Use the `Destroy` callback for class-specific cleanup. Use the general
`EntityFree` hook only when you need to observe all freed entities.

Keep a few small entity callbacks in the mode file. Put a large entity or a
large collection of small entities in a separate file such as
`g_mode_my_mode_entity.c`, and add that source file to `Makefile.am`. Expose a
small mode-local function or declaration rather than moving private state into
a global variable.

## Adding mode-specific items

There are two ways to register an item.

### Alias a built-in item

Use an alias when the complete item already exists in the common catalog:

```c
static const g_item_t *G_MyModeResolveItem(g_mode_t *mode) {
  const g_mode_context_t *context = G_ModeContext(mode);
  return context ? &context->items[ITEM_SOME_EXISTING_TAG] : NULL;
}

static const g_mode_item_def_t g_my_mode_items[] = {
  {
    .classname = "item_my_alias",
    .Resolve = G_MyModeResolveItem,
  },
};
```

The default `dynamic` value is false, so this descriptor uses the existing
catalog item and tag.

### Register a dynamic item

Use a dynamic item when the mode owns a distinct item definition:

```c
static const g_item_t g_my_item_prototype = {
  .def = {
    .name = "My Mode Item",
    .classname = "item_my_mode",
    .type = ITEM_TYPE_POWERUP,
    .quantity = 30,
  },
  .Pickup = G_MyItemPickup,
  .Use = G_MyItemUse,
  .Drop = G_MyItemDrop,
};

static const g_item_t *G_MyModeResolveItem(g_mode_t *mode) {
  (void) mode;
  return &g_my_item_prototype;
}

static const g_mode_item_def_t g_my_mode_items[] = {
  {
    .classname = "item_my_mode",
    .Resolve = G_MyModeResolveItem,
    .dynamic = true,
  },
};
```

Then reference the array from the descriptor:

```c
.items = g_my_mode_items,
.num_items = lengthof(g_my_mode_items),
```

The runtime clones a dynamic prototype, assigns an available inventory tag,
and publishes its presentation metadata to cgame. Do not hard-code or infer
the assigned tag. Resolve the runtime item through the mode registry, keep the
`g_item_t *` on the entity, or use `G_ModeItemByTag`.

If a dynamic weapon uses dynamic ammunition, provide `ResolveAmmo`. It runs
after every dynamic tag has been allocated, so the runtime can safely connect
the two definitions.

A prototype must be complete enough for its item type. Include its pickup,
use, drop, and think callbacks when common item initialization cannot supply
them.

Remember that dropped items may be created by a different path from map
items. Put shared visual and team initialization in a helper and call it from
both paths. Do not derive a team, color, or model from a dynamic inventory tag;
tags describe runtime catalog position, not semantic identity.

## Changing bot behavior

Modes should express bot policy through these hooks instead of adding
mode-name checks to the generic AI:

- `BotDirectives` adjusts general policy, currently including item weight and
  an `int16_t` role;
- `BotTarget` adjusts the priority and chase multiplier for a target; and
- `BotCanPickup` can make a final decision about a particular pickup.

For per-bot persistent settings, request `client_data_size` and add the fields
to the mode's client record. Human clients get the same record, so check
`cl->ai` when behavior should apply only to bots.

Example:

```c
static void G_MyModeBotDirectives(g_mode_t *mode, g_client_t *cl,
                                  const g_entity_t *ent,
                                  g_mode_bot_directives_t *directives) {
  (void) ent;

  if (!cl || !cl->ai) {
    return;
  }

  g_my_mode_client_t *data = G_MyModeClient(mode, cl);
  if (data) {
    directives->role = data->role;
  }
}
```

This keeps the bot planner generic: it consumes policy outputs without
calling a specific mode directly.

## Owning damage, spawning, or respawning

These hooks let a module alter important common behavior without replacing
the entire common subsystem.

### Damage

`ModifyDamage` receives a mutable copy of the damage request. It can change
damage, knockback, flags, or other fields. Set `*cancel = true` to stop the
damage operation entirely. Later modifiers do not run after cancellation.

`DamageApplied` runs after armor and health damage have been calculated. Use
it for effects such as life steal or damage-based scoring.

### Weapon timing

`ModifyWeaponInterval` returns a new firing interval. Each composed module
receives the value returned by the previous module.

### Spawn selection

`SelectSpawn` returns an entity when the mode wants a particular spawn point.
Return `NULL` to allow the next modifier or common spawn logic to choose.

Keep reusable spawn searches and safety scoring in the spawn component. A mode
should normally provide policy, filters, or a selected spawn class.

### Respawning

`Respawn` returns true when the mode has fully handled the request. This can
mean spawning immediately or adding the client to a mode-owned queue. Return
false to let later modifiers and common respawn logic continue.

A queue usually needs:

- per-client data for queue position and eligibility;
- per-level state for queue timing or round state;
- `Respawn` to enqueue;
- `Frame` to release queued clients; and
- `ClientDisconnect` to remove stale participants.

If queue logic can be reused by several modes, put the queue mechanics in a
common component and let each mode configure its policy.

## Adding a new primary mode

Begin with the same descriptor pattern, but use:

```c
static const g_mode_def_t g_race_mode = {
  .name = "race",
  .kind = G_MODE_PRIMARY,
  .ops = &g_race_mode_ops,
};
```

Register the file and descriptor as described above. Then add explicit
selection logic.

The current compatibility helper `G_ModePrimaryName(bool objective)` selects
CTF when a flag objective is enabled and Deathmatch otherwise. A second
primary-mode selector cannot be represented by that boolean. Extend
worldspawn resolution and the helper interface so that each primary mode has
an unambiguous cvar or map setting and exactly one primary name is passed to
`G_ModeBeginLevel`.

Do not rely on registry order to choose among multiple primary modes. Make the
precedence visible in the selection code and document conflicting settings.

If the mode is an objective mode, it may provide:

```c
.objective_cvar = "g_race",
.capture_limit_cvar = "g_lap_limit",
```

Only use `capture_limit_cvar` if the common capture-limit semantics are
appropriate. A different win condition can be implemented in `CheckRules`.

## Adding a legacy `g_gameplay` modifier

Instagib and Arena are selected by the existing `g_gameplay` setting. A new
modifier should join that selector only when it is mutually exclusive with
those gameplay variants.

To do so:

1. add a distinct value to `g_gameplay_t` in `g_types.h`;
2. set `.gameplay_selector = true`;
3. set `.gameplay` to the new enum value;
4. update `G_GameplayName` in `g_util.c` for its display name; and
5. register the mode as usual.

`G_ModeModifierName` and `G_ModeGameplayByName` only inspect modifier
descriptors whose `gameplay_selector` is true. This is important because a
zero-initialized `gameplay` field equals `GAME_DEATHMATCH`. Independent
modifiers must leave `gameplay_selector` false and be composed explicitly.

## Capabilities

Capabilities let common code ask about behavior without naming a mode:

```c
if (G_ModeHasCapability(G_MODE_CAP_TEAMPLAY)) {
  // Common team behavior.
}
```

Use an existing capability when it describes the behavior correctly. Add a
new capability bit to `g_mode_capability_t` only when common code genuinely
needs to branch on that behavior.

Good capability: “this composition suppresses ordinary map items.”

Poor capability: “the active mode is called race.”

Capabilities from the primary mode and all active modifiers are combined.

## File layout

For a small mode, one file is enough:

```text
g_mode_low_gravity.c
```

As it grows, split by responsibility:

```text
g_mode_race.c
g_mode_race_entity.c
g_mode_race_item.c
```

The main mode file should retain the descriptor and high-level rules.
Mode-specific entity implementations belong in the entity file when they are
large or numerous. Reusable teams, spawning rules, match limits, and queue
mechanics belong in common component files rather than under one mode's name.

Keep private declarations private where practical. If several files need a
mode-private structure, add a narrowly scoped private header such as
`g_mode_race.h`; do not add those details to the public `g_mode.h` interface.

## Build and test checklist

After adding or changing a module:

1. Regenerate the build when a source was added to `Makefile.am`.
2. Build the game module with warnings visible.
3. Start a map with the module disabled and verify normal behavior.
4. Start a map with the module enabled and verify its hooks run.
5. Test it with every modifier it is expected to support.
6. Connect, disconnect, and reconnect clients to exercise slot reuse.
7. Spawn and free mode entities to exercise entity slot reuse.
8. Change maps to exercise `LevelEnd`, allocation, and `LevelBegin`.
9. Test bots if the mode changes targets, pickups, teams, or inventories.
10. Test dropped and reset items, not only map-spawned items.

For a build configured with tests:

```sh
make -j"$(nproc)"
make check
```

`make check` may report zero tests when the build was not configured with
`--with-tests`. That is not the same as running and passing the test suite.

Useful runtime checks include:

```text
set developer 1
set g_hello 1
set g_double_damage 1
set g_touch 1
map edge
```

Add temporary logging at the module boundary rather than scattering it
through common code, and remove it before submitting the change.

## Common mistakes

### “The mode is registered, but nothing happens”

The descriptor is known, but the mode was not added to the worldspawn
composition. Registration and activation are separate steps.

### The module is active twice

An independent modifier was accidentally marked as a legacy gameplay
selector, or worldspawn appended the same name more than once. Keep
`gameplay_selector` false unless `g_gameplay` owns selection.

### A state accessor returns `NULL`

Check that the matching size field is present in the descriptor and that the
module is active. Guard the result before use. Also inspect designated
initializers carefully: repeating a field such as `.state_size` can silently
replace the earlier value on some compiler configurations.

### Client or entity data belongs to an old slot occupant

The private record omitted the required prefix, or the code did not validate
the generation or spawn ID. Put `g_mode_client_t` or `g_mode_entity_t` first
and add a `_Static_assert`.

### A dynamic item has the wrong identity or appearance

The code assumed a runtime tag was a team or item index. Resolve the item from
the mode-owned registry and store semantic identity separately. Apply shared
initialization to both map-spawned and dropped instances.

### One modifier unexpectedly overrides another

Review composition order and hook semantics. Handler hooks stop at the first
successful result. Transformer hooks apply sequentially.

### A fifth modifier does not run

The current composition has `G_MODE_MAX_MODIFIERS` slots. Increase the limit
deliberately or reduce the active modifier set; never write past the array or
append without checking `num_modifiers`.

### A mode works once and crashes after a map change

Look for cached pointers to level allocations, entities, clients, items,
cvars, or media in mutable file-global variables. Put changing values in the
mode state or AoS records and rebuild references during `LevelBegin`.

## Removing a mode cleanly

The architecture is intended to make removals mechanical. To remove a mode:

1. remove its selection or composition code;
2. remove its definition function from `g_mode.c`;
3. remove its registry slot or assignment;
4. remove its source files from `Makefile.am`;
5. remove mode-only enum values, capability bits, cgame presentation, and
   assets if nothing else uses them;
6. regenerate the build files; and
7. build and test the remaining compositions.

Common code should depend on hooks and capabilities, not the removed mode's
symbols. Any remaining direct dependency is a useful sign that behavior still
needs to move behind the mode boundary or into a reusable component.

## Existing examples to study

- `g_mode_instagib.c`: a very small inventory modifier;
- `g_mode_arena.c`: another legacy gameplay modifier;
- `g_mode_techs.c`: a reusable modifier with cvars, items, private level
  state, client AoS data, damage, and weapon timing hooks;
- `g_mode_ctf.c`: a full primary objective mode with entity classes, dynamic
  items, level state, entity and client AoS records, bots, and match rules;
- `g_mode_spawn.c`: reusable spawn selection; and
- `g_mode_team.c`: reusable team behavior.

For architectural background and tradeoffs, also read:

- [game-modes-refactor-pr.md](game-modes-refactor-pr.md);
- [game-modes-refactor-summary.md](game-modes-refactor-summary.md); and
- [game-modes-refactor-report.md](game-modes-refactor-report.md).

For content extensions, read:

- [how-to-add-weapon.md](how-to-add-weapon.md); and
- [how-to-add-mode-item.md](how-to-add-mode-item.md).
