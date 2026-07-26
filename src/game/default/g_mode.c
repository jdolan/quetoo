/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "g_local.h"

#ifndef G_MODE_ENABLE_CTF
#define G_MODE_ENABLE_CTF 1
#endif

const g_mode_def_t *G_DefaultModeDefinition(void);
const g_mode_def_t *G_DeathmatchModeDefinition(void);
const g_mode_def_t *G_InstagibModeDefinition(void);
const g_mode_def_t *G_ArenaModeDefinition(void);
const g_mode_def_t *G_TechsModeDefinition(void);
#if G_MODE_ENABLE_CTF
const g_mode_def_t *G_CtfModeDefinition(void);
#endif

static const g_mode_def_t *g_mode_defs[] = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

static g_mode_t g_mode;
static g_mode_t g_mode_modifiers[G_MODE_MAX_MODIFIERS];
static bool g_mode_initialized;
static uint32_t g_mode_generation;
static g_mode_context_t g_mode_context;
static g_item_tag_t g_mode_next_item_tag;

static size_t G_ModeStride(const size_t size) {
  if (!size) {
    return 0;
  }

  const size_t alignment = _Alignof(max_align_t);
  return ((size + alignment - 1) / alignment) * alignment;
}

static void G_ModeRefreshContext(void) {
  // The server cvars are already clamped by the server, but keep the mode
  // allocator bounded by the same compile-time capacities as g_entity_s and
  // g_client_s.  This makes the AoS slabs safe even when a test harness or a
  // custom game interface supplies an out-of-range cvar value.
  const size_t configured_entities = sv_max_entities && sv_max_entities->integer > 0 ?
      (size_t) sv_max_entities->integer : 0;
  const size_t configured_clients = sv_max_clients && sv_max_clients->integer > 0 ?
      (size_t) sv_max_clients->integer : 0;

  g_mode_context = (g_mode_context_t) {
    .level = &g_level,
    .media = &g_media,
    .teams = g_team_list,
    .items = g_items,
    .entities = ge.entities,
    .clients = ge.clients,
    .max_entities = configured_entities < MAX_ENTITIES ? configured_entities : MAX_ENTITIES,
    .max_clients = configured_clients < MAX_CLIENTS ? configured_clients : MAX_CLIENTS,
  };
}

static const g_mode_def_t *G_ModeFind(const char *name) {
  if (name) {
    for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
      if (g_mode_defs[i] && !q_strcmp(g_mode_defs[i]->name, name)) {
        return g_mode_defs[i];
      }
    }
  }

  return G_DefaultModeDefinition();
}

static const g_mode_def_t *G_ModeFindExact(const char *name) {
  if (!name) {
    return NULL;
  }
  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    if (g_mode_defs[i] && !q_strcmp(g_mode_defs[i]->name, name)) {
      return g_mode_defs[i];
    }
  }
  return NULL;
}

static void G_ModeActivate(g_mode_t *mode, const g_mode_def_t *def) {
  *mode = (g_mode_t) {
    .def = def,
    .context = &g_mode_context,
    .entity_stride = G_ModeStride(def->entity_data_size),
    .client_stride = G_ModeStride(def->client_data_size),
    .generation = ++g_mode_generation,
  };

  size_t dynamic_items = 0;
  for (size_t i = 0; i < def->num_items; i++) {
    if (def->items[i].dynamic) {
      dynamic_items++;
    }
  }
  if (dynamic_items) {
    if ((size_t) g_mode_next_item_tag + dynamic_items > MAX_INVENTORY) {
      gi.Error("Mode '%s' registers too many inventory items\n", def->name);
    }
    mode->item_data = gi.Malloc(def->num_items * sizeof(g_item_t), MEM_TAG_GAME_LEVEL);
    memset(mode->item_data, 0, def->num_items * sizeof(g_item_t));
    mode->num_item_data = def->num_items;
    for (size_t i = 0; i < def->num_items; i++) {
      const g_mode_item_def_t *item = &def->items[i];
      if (!item->dynamic) {
        continue;
      }
      const g_item_t *prototype = item->Resolve ? item->Resolve(mode) : item->item;
      if (!prototype) {
        gi.Error("Mode '%s' item '%s' has no prototype\n", def->name, item->classname);
      }
      G_InitModeItem(&mode->item_data[i], prototype, g_mode_next_item_tag++);
      // Keep the familiar tag-indexed catalog usable by common code (ammo
      // resolution, weapon switching, and AI) while retaining ownership in
      // the active mode descriptor.
      g_items[mode->item_data[i].def.tag] = mode->item_data[i];
    }
    for (size_t i = 0; i < def->num_items; i++) {
      const g_mode_item_def_t *item = &def->items[i];
      if (!item->dynamic || !item->ResolveAmmo) {
        continue;
      }
      const g_item_t *ammo = item->ResolveAmmo(mode);
      if (ammo) {
        mode->item_data[i].def.ammo = ammo->def.tag;
      }
    }
  }

  if (def->state_size) {
    mode->state = gi.Malloc(def->state_size, MEM_TAG_GAME_LEVEL);
    memset(mode->state, 0, def->state_size);
  }
  if (mode->entity_stride) {
    const size_t size = mode->entity_stride * mode->context->max_entities;
    mode->entity_data = gi.Malloc(size, MEM_TAG_GAME_LEVEL);
    memset(mode->entity_data, 0, size);
  }
  if (mode->client_stride) {
    const size_t size = mode->client_stride * mode->context->max_clients;
    mode->client_data = gi.Malloc(size, MEM_TAG_GAME_LEVEL);
    memset(mode->client_data, 0, size);
  }
}

