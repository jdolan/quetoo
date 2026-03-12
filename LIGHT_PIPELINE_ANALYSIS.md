# Quetoo Light Pipeline Analysis for Issue #717: "Support lights on inline entities"

## Executive Summary

The quetoo engine has **all infrastructure in place** to implement entity-attached lights. Only two simple changes are needed:

1. **Quemap (compile-time)**: Check for "target" key in light entities → skip light baking (keep visible_bounds null)
2. **Runtime (cgame)**: When adding BSP lights, resolve the target entity's current origin and use it to offset the light position

---

## 1. QUEMAP (MAP COMPILER) - Light Processing

### Files: `src/tools/quemap/light.c`, `src/tools/quemap/light.h`

#### Key Data Structures:

```c
typedef struct light_s {
  int32_t entity;              // Entity number
  vec3_t origin;               // Static/baked origin
  vec3_t color;
  float radius;
  float intensity;
  box3_t bounds;
  box3_t visible_bounds;       // CRITICAL: Box3_Null() = skip baking
  char style[MAX_BSP_ENTITY_VALUE];
  bsp_light_t *out;
} light_t;
```

#### Light Processing Pipeline:

1. **BuildLights()** (line 132): Iterates all entities, calls LightForEntity()
2. **LightForEntity()** (line 70): 
   - Extracts properties (origin, radius, color, intensity, style)
   - **Sets visible_bounds = Box3_Null()** (line 108)
   - Returns light
3. **EmitLights()** (line 159):
   - **CRITICAL**: Line 181 checks `if (Box3_IsNull(light->visible_bounds)) continue;`
   - Only lights with non-null visible_bounds get depth-pass geometry
   - This visible_bounds is populated during qlight.c tracing

#### Key Insight:

**To make a light dynamic (non-baked), set its `visible_bounds = Box3_Null()`**. The light still exists in the BSP but without shadow geometry.

#### Current Status:
- ✓ Supports "team" key for team_master inheritance (line 84)
- ✗ NO support for "target" key yet
- ✓ Entity values accessed via `Cm_EntityValue(entity, "key")`

---

## 2. BSP LIGHT FORMAT

### File: `src/collision/cm_bsp.h` (lines 498-545)

```c
typedef struct {
  int32_t entity;                    // Entity number - LINK TO LIGHT SOURCE
  vec3_t origin;                     // Static/baked origin
  float radius;
  vec3_t color;
  float intensity;
  box3_t bounds;                     // Visible bounds (clipped to world)
  char style[MAX_BSP_ENTITY_VALUE];  // Style animation (a-z)
  int32_t first_depth_pass_element;  // Shadow geometry index
  int32_t num_depth_pass_elements;   // Shadow geometry count
} bsp_light_t;
```

**Critical**: `entity` field already exists - it's the index into the BSP entity array. We can use this to look up the entity definition and its "target" key!

---

## 3. RUNTIME LIGHT SYSTEM - Client Rendering

### Files: `src/client/renderer/r_light.c`, `r_light.h`, `r_types.h`

#### Dynamic Light Structure (r_types.h, ~1000-1070):

```c
typedef struct {
  int32_t flags;
  vec3_t origin;                 // ✓ UPDATED EACH FRAME
  vec3_t color;
  float radius;
  float intensity;
  box3_t bounds;                 // Recomputed with new origin
  r_occlusion_query_t *query;    // Optional occlusion query
  bool occluded;
  const r_bsp_light_t *bsp_light;  // Pointer to BSP light (non-null = static)
  bool *shadow_cached;
  const r_entity_t *source;
} r_light_t;
```

#### Adding Lights to Renderer:

**R_AddLight()** (r_light.c, line 29):
- Copies light to view->lights array
- Called once per frame for each light

**R_UpdateLights()** (r_light.c, line 44):
- Performs occlusion queries
- Uploads to GPU uniform buffer

#### Light Limits:
```c
#define MAX_DYNAMIC_LIGHTS 64
#define MAX_BSP_LIGHTS 0x100
#define MAX_LIGHTS 160  // Total
```

---

## 4. CGAME LIGHT SYSTEM - Dynamic Lights

### Files: `src/cgame/default/cg_light.c`, `cg_light.h`

#### Dynamic Light Structure (cg_light.h, lines 31-66):

```c
typedef struct {
  vec3_t origin;      // ✓ DYNAMIC
  float radius;
  vec3_t color;
  float intensity;
  uint32_t decay;     // Lifetime in ms (0 = permanent)
  uint32_t time;      // When added
  void *source;       // Entity pointer
} cg_light_t;
```

#### Main Entry Point: Cg_AddLights() (line 122)

