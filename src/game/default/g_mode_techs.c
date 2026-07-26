/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 */

#include "g_local.h"

/*
 * Techs are a reusable modifier, not a CTF-owned feature.  This translation
 * unit owns the enablement policy, tech item lifecycle, spawn placement,
 * callbacks, media, and compatibility services used by older common paths.
 * Mutable state is obtained from the active modifier context; this file has
 * no mutable mode singleton.
 */
#define TECH_HASTE_FACTOR 0.75f
#define TECH_REGEN_TICK_TIME 500
#define TECH_REGEN_HEALTH 1
#define TECH_RESIST_DAMAGE_FACTOR 0.5f
#define TECH_RESIST_KNOCKBACK_FACTOR 0.75f
#define TECH_STRENGTH_DAMAGE_FACTOR 1.5f
#define TECH_STRENGTH_KNOCKBACK_FACTOR 1.25f
#define TECH_VAMPIRE_DAMAGE_FACTOR 0.25f

typedef struct {
  g_mode_client_t base;
  uint32_t regen_time;
  uint32_t sound_time;
} g_techs_client_t;

typedef struct {
  uint16_t sounds[TECH_TOTAL];
} g_techs_state_t;

_Static_assert(offsetof(g_techs_client_t, base) == 0,
               "tech client data must begin with g_mode_client_t");

static const g_mode_cvar_def_t g_techs_cvars[] = {
  {
    .name = "g_techs",
    .default_value = "default",
    .flags = CVAR_SERVER_INFO,
    .description = "Whether to allow techs or not. \"default\" only allows techs in CTF; 1 is always allow, 0 is never allow.",
  },
};

static g_mode_t *G_TechMode(void) {
  return G_ModeModifier("techs");
}

static const g_mode_context_t *G_TechContext(void) {
  const g_mode_t *mode = G_TechMode();
  return G_ModeContext(mode);
}

static g_techs_client_t *G_TechClientData(g_mode_t *mode, g_client_t *cl) {
  if (!mode || !cl || !mode->client_data) {
    return NULL;
  }
  g_techs_client_t *data = G_ModeClientData(mode, cl->ps.client);
  return data->base.client == cl ? data : NULL;
}

static const g_item_t *G_TechItem(g_mode_t *mode, const g_item_tag_t tag) {
  const g_mode_context_t *context = G_ModeContext(mode);
  return context && context->items ? &context->items[tag] : NULL;
}

static const g_item_t *G_TechHasteItem(g_mode_t *mode) {
  return G_TechItem(mode, TECH_HASTE);
}

static const g_item_t *G_TechRegenItem(g_mode_t *mode) {
  return G_TechItem(mode, TECH_REGEN);
}

static const g_item_t *G_TechResistItem(g_mode_t *mode) {
  return G_TechItem(mode, TECH_RESIST);
}

static const g_item_t *G_TechStrengthItem(g_mode_t *mode) {
  return G_TechItem(mode, TECH_STRENGTH);
}

static const g_item_t *G_TechVampireItem(g_mode_t *mode) {
  return G_TechItem(mode, TECH_VAMPIRE);
}

static const g_mode_item_def_t g_techs_items[] = {
  { .classname = "item_tech_haste", .Resolve = G_TechHasteItem },
  { .classname = "item_tech_regen", .Resolve = G_TechRegenItem },
  { .classname = "item_tech_resist", .Resolve = G_TechResistItem },
  { .classname = "item_tech_strength", .Resolve = G_TechStrengthItem },
  { .classname = "item_tech_vampire", .Resolve = G_TechVampireItem },
};

cvar_t *G_ModeTechsCvar(void) {
  return gi.GetCvar("g_techs");
}

bool G_ModeTechsEnabled(void) {
  const g_mode_context_t *context = G_TechContext();
  return context && context->level && context->level->techs;
}