static void G_ModeRelease(g_mode_t *mode) {
  if (!mode->def) {
    return;
  }
  if (mode->def->ops && mode->def->ops->LevelEnd) {
    mode->def->ops->LevelEnd(mode);
  }
  if (mode->state) {
    gi.Free(mode->state);
  }
  if (mode->entity_data) {
    gi.Free(mode->entity_data);
  }
  if (mode->client_data) {
    gi.Free(mode->client_data);
  }
  if (mode->item_data && mode->context && mode->context->items) {
    for (size_t i = 0; i < mode->num_item_data; i++) {
      const g_item_tag_t tag = mode->item_data[i].def.tag;
      if (tag >= ITEM_TOTAL && tag < MAX_INVENTORY) {
        memset(&mode->context->items[tag], 0, sizeof(g_item_t));
      }
    }
  }
  if (mode->item_data) {
    gi.Free(mode->item_data);
  }
  memset(mode, 0, sizeof(*mode));
}

static const g_mode_entity_class_def_t *G_ModeFindEntityClass(const g_mode_t *mode,
                                                               const char *classname) {
  if (!mode || !mode->def || !mode->def->entity_classes || !classname) {
    return NULL;
  }
  for (size_t i = 0; i < mode->def->num_entity_classes; i++) {
    const g_mode_entity_class_def_t *clazz = &mode->def->entity_classes[i];
    if (!q_strcmp(clazz->classname, classname)) {
      return clazz;
    }
  }
  return NULL;
}

static void G_ModeValidateRegistry(void) {
  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    const g_mode_def_t *a = g_mode_defs[i];
    if (!a) {
      continue;
    }
    for (size_t j = i + 1; j < lengthof(g_mode_defs); j++) {
      const g_mode_def_t *b = g_mode_defs[j];
      if (b && !q_strcmp(a->name, b->name)) {
        gi.Error("Duplicate game mode '%s'\n", a->name);
      }
    }
    for (size_t j = 0; j < a->num_entity_classes; j++) {
      const char *classname = a->entity_classes[j].classname;
      for (size_t k = j + 1; k < a->num_entity_classes; k++) {
        if (!q_strcmp(classname, a->entity_classes[k].classname)) {
          gi.Error("Duplicate mode entity class '%s'\n", classname);
        }
      }
      for (size_t k = i + 1; k < lengthof(g_mode_defs); k++) {
        const g_mode_def_t *b = g_mode_defs[k];
        if (!b) {
          continue;
        }
        for (size_t l = 0; l < b->num_entity_classes; l++) {
          if (!q_strcmp(classname, b->entity_classes[l].classname)) {
            gi.Error("Duplicate mode entity class '%s'\n", classname);
          }
        }
      }
    }
    for (size_t j = 0; j < a->num_items; j++) {
      const char *classname = a->items[j].classname;
      if (!a->items[j].item && !a->items[j].Resolve) {
        gi.Error("Mode item class '%s' has no resolver\n", classname);
      }
      for (size_t k = j + 1; k < a->num_items; k++) {
        if (!q_strcmp(classname, a->items[k].classname)) {
          gi.Error("Duplicate mode item class '%s'\n", classname);
        }
      }
      for (size_t k = i + 1; k < lengthof(g_mode_defs); k++) {
        const g_mode_def_t *b = g_mode_defs[k];
        if (!b) {
          continue;
        }
        for (size_t l = 0; l < b->num_items; l++) {
          if (!q_strcmp(classname, b->items[l].classname)) {
            gi.Error("Duplicate mode item class '%s'\n", classname);
          }
        }
      }
    }
  }
}