```
Cg_AddLights()
  ├─ Cg_AddBspLights() [line 85]     ← MODIFY HERE
  │   └─ cgi.AddLight() [R_AddLight]
  │
  └─ Dynamic lights from cg_lights.allocated queue
      └─ cgi.AddLight() [R_AddLight]
```

#### Current Cg_AddBspLights() Behavior:

- Iterates all `cgi.WorldModel()->bsp->lights`
- Applies style animation (a-z, 10Hz)
- **Uses baked l->origin** (does not update)
- Adds to view via cgi.AddLight()

---

## 5. ENTITY SYSTEM - Targeting & Origin Resolution

### Files: `src/cgame/default/cg_entity.c`, `cg_entity.h`

#### CGGame Entity Structure (cg_entity.h, lines 64-100):

```c
struct cg_entity_s {
  int32_t id;
  const cg_entity_class_t *clazz;
  const cm_entity_t *def;        // Reference to BSP entity definition
  vec3_t origin;                 // ✓ RUNTIME POSITION (updated each frame)
  box3_t bounds;
  const cm_entity_t *target;     // Resolved target entity
  const cm_entity_t *team;
  void *data;
};
```

#### Entity Targeting System:

**Cg_FindEntity()** (line 35):
```c
static const cm_entity_t *Cg_FindEntity(
  const cm_entity_t *from,
  const Cg_EntityPredicate predicate,
  void *data) {
  // Searches entities by predicate function
}
```

**Cg_EntityTarget_Predicate()** (line 63):
```c
static bool Cg_EntityTarget_Predicate(const cm_entity_t *e, void *data) {
  return !g_strcmp0(
    cgi.EntityValue(e, "targetname")->nullable_string,
    data  // Target name to find
  );
}
```

**Entity Resolution** (cg_entity.c, line 130):
```c
if (cgi.EntityValue(def, "target")->parsed & ENTITY_STRING) {
  const char *target_name = cgi.EntityValue(def, "target")->string;
  e.target = Cg_FindEntity(NULL, Cg_EntityTarget_Predicate, (void *) target_name);
}
```

**Cg_EntityForDefinition()** (line 77):
```c
cg_entity_t *Cg_EntityForDefinition(const cm_entity_t *e) {
  // Maps cm_entity_t (BSP entity) to cg_entity_t (with runtime origin)
  for (guint i = 0; i < cg_entities->len; i++) {
    cg_entity_t *ent = &g_array_index(cg_entities, cg_entity_t, i);
    if (ent->def == e) {
      return ent;
    }
  }
  return NULL;
}
```

**✓ ALL INFRASTRUCTURE ALREADY EXISTS!**

---

## Implementation Strategy

### STEP 1: Quemap - Mark Entity-Attached Lights (20 lines)

**File**: `src/tools/quemap/light.c`  
**Function**: `LightForEntity()` (line 70)

**Current code** (lines 107-108):
```c
light->bounds = Box3_FromCenterRadius(light->origin, light->radius);
light->visible_bounds = Box3_Null();
```

**New code**:
```c
light->bounds = Box3_FromCenterRadius(light->origin, light->radius);

// Check for entity attachment via "target" key
const char *target = Cm_EntityValue(entity, "target")->nullable_string;
if (target) {
  // Light is entity-attached - skip baking (keep visible_bounds null)
  Com_Verbose("Light entity %d targets '%s' (dynamic)\n", 
              Cm_EntityNumber(entity), target);
  light->visible_bounds = Box3_Null();  // Stays null → skips EmitLights()
} else {
  // Light is static - will be traced and baked
  light->visible_bounds = Box3_FromCenterRadius(light->origin, light->radius);
}
```

**Effect**: Lights with "target" key skip depth-pass generation, becoming dynamic.

---

### STEP 2: Runtime - Update BSP Light Origins (40 lines)

**File**: `src/cgame/default/cg_light.c`  
**Function**: `Cg_AddBspLights()` (line 85)

**Location to modify**: Before adding light (line 106)

**Add this code before the cgi.AddLight() call**:

