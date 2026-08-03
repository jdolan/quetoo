/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "g_local.h"

/**
 * @brief The techs own their own configuration, media and enabled state so that
 * a module adopting them needs only `g_client_tech_t`, the ITEM_TYPE_TECH item
 * type, the TECH_FIRST..TECH_LAST tags with their item definitions, and the
 * STAT_TECH wire value.
 */

cvar_t *g_techs;

/**
 * @brief `g_module_t` function pointers.
 */
static struct {
  ResetDroppedItem ResetDroppedItem;
  DropInventoryItem DropInventoryItem;
  ModifyDamage ModifyDamage;
  CheckCvars CheckCvars;
  TossInventory TossInventory;
  InitItem InitItem;
  InitMedia InitMedia;
  ConfigureLevel ConfigureLevel;
} super;

static bool installed;

static struct {
  uint16_t sounds[TECH_TOTAL];
} g_tech_media;


/**
 * @brief True when techs are available this level.
 */
static bool g_tech_enabled;

/**
 * @return True if techs are enabled for this level.
 */
bool G_Tech_Enabled(void) {
  return g_tech_enabled;
}

/**
 * @brief Respawns a dropped tech, deferring anything else.
 */
static void G_ResetDroppedItem_Tech(g_entity_t *ent) {

  if (ent->item->def.type == ITEM_TYPE_TECH) {
    G_ResetDroppedTech(ent);
    return;
  }

  super.ResetDroppedItem(ent);
}

/**
 * @brief Resolves "tech" to whichever tech the client is carrying.
 */
static void G_DropInventoryItem_Tech(g_client_t *cl, const char *name) {

  if (!q_strcasecmp(name, "tech")) {
    const g_item_t *tech = G_GetTech(cl);
    if (tech) {
      name = tech->def.name;
    }
  }

  super.DropInventoryItem(cl, name);
}

/**
 * @brief Applies the resist and strength modifiers, deferring the powerups to
 * super so that the three keep the order they had before resist and strength
 * were a hook.
 */
static void G_ModifyDamage_Tech(g_entity_t *target, g_entity_t *attacker, int32_t *damage, int32_t *knockback) {

  if (target->client && G_HasTech(target->client, TECH_RESIST)) {
    *damage *= TECH_RESIST_DAMAGE_FACTOR;
    *knockback *= TECH_RESIST_KNOCKBACK_FACTOR;

    G_PlayTechSound(target->client);
  }

  super.ModifyDamage(target, attacker, damage, knockback);

  if (attacker->client && G_HasTech(attacker->client, TECH_STRENGTH)) {
    *damage *= TECH_STRENGTH_DAMAGE_FACTOR;
    *knockback *= TECH_STRENGTH_KNOCKBACK_FACTOR;

    G_PlayTechSound(attacker->client);
  }
}

/**
 * @brief Indexes the techs' sounds for this level.
 */
static void G_InitMedia_Tech(void) {

  super.InitMedia();

  g_tech_media.sounds[TECH_HASTE    - TECH_FIRST] = gi.SoundIndex("techs/haste/haste");
  g_tech_media.sounds[TECH_REGEN    - TECH_FIRST] = gi.SoundIndex("techs/regen/regen");
  g_tech_media.sounds[TECH_RESIST   - TECH_FIRST] = gi.SoundIndex("techs/resist/resist");
  g_tech_media.sounds[TECH_STRENGTH - TECH_FIRST] = gi.SoundIndex("techs/strength/strength");
  g_tech_media.sounds[TECH_VAMPIRE  - TECH_FIRST] = gi.SoundIndex("techs/vampire/vampire");
}

/**
 * @brief Resolves whether techs are available this level.
 */
static void G_ConfigureLevel_Tech(void) {

  G_Tech_CheckState();

  super.ConfigureLevel();
}

/**
 * @brief Applies the techs' own cvars.
 */
static bool G_CheckCvars_Tech(void) {
  bool restart = false;

  if (g_techs->modified) {
    g_techs->modified = false;

    G_Tech_CheckState();

    gi.BroadcastPrint(PRINT_HIGH, "Techs have been %s\n", G_Tech_Enabled() ? "enabled" : "disabled");

    restart = true;
  }

  return super.CheckCvars() || restart;
}

/**
 * @brief Answers for the tech item type.
 */
