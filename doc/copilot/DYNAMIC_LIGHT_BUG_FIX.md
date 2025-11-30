# Dynamic Light Iteration Bug Fix

**Date:** November 29, 2025  
**File:** `src/cgame/default/cg_light.c`  
**Bug:** Decaying dynamic lights not appearing in-game

---

## Symptom

Dynamic lights with decay (lights that fade out over multiple frames) were not visible in-game, despite being properly allocated.

---

## Root Cause

**Iterator invalidation during list traversal!**

The code was iterating over a GQueue while simultaneously removing elements from it:

```c
// BUGGY CODE:
for (GList *list = cg_lights.allocated->head; list != NULL; list = list->next) {
    cg_light_t *light = list->data;
    // ...
    if (age >= light->decay) {
        Cg_FreeLight(light);  // ← Modifies the list we're iterating!
    }
}
```

When `Cg_FreeLight()` calls `g_queue_remove()`, it modifies the linked list structure. After removing an element, `list->next` points to invalid memory or skips elements, causing:
- Lights to disappear prematurely
- Iterator to break/skip remaining lights
- Some lights never getting processed

---

## The Fix

**Store the next pointer BEFORE any potential removal:**

```c
// FIXED CODE:
GList *list = cg_lights.allocated->head;
while (list != NULL) {
    GList *next = list->next;  // ← Save next BEFORE removal
    
    cg_light_t *light = list->data;
    
    const uint32_t age = cgi.client->unclamped_time - light->time;
    float intensity = light->intensity;
    if (light->decay) {
        intensity = Mixf(intensity, 0.f, age / (float) light->decay);
    }
    
    if (cg_add_lights->value) {
        cgi.AddLight(cgi.view, &(const r_light_t) {
            .origin = light->origin,
            .color = light->color,
            .radius = light->radius,
            .intensity = intensity,
            .bounds = Box3_FromCenterRadius(light->origin, light->radius),
            .source = light->source,
        });
    }
    
    if (age >= light->decay) {
        Cg_FreeLight(light);  // ← Safe now
    }
    
    list = next;  // ← Use saved pointer
}
```

---

## Why This Works

By storing `next` before calling `Cg_FreeLight()`:
1. We capture the correct next element in the list
2. If the current element is removed, we still have a valid pointer to continue
3. The iterator can safely proceed through all elements
4. Lights live their full lifetime and fade out properly

---

## What Should Work Now

- ✅ Muzzle flash lights (fade out over ~450ms)
- ✅ Explosion lights (fade over ~1000ms)
- ✅ Pickup effects lights
- ✅ Any dynamic light with a `decay` value

---

## Pattern for Safe List Iteration with Removal

This is a common pattern when you need to remove items during iteration:

```c
// SAFE PATTERN:
GList *list = queue->head;
while (list != NULL) {
    GList *next = list->next;  // Save next
    
    // Process element
    if (should_remove) {
        remove_from_list(list->data);  // Safe
    }
    
    list = next;  // Continue with saved pointer
}
```

---

## Summary

Fixed decaying dynamic lights by using safe iteration pattern that stores the next pointer before potential removal. Lights now persist their full lifetime and fade out correctly.

**Status:** ✅ Fixed! Dynamic lights now visible and decay properly. 🔦