void G_ModeInit(void) {
  if (g_mode_initialized) {
    return;
  }

  memset(&g_mode, 0, sizeof(g_mode));
  G_ModeRefreshContext();
  g_mode_defs[0] = G_DefaultModeDefinition();
  g_mode_defs[1] = G_DeathmatchModeDefinition();
  g_mode_defs[2] = G_InstagibModeDefinition();
  g_mode_defs[3] = G_ArenaModeDefinition();
#if G_MODE_ENABLE_CTF
  g_mode_defs[4] = G_CtfModeDefinition();
#else
  g_mode_defs[4] = NULL;
#endif
  g_mode_defs[5] = G_TechsModeDefinition();

  G_ModeValidateRegistry();

  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    const g_mode_def_t *def = g_mode_defs[i];
    if (!def || !def->cvars) {
      continue;
    }
    for (size_t j = 0; j < def->num_cvars; j++) {
      const g_mode_cvar_def_t *cvar = &def->cvars[j];
      cvar_t *registered = gi.AddCvar(cvar->name, cvar->default_value,
                                      cvar->flags, cvar->description);
      if (registered) {
        registered->modified = false;
      }
    }
  }

  g_mode_initialized = true;
}

void G_ModeShutdown(void) {
  G_ModeEndLevel();
  g_mode_initialized = false;
}

void G_ModeBeginLevel(const char *mode_name,
                      const char *const *modifier_names,
                      const size_t num_modifiers,
                      const char *map_name, const cm_entity_t *props) {
  G_ModeInit();
  G_ModeEndLevel();
  G_ModeRefreshContext();
  g_mode_next_item_tag = ITEM_TOTAL;

  G_ModeActivate(&g_mode, G_ModeFind(mode_name));

  const size_t modifiers = num_modifiers < G_MODE_MAX_MODIFIERS ?
      num_modifiers : G_MODE_MAX_MODIFIERS;
  for (size_t i = 0; i < modifiers; i++) {
    const g_mode_def_t *modifier = modifier_names ? G_ModeFindExact(modifier_names[i]) : NULL;
    if (modifier && modifier->kind == G_MODE_MODIFIER) {
      G_ModeActivate(&g_mode_modifiers[i], modifier);
    }
  }

  for (size_t i = 0; i < g_mode_context.max_entities; i++) {
    if (ge.entities[i]->in_use) {
      G_ModeEntitySpawn(ge.entities[i]);
    }
  }

  if (g_mode.def->ops && g_mode.def->ops->LevelBegin) {
    g_mode.def->ops->LevelBegin(&g_mode, map_name, props);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->LevelBegin) {
      modifier->def->ops->LevelBegin(modifier, map_name, props);
    }
  }
  G_ModePublishItemCatalog();
}

void G_ModeEndLevel(void) {
  gi.SetConfigString(CS_MODE_ITEMS, "");
  for (size_t i = G_MODE_MAX_MODIFIERS; i > 0; i--) {
    G_ModeRelease(&g_mode_modifiers[i - 1]);
  }
  G_ModeRelease(&g_mode);
}

void G_ModeFrame(void) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->Frame) {
    g_mode.def->ops->Frame(&g_mode);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->Frame) {
      modifier->def->ops->Frame(modifier);
    }
  }
}

void G_ModeResetItems(void) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ResetItems) {
    g_mode.def->ops->ResetItems(&g_mode);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ResetItems) {
      modifier->def->ops->ResetItems(modifier);
    }
  }
}

void G_ModeClientFrame(g_client_t *cl) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ClientFrame) {
    g_mode.def->ops->ClientFrame(&g_mode, cl);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ClientFrame) {
      modifier->def->ops->ClientFrame(modifier, cl);
    }
  }
}

void G_ModeClientBegin(g_client_t *cl) {
  if (g_mode.client_data && cl) {
    g_mode_client_t *data = G_ModeClientData(&g_mode, cl->ps.client);
    memset(data, 0, g_mode.client_stride);
    data->client = cl;
    data->generation = g_mode.generation;
  }

  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ClientBegin) {
    g_mode.def->ops->ClientBegin(&g_mode, cl);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->client_data && cl) {
      g_mode_client_t *data = G_ModeClientData(modifier, cl->ps.client);
      memset(data, 0, modifier->client_stride);
      data->client = cl;
      data->generation = modifier->generation;
    }
    if (modifier->def && modifier->def->ops && modifier->def->ops->ClientBegin) {
      modifier->def->ops->ClientBegin(modifier, cl);
    }
  }
}