bool G_ModeResolveTechs(g_level_t *level, const cm_entity_t *world) {
  if (!level) {
    return false;
  }

  const cvar_t *techs_cvar = G_ModeTechsCvar();
  if (techs_cvar && q_strcmp(techs_cvar->string, "default")) {
    level->techs = !!techs_cvar->integer;
  } else if (level->techs_map != -1) {
    level->techs = !!level->techs_map;
  } else if (world) {
    const cm_entity_t *techs = gi.EntityValue(world, "techs");
    level->techs = (techs->parsed & ENTITY_INTEGER) ? techs->integer :
        G_ModeHasCapability(G_MODE_CAP_FLAG_OBJECTIVE);
  } else {
    level->techs = G_ModeHasCapability(G_MODE_CAP_FLAG_OBJECTIVE);
  }

  return level->techs;
}

static float G_TechRangeFromSpawn(const g_mode_context_t *context,
                                  const g_entity_t *spawn) {
  float best_dist = FLT_MAX;
  bool any = false;

  if (!context || !spawn || !context->entities || !context->items) {
    return 0.f;
  }

  for (g_item_tag_t tech = TECH_FIRST; tech < TECH_LAST; tech++) {
    const g_item_t *item = &context->items[tech];
    for (size_t i = 0; i < context->max_entities; i++) {
      g_entity_t *ent = context->entities[i];
      if (!ent || !ent->in_use || ent->item != item) {
        continue;
      }

      const vec3_t v = Vec3_Subtract(spawn->s.origin, ent->s.origin);
      const float dist = Vec3_Length(v);
      if (dist < best_dist) {
        best_dist = dist;
      }
      any = true;
      break;
    }
  }

  return any ? best_dist : Randomf() * MAX_WORLD_DIST;
}

static void G_SelectFarthestTechSpawnPoint(const g_mode_context_t *context,
                                           const g_spawn_points_t *spawn_points,
                                           g_entity_t **point, float *point_dist) {
  if (!context || !spawn_points || !point || !point_dist) {
    return;
  }

  for (size_t i = 0; i < spawn_points->count; i++) {
    g_entity_t *spot = spawn_points->spots[i];
    const float dist = G_TechRangeFromSpawn(context, spot);
    if (dist > *point_dist) {
      *point = spot;
      *point_dist = dist;
    }
  }
}

g_entity_t *G_ModeSelectTechSpawnPoint(void) {
  const g_mode_context_t *context = G_TechContext();
  if (!context || !context->level) {
    return NULL;
  }

  float point_dist = -FLT_MAX;
  g_entity_t *point = NULL;
  if (G_ModeTeamplay()) {
    for (int32_t i = 0; i < context->level->num_teams; i++) {
      G_SelectFarthestTechSpawnPoint(context, &context->teams[i].spawn_points,
                                     &point, &point_dist);
    }
  } else {
    G_SelectFarthestTechSpawnPoint(context, &context->level->spawn_points,
                                   &point, &point_dist);
  }

  if (!point) {
    G_SelectFarthestTechSpawnPoint(context, &context->level->spawn_points,
                                   &point, &point_dist);
  }
  return point;
}

void G_SpawnTech(const g_item_t *item) {
  const g_mode_context_t *context = G_TechContext();
  g_entity_t *spawn = G_ModeSelectTechSpawnPoint();
  if (!context || !context->level || !item || !spawn) {
    return;
  }

  g_entity_t *ent = G_AllocEntity(item->def.classname);
  ent->s.origin = spawn->s.origin;
  G_SpawnItem(ent, item);
  ent->next_think = 0;
  ent->Think = NULL;
  ent->spawn_flags |= SF_ITEM_DROPPED;
  ent->move_type = MOVE_TYPE_BOUNCE;
  ent->touch_time = context->level->time + 1000;

  vec3_t angles = spawn->s.angles;
  angles.y += RandomRangef(-45.f, 45.f);
  vec3_t forward;
  Vec3_Vectors(angles, &forward, NULL, NULL);
  ent->velocity = Vec3_Scale(forward, 100.f);
  ent->velocity.z = 300.f + (Randomf() * 50.f);
  G_ResetItem(ent);
}