static void G_InitItem_Tech(g_item_t *it) {

  if (it->def.type == ITEM_TYPE_TECH) {
    it->Pickup = G_PickupTech;
    it->Drop = G_DropItem;
    return;
  }

  super.InitItem(it);
}

/**
 * @brief Tosses the tech a client leaving play is holding.
 */
static void G_TossInventory_Tech(g_client_t *cl) {

  G_TossTech(cl);

  super.TossInventory(cl);
}

/**
 * @brief Registers the techs' cvars and installs their hooks.
 */
void G_Tech_Init(void) {

  // G_Init runs on every server initialization, and the module is not always
  // unloaded in between, so installing twice would point super at ourselves.
  if (!installed) {
    installed = true;

    super.ResetDroppedItem = G_ResetDroppedItem;
    G_ResetDroppedItem = G_ResetDroppedItem_Tech;

    super.DropInventoryItem = G_DropInventoryItem;
    G_DropInventoryItem = G_DropInventoryItem_Tech;

    super.ModifyDamage = G_ModifyDamage;
    G_ModifyDamage = G_ModifyDamage_Tech;

    super.CheckCvars = G_CheckCvars;
    G_CheckCvars = G_CheckCvars_Tech;
    super.TossInventory = G_TossInventory;
    G_TossInventory = G_TossInventory_Tech;

    super.InitItem = G_InitItem;
    G_InitItem = G_InitItem_Tech;

    super.InitMedia = G_InitMedia;
    G_InitMedia = G_InitMedia_Tech;

    super.ConfigureLevel = G_ConfigureLevel;
    G_ConfigureLevel = G_ConfigureLevel_Tech;
  }

  g_techs = gi.AddCvar("g_techs", "default", CVAR_SERVER_INFO, "Whether to allow techs or not. \"default\" only allows techs in CTF; 1 is always allow, 0 is never allow.");

  g_techs->modified = false;
}

/**
 * @brief Resolves whether techs are active this level, honouring an explicit
 * cvar setting and otherwise taking the feature being compiled in as consent.
 */
void G_Tech_CheckState(void) {

  if (q_strcmp(g_techs->string, "default")) {
    g_tech_enabled = !!g_techs->integer;
  } else {
    g_tech_enabled = true;
  }
}

/**
 * @brief Returns the distance to the nearest tech from the given spot.
 */
static float G_TechRangeFromSpawn(const g_entity_t *spawn) {
  float best_dist = FLT_MAX;
  bool any = false;

  for (g_item_tag_t tech = TECH_FIRST; tech < TECH_LAST; tech++) {

    g_entity_t *ent = NULL;
    G_ForEachEntity(e, {
      if (e->item == &g_items[tech]) {
        ent = e;
        break;
      }
    });

    if (!ent) {
      continue;
    }

    const vec3_t v = Vec3_Subtract(spawn->s.origin, ent->s.origin);
    const float dist = Vec3_Length(v);

    if (dist < best_dist) {
      best_dist = dist;
    }

    any = true;
  }

  if (!any) {
    return Randomf() * MAX_WORLD_DIST;
  }

  return best_dist;
}

/**
 * @brief Finds the spawn point farthest from all existing tech items within the given set.
 */
static void G_SelectFarthestTechSpawnPoint(const g_spawn_points_t *spawn_points, g_entity_t **point, float *point_dist) {

  for (size_t i = 0; i < spawn_points->count; i++) {
    g_entity_t *spot = spawn_points->spots[i];
    float dist = G_TechRangeFromSpawn(spot);

    if (dist > *point_dist) {
      *point = spot;
      *point_dist = dist;
    }
  }
}

/**
 * @brief Selects the optimal spawn point for a tech item by maximizing distance from all other techs.
 */
static g_entity_t *G_SelectTechSpawnPoint(void) {
  float point_dist = -FLT_MAX;
  g_entity_t *point = NULL;

  if (g_level.teams) {
    for (int32_t i = 0; i < g_level.num_teams; i++) {
      G_SelectFarthestTechSpawnPoint(&g_team_list[i].spawn_points, &point, &point_dist);
    }
  } else {
    G_SelectFarthestTechSpawnPoint(&g_level.spawn_points, &point, &point_dist);
  }

  if (!point) {
    G_SelectFarthestTechSpawnPoint(&g_level.spawn_points, &point, &point_dist);
  }

  return point;
}