void G_ModeClientDisconnect(g_client_t *cl) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ClientDisconnect) {
    g_mode.def->ops->ClientDisconnect(&g_mode, cl);
  }

  if (g_mode.client_data && cl) {
    memset(G_ModeClientData(&g_mode, cl->ps.client), 0, g_mode.client_stride);
  }
  for (size_t i = G_MODE_MAX_MODIFIERS; i > 0; i--) {
    g_mode_t *modifier = &g_mode_modifiers[i - 1];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ClientDisconnect) {
      modifier->def->ops->ClientDisconnect(modifier, cl);
    }
    if (modifier->client_data && cl) {
      memset(G_ModeClientData(modifier, cl->ps.client), 0, modifier->client_stride);
    }
  }
}

bool G_ModeClientInventory(g_client_t *cl, const g_item_t **starting_weapon) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ClientInventory &&
      g_mode.def->ops->ClientInventory(&g_mode, cl, starting_weapon)) {
    return true;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ClientInventory &&
        modifier->def->ops->ClientInventory(modifier, cl, starting_weapon)) {
      return true;
    }
  }
  return false;
}

void G_ModeEntitySpawn(g_entity_t *ent) {
  if (!ent) {
    return;
  }

  g_mode_t *mode = &g_mode;
  for (size_t i = 0; i <= G_MODE_MAX_MODIFIERS; i++) {
    if (i) mode = &g_mode_modifiers[i - 1];
    if (!mode->def || !mode->entity_data) {
      continue;
    }
    void *data = G_ModeEntityData(mode, ent->s.number);
    memset(data, 0, mode->entity_stride);
    ((g_mode_entity_t *) data)->entity = ent;
    ((g_mode_entity_t *) data)->spawn_id = ent->s.spawn_id;
    if (mode->def->ops && mode->def->ops->EntitySpawn) {
      mode->def->ops->EntitySpawn(mode, ent, data);
    }
  }
}

void G_ModeEntityFree(g_entity_t *ent) {
  if (!ent) {
    return;
  }

  for (size_t i = G_MODE_MAX_MODIFIERS; i > 0; i--) {
    g_mode_t *mode = &g_mode_modifiers[i - 1];
    if (!mode->def || !mode->entity_data) {
      continue;
    }
    void *data = G_ModeEntityData(mode, ent->s.number);
    const g_mode_entity_class_def_t *clazz = G_ModeFindEntityClass(mode, ent->classname);
    if (clazz && clazz->Destroy) {
      clazz->Destroy(mode, ent, data);
    }
    if (mode->def->ops && mode->def->ops->EntityFree) {
      mode->def->ops->EntityFree(mode, ent, data);
    }
    memset(data, 0, mode->entity_stride);
  }
  if (g_mode.def && g_mode.entity_data) {
    void *data = G_ModeEntityData(&g_mode, ent->s.number);
    const g_mode_entity_class_def_t *clazz = G_ModeFindEntityClass(&g_mode, ent->classname);
    if (clazz && clazz->Destroy) {
      clazz->Destroy(&g_mode, ent, data);
    }
    if (g_mode.def->ops && g_mode.def->ops->EntityFree) {
      g_mode.def->ops->EntityFree(&g_mode, ent, data);
    }
    memset(data, 0, g_mode.entity_stride);
  }
}

bool G_ModeSpawnEntityClass(g_entity_t *ent) {
  if (!ent || !ent->classname) {
    return false;
  }

  g_mode_t *mode = &g_mode;
  for (size_t i = 0; i <= G_MODE_MAX_MODIFIERS; i++) {
    if (i) mode = &g_mode_modifiers[i - 1];
    const g_mode_entity_class_def_t *clazz = G_ModeFindEntityClass(mode, ent->classname);
    if (!clazz) {
      continue;
    }
    void *data = mode->entity_data ? G_ModeEntityData(mode, ent->s.number) : NULL;
    if (clazz->Spawn) {
      clazz->Spawn(mode, ent, data);
    }
    return true;
  }
  return false;
}

const g_item_t *G_ModeFindItemByClassName(const char *classname) {
  if (!classname) {
    return NULL;
  }
  const g_mode_t *mode = &g_mode;
  for (size_t i = 0; i <= G_MODE_MAX_MODIFIERS; i++) {
    if (i) mode = &g_mode_modifiers[i - 1];
    if (!mode->def || !mode->def->items) {
      continue;
    }
    for (size_t j = 0; j < mode->def->num_items; j++) {
      const g_mode_item_def_t *item = &mode->def->items[j];
      if (!q_strcmp(item->classname, classname)) {
        if (item->dynamic) {
          if (!mode->item_data || !g_items) {
            return NULL;
          }
          return &g_items[mode->item_data[j].def.tag];
        }
        return item->Resolve ? item->Resolve((g_mode_t *) mode) : item->item;
      }
    }
  }
  return NULL;
}

const g_item_t *G_ModeItemByTag(const g_item_tag_t tag) {
  if (tag <= ITEM_NONE || tag >= MAX_INVENTORY || !g_items) {
    return NULL;
  }

  const g_item_t *item = &g_items[tag];
  return item->def.tag == tag && item->def.type != ITEM_TYPE_NONE ? item : NULL;
}