static void G_ModeTechsResetItems(g_mode_t *mode) {
  const g_mode_context_t *context = G_ModeContext(mode);
  if (!context || !context->level || !context->items || !context->level->techs) {
    return;
  }
  for (g_item_tag_t i = TECH_FIRST; i < TECH_LAST; i++) {
    G_SpawnTech(&context->items[i]);
  }
}

void G_ModeResetDroppedTech(g_entity_t *ent) {
  if (!ent || !ent->item) {
    return;
  }
  G_SpawnTech(ent->item);
  G_FreeEntity(ent);
}

bool G_ModeHasTech(const g_client_t *cl, const g_item_tag_t tech) {
  return cl && tech > ITEM_NONE && tech < MAX_INVENTORY && !!cl->inventory[tech];
}

const g_item_t *G_ModeGetTech(const g_client_t *cl) {
  const g_mode_context_t *context = G_TechContext();
  if (!cl || !context || !context->items) {
    return NULL;
  }
  for (g_item_tag_t i = TECH_FIRST; i < TECH_LAST; i++) {
    if (G_ModeHasTech(cl, i)) {
      return &context->items[i];
    }
  }
  return NULL;
}

bool G_ModePickupTech(g_client_t *cl, g_entity_t *ent) {
  if (!cl || !ent || !ent->item) {
    return false;
  }
  for (g_item_tag_t tech = TECH_FIRST; tech < TECH_LAST; tech++) {
    if (G_ModeHasTech(cl, tech)) {
      return false;
    }
  }
  cl->inventory[ent->item->def.tag]++;
  return true;
}

g_entity_t *G_ModeTossTech(g_client_t *cl) {
  const g_item_t *tech = G_ModeGetTech(cl);
  if (!tech) {
    return NULL;
  }
  cl->inventory[tech->def.tag] = 0;
  return G_DropItem(cl, tech);
}

g_entity_t *G_ModeDropTech(g_client_t *cl, const g_item_t *item) {
  (void) item;
  return G_ModeTossTech(cl);
}

void G_ModePlayTechSound(g_client_t *cl) {
  g_mode_t *mode = G_TechMode();
  const g_mode_context_t *context = G_TechContext();
  g_techs_client_t *data = G_TechClientData(mode, cl);
  const g_techs_state_t *state = G_ModeState(mode);
  const g_item_t *tech = G_ModeGetTech(cl);
  if (!context || !context->level || !state || !data || !tech ||
      tech->def.tag < TECH_FIRST || tech->def.tag >= TECH_LAST) {
    return;
  }
  if (data->sound_time < context->level->time) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = state->sounds[tech->def.tag - TECH_FIRST],
      .entity = cl->entity,
    }, MULTICAST_PHS);
    data->sound_time = context->level->time + 500;
  }
}

static void G_ModeTechsClientFrame(g_mode_t *mode, g_client_t *cl) {
  const g_mode_context_t *context = G_ModeContext(mode);
  g_techs_client_t *data = G_TechClientData(mode, cl);
  if (!context || !context->level || !cl || !cl->entity || cl->entity->dead ||
      !data || !G_ModeHasTech(cl, TECH_REGEN)) {
    return;
  }
  if (data->regen_time < context->level->time) {
    data->regen_time = context->level->time + TECH_REGEN_TICK_TIME;
    if (cl->entity->health < cl->entity->max_health) {
      cl->entity->health = Minf(cl->entity->health + TECH_REGEN_HEALTH,
                                cl->entity->max_health);
      G_ModePlayTechSound(cl);
    }
  }
}