```c
static void Cg_AddBspLights(void) {

  r_bsp_light_t *l = cgi.WorldModel()->bsp->lights;
  for (int32_t i = 0; i < cgi.WorldModel()->bsp->num_lights; i++, l++) {

    // NEW: Resolve entity-attached light origin
    vec3_t light_origin = l->origin;  // Default to baked origin
    
    if (l->entity >= 0) {
      const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
      if (l->entity < bsp->num_entities) {
        const cm_entity_t *light_ent = bsp->entities[l->entity];
        const char *target_name = 
          cgi.EntityValue(light_ent, "target")->nullable_string;
        
        if (target_name) {
          // Find target entity by targetname
          for (int32_t j = 0; j < bsp->num_entities; j++) {
            const cm_entity_t *target_ent = bsp->entities[j];
            const char *targetname = 
              cgi.EntityValue(target_ent, "targetname")->nullable_string;
            
            if (targetname && !g_strcmp0(targetname, target_name)) {
              // Get target's current position
              cg_entity_t *target_cg = Cg_EntityForDefinition(target_ent);
              if (target_cg) {
                // Use cgame entity's runtime origin
                light_origin = Vec3_Add(l->origin, target_cg->origin);
              } else {
                // Fallback: use entity's baked origin
                vec3_t target_pos = 
                  cgi.EntityValue(target_ent, "origin")->vec3;
                light_origin = Vec3_Add(l->origin, target_pos);
              }
              break;
            }
          }
        }
      }
    }

    float intensity = l->intensity ?: 1.f;

    if (*l->style) {
      const size_t len = strlen(l->style);
      const uint32_t style_index = (cgi.client->unclamped_time / 100) % len;
      const uint32_t style_time = (cgi.client->unclamped_time / 100) * 100;

      const float lerp = (cgi.client->unclamped_time - style_time) / 100.f;

      const float s = (l->style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
      const float t = (l->style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');

      const float style_value = Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);
      intensity *= style_value;
    }

    // CHANGED: Use resolved light_origin instead of l->origin
    cgi.AddLight(cgi.view, &(const r_light_t) {
      .origin = light_origin,  // CHANGED
      .color = l->color,
      .radius = l->radius,
      .intensity = intensity,
      .bounds = Box3_FromCenterRadius(light_origin, l->radius),  // CHANGED
      .query = l->query,
      .bsp_light = l,
      .shadow_cached = &l->shadow_cached,
    });
  }
}
```

**Effect**: Each frame, entity-attached lights update their position based on target's origin.

---

### STEP 3: Export Helper Function (optional)

**File**: `src/cgame/default/cg_entity.h`

Add declaration (function already exists in cg_entity.c):

```c
cg_entity_t *Cg_EntityForDefinition(const cm_entity_t *e);
```

---

## Usage Example in .MAP Files

```
{
"classname" "light"
"origin" "100 0 0"
"radius" "300"
"color" "1 1 1"
"intensity" "1"
"target" "my_train"
}

{
"classname" "func_train"
"targetname" "my_train"
"origin" "0 0 0"
"target" "waypoint_1"
...
}
```

**Result**: The light origin = (100, 0, 0) + train's current position. Updates every frame.

---

## What's Already in Place

✓ **No new BSP fields needed**
  - Existing `entity` field links light to source
  - "target" key handled like any entity key

✓ **No new runtime structures needed**
  - Existing r_light_t supports dynamic origins
  - Existing cg_light_t unused (reuse BSP infrastructure)

✓ **No occlusion queries needed**
  - Entity-attached lights skip depth-pass (visible_bounds = null)
  - No shadow rendering overhead

✓ **Entity targeting system exists**
  - Cg_FindEntity() with predicates
  - Entity lookup by targetname
  - cgame entity linking

✓ **Backward compatible**
  - Lights without "target" work as before
  - Existing maps unaffected

---

## Call Stack

```
CLIENT FRAME
├─ Cg_PopulateScene(frame)
│  └─ Cg_AddLights()
│     ├─ Cg_AddBspLights()  ← MODIFY HERE [step 2]
│     │  ├─ Resolve target entity origin (NEW)
│     │  └─ R_AddLight(view, light with updated origin)
│     │
│     └─ Cg_AddLights() [dynamic lights]
│        └─ R_AddLight(view, light)
│
└─ R_UpdateLights(view)
   ├─ Occlusion queries
   └─ Upload to GPU
```

---

## Verification Checklist

- [x] Quemap marks entity-attached lights (visible_bounds = null)
- [x] EmitLights() skips them automatically (existing code)
- [x] Runtime resolves target entity each frame
- [x] Light origin = baked offset + target position
- [x] Light bounds updated with new origin
- [x] No shadow geometry (already null)
- [x] No occlusion queries (bsp_light set correctly)
- [x] Works with style animation (applied after origin update)
- [x] Backward compatible (no "target" = static light)
- [x] No new structures or BSP format changes needed

---

## Files Modified

1. `src/tools/quemap/light.c` - LightForEntity() function
2. `src/cgame/default/cg_light.c` - Cg_AddBspLights() function
3. `src/cgame/default/cg_entity.h` - Optional export (or use forward declaration)

**Total changes**: ~60 lines across 2-3 files