size_t G_ModeItemCount(const g_item_type_t type) {
  size_t count = 0;

  for (g_item_tag_t tag = ITEM_FIRST; tag < MAX_INVENTORY; tag++) {
    const g_item_t *item = G_ModeItemByTag(tag);
    if (item && (type == ITEM_TYPE_NONE || item->def.type == type)) {
      count++;
    }
  }

  return count;
}

const g_item_t *G_ModeItemAt(const g_item_type_t type, const size_t index) {
  size_t current = 0;

  for (g_item_tag_t tag = ITEM_FIRST; tag < MAX_INVENTORY; tag++) {
    const g_item_t *item = G_ModeItemByTag(tag);
    if (!item || (type != ITEM_TYPE_NONE && item->def.type != type)) {
      continue;
    }
    if (current++ == index) {
      return item;
    }
  }

  return NULL;
}

const g_item_t *G_ModeFindItem(const char *name) {
  if (!name) {
    return NULL;
  }
  const g_mode_t *mode = &g_mode;
  for (size_t i = 0; i <= G_MODE_MAX_MODIFIERS; i++) {
    if (i) mode = &g_mode_modifiers[i - 1];
    if (!mode->def || !mode->def->items) {
      continue;
    }
    for (size_t j = 0; j < mode->def->num_items; j++) {
      const g_mode_item_def_t *descriptor = &mode->def->items[j];
      const g_item_t *item = descriptor->dynamic ?
          (mode->item_data && g_items ? &g_items[mode->item_data[j].def.tag] : NULL) :
          (descriptor->Resolve ? descriptor->Resolve((g_mode_t *) mode) : descriptor->item);
      if (item && item->def.name && !q_strcasecmp(item->def.name, name)) {
        return item;
      }
    }
  }
  return NULL;
}

g_mode_t *G_ModeActive(void) {
  return g_mode.def ? &g_mode : NULL;
}

g_mode_t *G_ModeModifier(const char *name) {
  if (!name) {
    return NULL;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    if (g_mode_modifiers[i].def && !q_strcmp(g_mode_modifiers[i].def->name, name)) {
      return &g_mode_modifiers[i];
    }
  }
  return NULL;
}

static bool G_ModeOwnsItem(g_mode_t *mode, const g_item_t *item) {
  if (!mode || !mode->def || !mode->def->items || !item) {
    return false;
  }

  for (size_t i = 0; i < mode->def->num_items; i++) {
    const g_mode_item_def_t *descriptor = &mode->def->items[i];
    if (!descriptor->dynamic) {
      continue;
    }
    if (mode->item_data &&
        mode->item_data[i].def.tag == item->def.tag) {
      return true;
    }
  }

  return false;
}

g_mode_t *G_ModeForItem(const g_item_t *item) {
  if (G_ModeOwnsItem(&g_mode, item)) {
    return &g_mode;
  }

  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    if (G_ModeOwnsItem(&g_mode_modifiers[i], item)) {
      return &g_mode_modifiers[i];
    }
  }

  return NULL;
}

const g_mode_context_t *G_ModeContext(const g_mode_t *mode) {
  return mode ? mode->context : NULL;
}

void *G_ModeState(g_mode_t *mode) {
  return mode ? mode->state : NULL;
}

void *G_ModeEntityData(g_mode_t *mode, const int32_t entity_num) {
  assert(mode);
  assert(mode->entity_data);
  assert(entity_num >= 0 && (size_t) entity_num < mode->context->max_entities);
  return (uint8_t *) mode->entity_data + mode->entity_stride * (size_t) entity_num;
}

void *G_ModeClientData(g_mode_t *mode, const int32_t client_num) {
  assert(mode);
  assert(mode->client_data);
  assert(client_num >= 0 && (size_t) client_num < mode->context->max_clients);
  return (uint8_t *) mode->client_data + mode->client_stride * (size_t) client_num;
}

bool G_ModeItemPickup(g_client_t *cl, g_entity_t *ent) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ItemPickup) {
    if (g_mode.def->ops->ItemPickup(&g_mode, cl, ent)) {
      return true;
    }
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ItemPickup) {
      return modifier->def->ops->ItemPickup(modifier, cl, ent);
    }
  }
  return false;
}

g_entity_t *G_ModeItemDrop(g_client_t *cl) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ItemDrop) {
    g_entity_t *drop = g_mode.def->ops->ItemDrop(&g_mode, cl);
    if (drop) {
      return drop;
    }
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ItemDrop) {
      return modifier->def->ops->ItemDrop(modifier, cl);
    }
  }
  return NULL;
}