static void G_ModeTechsModifyDamage(g_mode_t *mode, g_damage_t *damage,
                                    bool *cancel) {
  (void) mode;
  (void) cancel;
  if (!damage) {
    return;
  }
  if (damage->target && damage->target->client &&
      G_ModeHasTech(damage->target->client, TECH_RESIST)) {
    damage->damage *= TECH_RESIST_DAMAGE_FACTOR;
    damage->knockback *= TECH_RESIST_KNOCKBACK_FACTOR;
    G_ModePlayTechSound(damage->target->client);
  }
  if (damage->attacker && damage->attacker->client &&
      G_ModeHasTech(damage->attacker->client, TECH_STRENGTH)) {
    damage->damage *= TECH_STRENGTH_DAMAGE_FACTOR;
    damage->knockback *= TECH_STRENGTH_KNOCKBACK_FACTOR;
    G_ModePlayTechSound(damage->attacker->client);
  }
}

static uint32_t G_ModeTechsModifyWeaponInterval(g_mode_t *mode,
                                                g_client_t *cl,
                                                const uint32_t interval) {
  (void) mode;
  if (!cl) {
    return interval;
  }
  uint32_t modified = interval;
  if (G_ModeHasTech(cl, TECH_HASTE)) {
    modified = (uint32_t) (modified * TECH_HASTE_FACTOR);
    G_ModePlayTechSound(cl);
  } else if (G_ModeHasTech(cl, TECH_STRENGTH)) {
    G_ModePlayTechSound(cl);
  }
  return modified;
}

static void G_ModeTechsDamageApplied(g_mode_t *mode, const g_damage_t *damage,
                                     const int32_t damage_health,
                                     const bool was_dead) {
  (void) mode;
  if (!damage || damage_health <= 0 || was_dead || !damage->attacker ||
      !damage->attacker->client || !damage->target ||
      damage->target == damage->attacker ||
      G_OnSameTeam(damage->attacker->client, damage->target->client)) {
    return;
  }
  if (G_ModeHasTech(damage->attacker->client, TECH_VAMPIRE)) {
    damage->attacker->health = Minf(
        damage->attacker->health + damage->damage * TECH_VAMPIRE_DAMAGE_FACTOR,
        damage->attacker->max_health);
    G_ModePlayTechSound(damage->attacker->client);
  }
}

static void G_ModeTechsLevelBegin(g_mode_t *mode, const char *map_name,
                                  const cm_entity_t *props) {
  (void) map_name;
  (void) props;
  g_techs_state_t *state = G_ModeState(mode);
  if (!state) {
    return;
  }
  state->sounds[TECH_HASTE - TECH_FIRST] = gi.SoundIndex("techs/haste/haste");
  state->sounds[TECH_REGEN - TECH_FIRST] = gi.SoundIndex("techs/regen/regen");
  state->sounds[TECH_RESIST - TECH_FIRST] = gi.SoundIndex("techs/resist/resist");
  state->sounds[TECH_STRENGTH - TECH_FIRST] = gi.SoundIndex("techs/strength/strength");
  state->sounds[TECH_VAMPIRE - TECH_FIRST] = gi.SoundIndex("techs/vampire/vampire");
}

static const g_mode_ops_t g_techs_ops = {
  .LevelBegin = G_ModeTechsLevelBegin,
  .ResetItems = G_ModeTechsResetItems,
  .ClientFrame = G_ModeTechsClientFrame,
  .ModifyDamage = G_ModeTechsModifyDamage,
  .ModifyWeaponInterval = G_ModeTechsModifyWeaponInterval,
  .DamageApplied = G_ModeTechsDamageApplied,
};

static const g_mode_def_t g_techs_mode = {
  .name = "techs",
  .kind = G_MODE_MODIFIER,
  .state_size = sizeof(g_techs_state_t),
  .client_data_size = sizeof(g_techs_client_t),
  .cvars = g_techs_cvars,
  .num_cvars = lengthof(g_techs_cvars),
  .items = g_techs_items,
  .num_items = lengthof(g_techs_items),
  .ops = &g_techs_ops,
};

const g_mode_def_t *G_TechsModeDefinition(void) {
  return &g_techs_mode;
}