/**
 * @brief Spawns a single tech item at a randomly selected spawn point with a random initial velocity.
 */
static void G_SpawnTech(const g_item_t *item) {

  g_entity_t *spawn = G_SelectTechSpawnPoint();

  vec3_t angles = spawn->s.angles;
  angles.y += RandomRangef(-45.f, 45.f);

  vec3_t forward;
  Vec3_Vectors(angles, &forward, NULL, NULL);

  g_entity_t *ent = G_AllocEntity(item->def.classname);

  // Techs spawn from the player spawn points, so start clear of the point
  // itself, along the way the tech is about to be thrown. A client spawning in
  // on the first frame of the level would otherwise have the tech inside its
  // bounding box and collect something it never saw.
  ent->s.origin = Vec3_Fmaf(spawn->s.origin, 32.f, forward);

  G_SpawnItem(ent, item);
  ent->next_think = 0;
  ent->Think = NULL;

  // Treat spawned techs like dropped items so they can land near spawn points
  // instead of forcing immediate pickup on spawn.
  ent->spawn_flags |= SF_ITEM_DROPPED;
  ent->move_type = MOVE_TYPE_BOUNCE;
  ent->touch_time = g_level.time + 1000;

  ent->velocity = Vec3_Scale(forward, 100.f);
  ent->velocity.z = 300.f + (Randomf() * 50.f);

  G_ResetItem(ent);
}

/**
 * @brief Spawn all of the techs.
 */
void G_Tech_SpawnAll(void) {

  if (!g_tech_enabled) {
    return;
  }

  for (g_item_tag_t i = TECH_FIRST; i < TECH_LAST; i++) {
    G_SpawnTech(&g_items[i]);
  }
}

/**
 * @brief Respawns a tech item at a new spawn point and frees the dropped entity.
 */
void G_ResetDroppedTech(g_entity_t *ent) {

  G_SpawnTech(ent->item);

  G_FreeEntity(ent);
}

/**
 * @brief Check if a player has the specified tech.
 */
bool G_HasTech(const g_client_t *cl, g_item_tag_t tech) {
  return !!cl->inventory[tech];
}

/**
 * @brief Pickup function for techs. Can only hold one tech at a time.
 */
bool G_PickupTech(g_client_t *cl, g_entity_t *ent) {

  for (g_item_tag_t tech = TECH_FIRST; tech < TECH_LAST; tech++) {

    if (G_HasTech(cl, tech)) {
      return false;
    }
  }

  // add the weapon to inventory
  cl->inventory[ent->item->def.tag]++;

  return true;
}

/**
 * @brief Returns the tech item currently held by the client, or `NULL` if none.
 */
const g_item_t *G_GetTech(const g_client_t *cl) {

  for (g_item_tag_t i = TECH_FIRST; i < TECH_LAST; i++) {

    if (G_HasTech(cl, i)) {
      return &g_items[i];
    }
  }

  return NULL;
}

/**
 * @brief Drops the tech item currently carried by the client as a world entity.
 */
g_entity_t *G_TossTech(g_client_t *cl) {
  const g_item_t *tech = G_GetTech(cl);

  if (!tech) {
    return NULL;
  }

  cl->inventory[tech->def.tag] = 0;

  return G_DropItem(cl, tech);
}

/**
 * @brief Plays the activation or ambient sound for the client's currently held tech powerup.
 */
void G_PlayTechSound(g_client_t *cl) {

  const g_item_t *tech = G_GetTech(cl);

  if (!tech) {
    return;
  }

  if (cl->tech.sound_time < g_level.time) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = g_tech_media.sounds[tech->def.tag - TECH_FIRST],
      .entity = cl->entity,
    }, MULTICAST_PHS);
    cl->tech.sound_time = g_level.time + 500;
  }
}

/**
 * @brief Applies the regeneration tech's periodic healing.
 */
void G_Tech_ClientThink(g_entity_t *ent) {

  g_client_t *cl = ent->client;

  if (!G_HasTech(cl, TECH_REGEN)) {
    return;
  }

  if (cl->tech.regen_time < g_level.time) {
    cl->tech.regen_time = g_level.time + TECH_REGEN_TICK_TIME;

    if (ent->health < ent->max_health) {
      ent->health = Minf(ent->health + TECH_REGEN_HEALTH, ent->max_health);
      G_PlayTechSound(cl);
    }
  }
}