g_entity_t *G_ModeItemDropCallback(g_client_t *cl, const g_item_t *item) {
  (void) item;
  return G_ModeItemDrop(cl);
}

void G_ModeItemResetDropped(g_entity_t *ent) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ItemResetDropped) {
    g_mode.def->ops->ItemResetDropped(&g_mode, ent);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ItemResetDropped) {
      modifier->def->ops->ItemResetDropped(modifier, ent);
    }
  }
}

bool G_ModeItemReset(g_entity_t *ent) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ItemReset) {
    if (g_mode.def->ops->ItemReset(&g_mode, ent)) {
      return true;
    }
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ItemReset &&
        modifier->def->ops->ItemReset(modifier, ent)) {
      return true;
    }
  }
  return false;
}

g_team_t *G_ModeTeamForFlag(const g_entity_t *ent) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->TeamForFlag) {
    g_team_t *team = g_mode.def->ops->TeamForFlag(&g_mode, ent);
    if (team) return team;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->TeamForFlag) {
      g_team_t *team = modifier->def->ops->TeamForFlag(modifier, ent);
      if (team) return team;
    }
  }
  return NULL;
}

g_entity_t *G_ModeFlagForTeam(const g_team_t *team) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->FlagForTeam) {
    g_entity_t *flag = g_mode.def->ops->FlagForTeam(&g_mode, team);
    if (flag) return flag;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->FlagForTeam) {
      g_entity_t *flag = modifier->def->ops->FlagForTeam(modifier, team);
      if (flag) return flag;
    }
  }
  return NULL;
}

const g_item_t *G_ModeGetFlag(const g_client_t *cl) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->GetFlag) {
    const g_item_t *flag = g_mode.def->ops->GetFlag(&g_mode, cl);
    if (flag) return flag;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->GetFlag) {
      const g_item_t *flag = modifier->def->ops->GetFlag(modifier, cl);
      if (flag) return flag;
    }
  }
  return NULL;
}

int32_t G_ModeEffectForTeam(const g_team_t *team) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->EffectForTeam) {
    const int32_t effect = g_mode.def->ops->EffectForTeam(&g_mode, team);
    if (effect) return effect;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->EffectForTeam) {
      return modifier->def->ops->EffectForTeam(modifier, team);
    }
  }
  return team ? team->effect : 0;
}

void G_ModeBotDirectives(g_client_t *cl, const g_entity_t *ent,
                         g_mode_bot_directives_t *directives) {
  if (!directives) {
    return;
  }
  if (directives->item_weight <= 0.f) {
    directives->item_weight = 1.f;
  }

  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->BotDirectives) {
    g_mode.def->ops->BotDirectives(&g_mode, cl, ent, directives);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->BotDirectives) {
      modifier->def->ops->BotDirectives(modifier, cl, ent, directives);
    }
  }
}

void G_ModeBotTarget(const g_client_t *cl, const g_entity_t *target,
                     float *priority, float *chase_multiplier) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->BotTarget) {
    g_mode.def->ops->BotTarget(&g_mode, cl, target, priority, chase_multiplier);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->BotTarget) {
      modifier->def->ops->BotTarget(modifier, cl, target, priority, chase_multiplier);
    }
  }
}

bool G_ModeBotCanPickup(const g_client_t *cl, const g_entity_t *item,
                        bool *can_pickup) {
  if (!can_pickup) {
    return false;
  }
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->BotCanPickup &&
      g_mode.def->ops->BotCanPickup(&g_mode, cl, item, can_pickup)) {
    return true;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->BotCanPickup &&
        modifier->def->ops->BotCanPickup(modifier, cl, item, can_pickup)) {
      return true;
    }
  }
  return false;
}

void G_ModeModifyDamage(g_damage_t *damage, bool *cancel) {
  if (!damage || !cancel) {
    return;
  }
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ModifyDamage) {
    g_mode.def->ops->ModifyDamage(&g_mode, damage, cancel);
  }
  for (size_t i = 0; !*cancel && i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ModifyDamage) {
      modifier->def->ops->ModifyDamage(modifier, damage, cancel);
    }
  }
}

uint32_t G_ModeModifyWeaponInterval(g_client_t *cl, uint32_t interval) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->ModifyWeaponInterval) {
    interval = g_mode.def->ops->ModifyWeaponInterval(&g_mode, cl, interval);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->ModifyWeaponInterval) {
      interval = modifier->def->ops->ModifyWeaponInterval(modifier, cl, interval);
    }
  }
  return interval;
}

void G_ModeDamageApplied(const g_damage_t *damage, const int32_t damage_health,
                         const bool was_dead) {
  if (!damage) {
    return;
  }
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->DamageApplied) {
    g_mode.def->ops->DamageApplied(&g_mode, damage, damage_health, was_dead);
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->DamageApplied) {
      modifier->def->ops->DamageApplied(modifier, damage, damage_health, was_dead);
    }
  }
}

bool G_ModeRespawn(g_client_t *cl, const bool voluntary) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->Respawn &&
      g_mode.def->ops->Respawn(&g_mode, cl, voluntary)) {
    return true;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->Respawn &&
        modifier->def->ops->Respawn(modifier, cl, voluntary)) {
      return true;
    }
  }
  return false;
}

g_entity_t *G_ModeSelectSpawn(g_client_t *cl) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->SelectSpawn) {
    g_entity_t *spawn = g_mode.def->ops->SelectSpawn(&g_mode, cl);
    if (spawn) {
      return spawn;
    }
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->SelectSpawn) {
      g_entity_t *spawn = modifier->def->ops->SelectSpawn(modifier, cl);
      if (spawn) {
        return spawn;
      }
    }
  }
  return NULL;
}

bool G_ModeAssignTeam(g_client_t *cl) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->AssignTeam &&
      g_mode.def->ops->AssignTeam(&g_mode, cl)) {
    return true;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->AssignTeam &&
        modifier->def->ops->AssignTeam(modifier, cl)) {
      return true;
    }
  }
  return false;
}

bool G_ModeCheckRules(void) {
  if (g_mode.def && g_mode.def->ops && g_mode.def->ops->CheckRules &&
      g_mode.def->ops->CheckRules(&g_mode)) {
    return true;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    g_mode_t *modifier = &g_mode_modifiers[i];
    if (modifier->def && modifier->def->ops && modifier->def->ops->CheckRules &&
        modifier->def->ops->CheckRules(modifier)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Publish dynamic mode items to the client game.
 *
 * Built-in items remain described by `bg_item_defs`. Only dynamic descriptors
 * are transmitted here, keeping the catalog compact while giving a mode a
 * stable presentation record for every runtime inventory tag it owns.
 * The payload is an info string keyed by `n<tag>`, `h<tag>`, `i<tag>`,
 * `m<tag>`, `y<tag>`, `q<tag>`, `a<tag>`, and `c<tag>` for name, classname,
 * icon, model, item type, quantity, ammo tag, and effect color respectively.
 */
void G_ModePublishItemCatalog(void) {
  char catalog[MAX_INFO_STRING_STRING] = { 0 };
  const g_mode_t *mode = &g_mode;
  for (size_t i = 0; i <= G_MODE_MAX_MODIFIERS; i++) {
    if (i) mode = &g_mode_modifiers[i - 1];
    if (!mode->def || !mode->item_data || !mode->context || !mode->context->items) {
      continue;
    }

    for (size_t j = 0; j < mode->def->num_items; j++) {
      const g_mode_item_def_t *descriptor = &mode->def->items[j];
      if (!descriptor->dynamic) {
        continue;
      }

      const g_item_t *item = &mode->context->items[mode->item_data[j].def.tag];
      const int32_t tag = item->def.tag;
      char key[16];
      char value[16];

      q_snprintf(key, sizeof(key), "n%d", tag);
      bool valid = InfoString_Set(catalog, key, item->def.name ?: "");
      q_snprintf(key, sizeof(key), "h%d", tag);
      valid = InfoString_Set(catalog, key, item->def.classname ?: "") && valid;
      q_snprintf(key, sizeof(key), "i%d", tag);
      valid = InfoString_Set(catalog, key, item->def.icon ?: "") && valid;
      q_snprintf(key, sizeof(key), "m%d", tag);
      valid = InfoString_Set(catalog, key, item->def.model ?: "") && valid;
      q_snprintf(key, sizeof(key), "y%d", tag);
      q_snprintf(value, sizeof(value), "%d", item->def.type);
      valid = InfoString_Set(catalog, key, value) && valid;
      q_snprintf(key, sizeof(key), "q%d", tag);
      q_snprintf(value, sizeof(value), "%d", item->def.quantity);
      valid = InfoString_Set(catalog, key, value) && valid;
      q_snprintf(key, sizeof(key), "a%d", tag);
      q_snprintf(value, sizeof(value), "%d", item->def.ammo);
      valid = InfoString_Set(catalog, key, value) && valid;
      q_snprintf(key, sizeof(key), "c%d", tag);
      valid = InfoString_Set(catalog, key, Color_Unparse(item->def.effect_color)) && valid;
      if (!valid) {
        gi.Error("Dynamic item catalog is too large for mode item %d\n", tag);
      }
    }
  }

  gi.SetConfigString(CS_MODE_ITEMS, catalog);
}

bool G_ModeHasCapability(const uint32_t capability) {
  uint32_t capabilities = 0;

  if (g_mode.def) {
    capabilities |= g_mode.def->capabilities;
  }
  for (size_t i = 0; i < G_MODE_MAX_MODIFIERS; i++) {
    if (g_mode_modifiers[i].def) {
      capabilities |= g_mode_modifiers[i].def->capabilities;
    }
  }

  // Keep the query useful during worldspawn resolution, before the active
  // mode is composed, and for legacy team configuration until its mode
  // descriptor is migrated.
  if (g_level.ctf) {
    capabilities |= G_MODE_CAP_TEAMPLAY | G_MODE_CAP_FLAG_OBJECTIVE;
  }
  if (g_level.teams) {
    capabilities |= G_MODE_CAP_TEAMPLAY;
  }
  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    const g_mode_def_t *def = g_mode_defs[i];
    if (def && def->kind == G_MODE_MODIFIER && def->gameplay == g_level.gameplay) {
      capabilities |= def->capabilities;
    }
  }

  return (capabilities & capability) == capability;
}

bool G_ModeTeamplay(void) {
  return G_ModeHasCapability(G_MODE_CAP_TEAMPLAY);
}

static const g_mode_def_t *G_ModeObjectiveDefinition(void) {
  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    const g_mode_def_t *def = g_mode_defs[i];
    if (def && def->kind == G_MODE_PRIMARY &&
        (def->capabilities & G_MODE_CAP_FLAG_OBJECTIVE)) {
      return def;
    }
  }
  return NULL;
}

bool G_ModeObjectiveEnabled(void) {
  const g_mode_def_t *def = G_ModeObjectiveDefinition();
  if (!def || !def->objective_cvar) {
    return false;
  }
  const cvar_t *cvar = gi.GetCvar(def->objective_cvar);
  return cvar && cvar->integer > 0;
}

bool G_ModeObjectiveCvarChanged(int32_t *enabled) {
  const g_mode_def_t *def = G_ModeObjectiveDefinition();
  if (!def || !def->objective_cvar) {
    return false;
  }
  cvar_t *cvar = gi.GetCvar(def->objective_cvar);
  if (!cvar || !cvar->modified) {
    return false;
  }
  cvar->modified = false;
  if (enabled) {
    *enabled = cvar->integer;
  }
  return true;
}

bool G_ModeCaptureLimitCvarChanged(int32_t *limit) {
  const g_mode_def_t *def = G_ModeObjectiveDefinition();
  if (!def || !def->capture_limit_cvar) {
    return false;
  }
  cvar_t *cvar = gi.GetCvar(def->capture_limit_cvar);
  if (!cvar || !cvar->modified) {
    return false;
  }
  cvar->modified = false;
  if (limit) {
    *limit = cvar->integer;
  }
  return true;
}

int32_t G_ModeCaptureLimit(void) {
  const g_mode_def_t *def = G_ModeObjectiveDefinition();
  if (!def || !def->capture_limit_cvar) {
    return 0;
  }
  const cvar_t *cvar = gi.GetCvar(def->capture_limit_cvar);
  return cvar ? cvar->integer : 0;
}

const char *G_ModePrimaryName(const bool objective) {
  if (objective) {
    for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
      const g_mode_def_t *def = g_mode_defs[i];
      if (def && def->kind == G_MODE_PRIMARY &&
          (def->capabilities & G_MODE_CAP_FLAG_OBJECTIVE)) {
        return def->name;
      }
    }
  }
  return G_DeathmatchModeDefinition()->name;
}

const char *G_ModeModifierName(const g_gameplay_t gameplay) {
  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    const g_mode_def_t *def = g_mode_defs[i];
    if (def && def->kind == G_MODE_MODIFIER && def->gameplay_selector &&
        def->gameplay == gameplay) {
      return def->name;
    }
  }
  return NULL;
}

g_gameplay_t G_ModeGameplayByName(const char *name) {
  if (!name || !*name) {
    return GAME_DEATHMATCH;
  }

  char lower[64];
  q_strlcpy(lower, name, sizeof(lower));
  for (char *p = lower; *p; p++) {
    *p = (char) tolower((unsigned char) *p);
  }

  for (size_t i = 0; i < lengthof(g_mode_defs); i++) {
    const g_mode_def_t *def = g_mode_defs[i];
    if (!def || def->kind != G_MODE_MODIFIER || !def->gameplay_selector ||
        !def->name) {
      continue;
    }
    if (!q_strncmp(lower, def->name, q_strlen(def->name)) ||
        !q_strncmp(def->name, lower, q_strlen(lower))) {
      return def->gameplay;
    }
  }
  return GAME_DEATHMATCH;
}
