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
#include "bg_pmove.h"

/**
 * @brief The entity class structure.
 */
typedef struct {

  /**
   * @brief The entity class name.
   */
  char *classname;

  /**
   * @brief The instance initializer.
   */
  void (*Init)(g_entity_t *ent);
} g_entity_class_t;

static void G_worldspawn(g_entity_t *ent);

/**
 * @brief The entity classes.
 */
static const g_entity_class_t g_entity_classes[] = {

  { "func_button", G_func_button },
  { "func_conveyor", G_func_conveyor },
  { "func_door", G_func_door },
  { "func_door_rotating", G_func_door_rotating },
  { "func_door_secret", G_func_door_secret },
  { "func_plat", G_func_plat },
  { "func_rotating", G_func_rotating },
  { "func_timer", G_func_timer },
  { "func_train", G_func_train },
  { "func_wall", G_func_wall },
  { "func_water", G_func_water },

  { "info_notnull", G_info_notnull },
  { "info_player_intermission", G_info_player_intermission },
  { "info_player_deathmatch", G_info_player_deathmatch },
  { "info_player_start", G_info_player_start },
  { "info_player_team1", G_info_player_team1 },
  { "info_player_team2", G_info_player_team2 },
  { "info_player_team3", G_info_player_team3 },
  { "info_player_team4", G_info_player_team4 },
  { "info_player_team_any", G_info_player_team_any },

  { "misc_teleporter", G_misc_teleporter },
  { "misc_teleporter_dest", G_misc_teleporter_dest },
  { "misc_fireball", G_misc_fireball },

  { "path_corner", G_info_notnull },

  { "target_light", G_target_light },
  { "target_speaker", G_target_speaker },
  { "target_string", G_target_string },

  { "trigger_always", G_trigger_always },
  { "trigger_exec", G_trigger_exec },
  { "trigger_hurt", G_trigger_hurt },
  { "trigger_multiple", G_trigger_multiple },
  { "trigger_once", G_trigger_once },
  { "trigger_push", G_trigger_push },
  { "trigger_relay", G_trigger_relay },
  { "trigger_void", G_trigger_void },
  { "trigger_teleporter", G_misc_teleporter },

  { "worldspawn", G_worldspawn },

  // lastly, these are entities which we intentionally suppress

  { "func_group", G_FreeEntity },
  { "info_null", G_FreeEntity },

  { "light", G_FreeEntity },

  { "misc_dust", G_FreeEntity },
  { "misc_flame", G_FreeEntity },
  { "misc_light", G_FreeEntity },
  { "misc_model", G_FreeEntity },
  { "misc_sound", G_FreeEntity },
  { "misc_sparks", G_FreeEntity },
  { "misc_sprite", G_FreeEntity },
  { "misc_steam", G_FreeEntity },
  { "misc_weather", G_FreeEntity },
};

static const cm_entity_t *g_map;

static const cm_entity_t *G_MapValue(const char *key) {
  return g_map ? gi.EntityValue(g_map, key) : NULL;
}

/**
 * @brief Populates common entity fields and then dispatches the class initializer.
 */
static void G_SpawnEntity(cm_entity_t *def) {

  const char *classname = gi.EntityValue(def, "classname")->string;
  g_entity_t *ent = G_AllocEntity(classname);

  ent->def = def;

  ent->s.origin = gi.EntityValue(ent->def, "origin")->vec3;
  ent->s.angles = gi.EntityValue(ent->def, "angles")->vec3;

  const cm_entity_t *angle = gi.EntityValue(ent->def, "angle");
  if (angle->parsed & ENTITY_FLOAT) {
    ent->s.angles = Vec3(0.f, angle->value, 0.f);
  }

  ent->model = gi.EntityValue(ent->def, "model")->nullable_string;

  ent->spawn_flags = gi.EntityValue(ent->def, "spawnflags")->integer;

  ent->speed = gi.EntityValue(ent->def, "speed")->value;
  ent->accel = gi.EntityValue(ent->def, "accel")->value;
  ent->decel = gi.EntityValue(ent->def, "decel")->value;

  ent->target = gi.EntityValue(ent->def, "target")->nullable_string;
  ent->target_name = gi.EntityValue(ent->def, "targetname")->nullable_string;
  ent->message = gi.EntityValue(ent->def, "message")->nullable_string;
  ent->team = gi.EntityValue(ent->def, "team")->nullable_string;

  ent->wait = gi.EntityValue(ent->def, "wait")->value;
  ent->delay = gi.EntityValue(ent->def, "delay")->value;
  ent->random = gi.EntityValue(ent->def, "random")->value;
  ent->count = gi.EntityValue(ent->def, "count")->integer;

  ent->health = gi.EntityValue(ent->def, "health")->integer;
  ent->damage = gi.EntityValue(ent->def, "dmg")->integer;
  ent->mass = gi.EntityValue(ent->def, "mass")->value;

  // check item spawn functions
  const g_item_t *it = G_FindItemByClassName(ent->classname);
  if (it) {
    G_SpawnItem(ent, it);
    return;
  }

  // check normal spawn functions
  for (size_t i = 0; i < lengthof(g_entity_classes); i++) {
    const g_entity_class_t *clazz = g_entity_classes + i;

    if (!g_strcmp0(clazz->classname, ent->classname)) {
      clazz->Init(ent);
      return;
    }
  }

  gi.Warn("%s doesn't have a spawn function\n", etos(ent));

  G_FreeEntity(ent);
}

/**
 * @brief Chain together all entities with a matching team field.
 *
 * All but the first will have the `FL_TEAM_SLAVE` flag set.
 * All but the last will have the `team_next` field set to the next one.
 */
static void G_InitEntityTeams(void) {

  int32_t teams = 0, team_entities = 0;

  G_ForEachEntity(ent, {

    if (!ent->team) {
      continue;
    }

    if (ent->flags & FL_TEAM_SLAVE) {
      continue;
    }

    g_entity_t *team = ent;
    ent->team_master = ent;

    teams++;
    team_entities++;

    G_ForEachEntity(e, {

      if (e->s.number <= ent->s.number) {
        continue;
      }

      if (!e->team) {
        continue;
      }

      if (e->flags & FL_TEAM_SLAVE) {
        continue;
      }

      if (!g_strcmp0(ent->team, e->team)) {

        e->team_master = ent;
        e->flags |= FL_TEAM_SLAVE;

        team->team_next = e;
        team = e;

        team_entities++;
      }
    });
  });

  G_Debug("%i teams with %i entities\n", teams, team_entities);
}

/**
 * @brief Resolves references to frequently accessed media.
 */
static void G_InitMedia(void) {
  uint16_t i;

  memset(&g_media, 0, sizeof(g_media));

  // preload items/set item media ptrs
  G_InitItems();

  // precache player sounds; clients will load these when a new player model gets loaded.
  gi.SoundIndex("*death_1");
  gi.SoundIndex("*death_2");
  gi.SoundIndex("*death_void");
  gi.SoundIndex("*drown_1");
  gi.SoundIndex("*fall_1");
  gi.SoundIndex("*fall_2");
  gi.SoundIndex("*gasp_1");
  gi.SoundIndex("*gurp_1");
  gi.SoundIndex("*jump_1");
  gi.SoundIndex("*jump_2");
  gi.SoundIndex("*jump_3");
  gi.SoundIndex("*jump_4");
  gi.SoundIndex("*jump_5");
  gi.SoundIndex("*land_1");
  gi.SoundIndex("*pain25_1");
  gi.SoundIndex("*pain50_1");
  gi.SoundIndex("*pain75_1");
  gi.SoundIndex("*pain100_1");
  gi.SoundIndex("*sizzle_1");

  g_media.models.grenade = gi.ModelIndex("models/projectiles/grenade/tris");
  g_media.models.quake_grenade = gi.ModelIndex("models/projectiles/quake_grenade/tris");
  g_media.models.quake_nail = gi.ModelIndex("models/projectiles/quake_nail/tris");
  g_media.models.rocket = gi.ModelIndex("models/projectiles/rocket/tris");
  g_media.models.quake_rocket = gi.ModelIndex("models/projectiles/quake_rocket/tris");
  g_media.models.hook = gi.ModelIndex("models/grapplehook/tris");
  g_media.models.fireball = gi.ModelIndex("models/fireball/tris");
  g_media.sounds.bfg_hit = gi.SoundIndex("weapons/bfg/hit");
  g_media.sounds.bfg_prime = gi.SoundIndex("weapons/bfg/prime");
  g_media.sounds.death[0] = gi.SoundIndex("*death_1");
  g_media.sounds.death[1] = gi.SoundIndex("*death_2");
  g_media.sounds.gasp = gi.SoundIndex("*gasp_1");
  g_media.sounds.gurp = gi.SoundIndex("*gurp_1");
  g_media.sounds.pain[0] = gi.SoundIndex("*pain25_1");
  g_media.sounds.pain[1] = gi.SoundIndex("*pain50_1");
  g_media.sounds.pain[2] = gi.SoundIndex("*pain75_1");
  g_media.sounds.pain[3] = gi.SoundIndex("*pain100_1");
  g_media.sounds.grenade_hit = gi.SoundIndex("projectiles/grenade/hit");
  g_media.sounds.grenade_clang = gi.SoundIndex("weapons/handgrenades/hg_clang");
  g_media.sounds.grenade_throw = gi.SoundIndex("weapons/handgrenades/hg_throw");
  g_media.sounds.grenade_tick = gi.SoundIndex("weapons/handgrenades/hg_tick.ogg");
  g_media.sounds.quake_grenade_hit = gi.SoundIndex("projectiles/quake_grenade/hit");
  g_media.sounds.quake_nail_hit = gi.SoundIndex("projectiles/quake_nail/hit");
  g_media.sounds.rocket_fly = gi.SoundIndex("projectiles/rocket/fly");
  g_media.sounds.lightning_fly = gi.SoundIndex("weapons/lightning/fly");
  g_media.sounds.quad_attack = gi.SoundIndex("powerups/quad/attack");
  g_media.sounds.quad_expire = gi.SoundIndex("powerups/quad/expire");
  g_media.sounds.invulnerability_pickup = gi.SoundIndex("powerups/invulnerability/pickup");
  g_media.sounds.invulnerability_expire = gi.SoundIndex("powerups/invulnerability/expire");
  g_media.sounds.invulnerability_protect = gi.SoundIndex("powerups/invulnerability/protect");
  g_media.sounds.invisibility_pickup = gi.SoundIndex("powerups/invisibility/pickup");
  g_media.sounds.invisibility_expire = gi.SoundIndex("powerups/invisibility/expire");
  g_media.sounds.hook_fire = gi.SoundIndex("grapplehook/fire");
  g_media.sounds.hook_fly = gi.SoundIndex("grapplehook/fly");
  g_media.sounds.hook_hit = gi.SoundIndex("grapplehook/hit");
  g_media.sounds.hook_pull = gi.SoundIndex("grapplehook/pull");
  g_media.sounds.hook_detach = gi.SoundIndex("grapplehook/detach");
  g_media.sounds.hook_gibhit = gi.SoundIndex("grapplehook/gibhit");
  g_media.sounds.teleport = gi.SoundIndex("misc/teleport");

  for (i = 0; i < lengthof(g_media.sounds.quake_teleport); i++) {
    g_media.sounds.quake_teleport[i] = gi.SoundIndex(va("misc/quake_teleport%d", i + 1));
  }

  g_media.sounds.water_in = gi.SoundIndex("misc/water_in");
  g_media.sounds.water_out = gi.SoundIndex("misc/water_out");

  g_media.sounds.weapon_no_ammo = gi.SoundIndex("weapons/common/no_ammo");
  g_media.sounds.weapon_switch = gi.SoundIndex("weapons/common/switch");

  g_media.sounds.chat = gi.SoundIndex("misc/chat");
  g_media.sounds.ctf_capture = gi.SoundIndex("ctf/capture");
  g_media.sounds.ctf_return = gi.SoundIndex("ctf/return");
  g_media.sounds.ctf_steal = gi.SoundIndex("ctf/steal");

  for (i = 0; i < NUM_GIB_MODELS; i++) {
    g_media.models.gibs[i] = gi.ModelIndex(va("models/gibs/gib_%i/tris", i + 1));
  }

  for (i = 0; i < lengthof(g_media.sounds.lava); i++) {
    g_media.sounds.lava[i] = gi.SoundIndex(va("ambient/lava_%d", i + 1));
  }

  for (i = 0; i < NUM_GIB_SOUNDS; i++) {
    g_media.sounds.gib_hits[i] = gi.SoundIndex(va("gibs/gib_%i/hit", i + 1));
  }

  for (i = 1; i < lengthof(g_media.sounds.countdown); i++) {
    g_media.sounds.countdown[i] = gi.SoundIndex(va("misc/countdown_%d", i));
  }

  g_media.sounds.roar = gi.SoundIndex("misc/ominous_bwah");

  g_media.sounds.techs[TECH_HASTE - TECH_FIRST]    = gi.SoundIndex("techs/haste/haste");
  g_media.sounds.techs[TECH_REGEN - TECH_FIRST]    = gi.SoundIndex("techs/regen/regen");
  g_media.sounds.techs[TECH_RESIST - TECH_FIRST]   = gi.SoundIndex("techs/resist/resist");
  g_media.sounds.techs[TECH_STRENGTH - TECH_FIRST] = gi.SoundIndex("techs/strength/strength");
  g_media.sounds.techs[TECH_VAMPIRE - TECH_FIRST]  = gi.SoundIndex("techs/vampire/vampire");

  g_media.images.health = gi.ImageIndex("pics/i_health");
}

/**
 * @brief Sorts the spawns so that the furthest from the flag are at the beginning.
 */
static int32_t G_CreateTeamSpawnPoints_CompareFunc(gconstpointer a, gconstpointer b, gpointer user_data) {

  g_entity_t *flag = (g_entity_t *) user_data;
  
  const g_entity_t *ap = (const g_entity_t *) a;
  const g_entity_t *bp = (const g_entity_t *) b;

  const int32_t a_dist = Vec3_DistanceSquared(flag->s.origin, ap->s.origin);
  const int32_t b_dist = Vec3_DistanceSquared(flag->s.origin, bp->s.origin);

  return SignOf(b_dist - a_dist);
}

/**
 * @brief Creates team spawns if the map doesn't have one. Also creates flags, although
 * chances are they will be in crap positions.
 */
static void G_CreateTeamSpawnPoints(GSList **dm_spawns, GSList **team_red_spawns, GSList **team_blue_spawns) {

  // find our flags
  g_entity_t *red_flag, *blue_flag;
  g_entity_t *reused_spawns[2] = { NULL, NULL };
  
  red_flag = g_team_red->flag_entity;
  blue_flag = g_team_blue->flag_entity;

  if (!!red_flag != !!blue_flag) {
    gi.Error("Make sure you have both flags in your map!\n");
  } else if (!red_flag) {
    // no flag in map, so let's make one by repurposing the furthest spawn points

    if (g_slist_length(*dm_spawns) < 4) {
      return; // not enough points to make a flag
    }

    float furthest_dist = 0;

    for (GSList *pa = *dm_spawns; pa; pa = pa->next) {
      for (GSList *pb = *dm_spawns; pb; pb = pb->next) {

        if (pa == pb) {
          continue;
        }
        
        g_entity_t *pae = (g_entity_t *) pa->data;
        g_entity_t *pab = (g_entity_t *) pb->data;

        if ((reused_spawns[0] == pae && reused_spawns[1] == pab) ||
          (reused_spawns[0] == pab && reused_spawns[1] == pae)) {
          continue;
        }

        vec3_t line;
        line = Vec3_Subtract(pae->s.origin, pab->s.origin);
        line.z /= 10.0; // don't consider Z as heavily as X/Y

        const float dist = Vec3_LengthSquared(line);

        if (dist > furthest_dist) {

          reused_spawns[0] = pae;
          reused_spawns[1] = pab;
          furthest_dist = dist;
        }
      }
    }

    if (!reused_spawns[0] || !reused_spawns[1] || !furthest_dist) {
      return; // error in finding furthest points
    }
    
    red_flag = G_AllocEntity(g_team_red->flag);
    blue_flag = G_AllocEntity(g_team_blue->flag);

    const uint8_t r = RandomRangeu(0, 2);

    red_flag->s.origin = reused_spawns[r]->s.origin;
    blue_flag->s.origin = reused_spawns[r ^ 1]->s.origin;
    
    G_SpawnItem(red_flag, &g_items[FLAG_RED]);
    G_SpawnItem(blue_flag, &g_items[FLAG_BLUE]);
    
    g_team_red->flag_entity = red_flag;
    g_team_blue->flag_entity = blue_flag;
  }

  for (GSList *point = *dm_spawns; point; point = point->next) {

    g_entity_t *p = (g_entity_t *) point->data;

    if (p == reused_spawns[0] || p == reused_spawns[1]) {
      continue;
    }

    const float dist_to_red = Vec3_DistanceSquared(red_flag->s.origin, p->s.origin);
    const float dist_to_blue = Vec3_DistanceSquared(blue_flag->s.origin, p->s.origin);

    if (dist_to_red < dist_to_blue) {
      *team_red_spawns = g_slist_prepend(*team_red_spawns, p);
    } else {
      *team_blue_spawns = g_slist_prepend(*team_blue_spawns, p);
    }
  }

  if (g_slist_length(*team_red_spawns) == g_slist_length(*team_blue_spawns)) {
    return; // best case scenario
  }
  
  // unmatched spawns, we need to move some
  *team_red_spawns = g_slist_sort_with_data(*team_red_spawns, G_CreateTeamSpawnPoints_CompareFunc, red_flag);
  *team_blue_spawns = g_slist_sort_with_data(*team_blue_spawns, G_CreateTeamSpawnPoints_CompareFunc, blue_flag);
    
  int32_t num_red_spawns = (int32_t) g_slist_length(*team_red_spawns);
  int32_t num_blue_spawns = (int32_t) g_slist_length(*team_blue_spawns);
  int32_t diff = abs(num_red_spawns - num_blue_spawns);

  GSList **from, **to;

  if (num_red_spawns > num_blue_spawns) {
    from = team_red_spawns;
    to = team_blue_spawns;
  } else {
    from = team_blue_spawns;
    to = team_red_spawns;
  }

  // odd number of points, make one neutral
  if (diff & 1) {
    g_entity_t *point = (g_entity_t *) ((*from)->data);

    *to = g_slist_prepend(*to, point);
  }

  int32_t num_move = diff - 1;
  
  // move spawns to the other team
  while (num_move) {

    g_entity_t *point = (g_entity_t *) ((*from)->data);

    *from = g_slist_remove(*from, point);
    *to = g_slist_prepend(*to, point);

    num_move--;
  }
}

/**
 * @brief Set up the list of spawn points.
 */
static void G_InitSpawnPoints(void) {

  // first, set up all of the deathmatch points
  GSList *dm_spawns = NULL;
  g_entity_t *spot = NULL;

  while ((spot = G_Find(spot, EOFS(classname), "info_player_deathmatch")) != NULL) {
    dm_spawns = g_slist_prepend(dm_spawns, spot);
  }

  if (!dm_spawns) {
    spot = NULL;

    while ((spot = G_Find(spot, EOFS(classname), "info_player_start")) != NULL) {
      dm_spawns = g_slist_prepend(dm_spawns, spot);
    }
  }
  
  // find the team points, if we have any explicit ones in the map.
  // start by finding the flags
  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    g_team_list[t].flag_entity = G_Find(NULL, EOFS(classname), g_team_list[t].flag);
  }

  GSList *team_spawns[MAX_TEAMS];

  memset(team_spawns, 0, sizeof(team_spawns));

  spot = NULL;

  while ((spot = G_Find(spot, EOFS(classname), "info_player_team_any")) != NULL) {
    for (int32_t t = 0; t < MAX_TEAMS; t++) {
      team_spawns[t] = g_slist_prepend(team_spawns[t], spot);
    }
  }

  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    spot = NULL;
    g_team_t *team = &g_team_list[t];

    while ((spot = G_Find(spot, EOFS(classname), team->spawn)) != NULL) {
      team_spawns[t] = g_slist_prepend(team_spawns[t], spot);
    }

    team->spawn_points.count = g_slist_length(team_spawns[t]);
  }

  g_level.spawn_points.count = g_slist_length(dm_spawns);

  GSList *point = NULL;

  // in the odd case that the map only has team spawns, we'll use them
  if (!g_level.spawn_points.count) {
    for (int32_t t = 0; t < MAX_TEAMS; t++) {
      for (point = team_spawns[t]; point; point = point->next) {
        dm_spawns = g_slist_prepend(dm_spawns, (g_entity_t *) point->data);
      }
    }
    
    g_level.spawn_points.count = g_slist_length(dm_spawns);

    if (!g_level.spawn_points.count) {
      gi.Error("Map has no spawn points\n");
    }
  }

  if (!g_team_red->spawn_points.count) {

    // none in the map, let's make some!
    G_CreateTeamSpawnPoints(&dm_spawns, &team_spawns[TEAM_RED], &team_spawns[TEAM_BLUE]);

    // re-calculate final values since the above may change them
    for (int32_t t = 0; t < MAX_TEAMS; t++) {
      g_team_list[t].spawn_points.count = g_slist_length(team_spawns[t]);
    }

    g_level.spawn_points.count = g_slist_length(dm_spawns);
  }

  // copy all the data in!
  size_t i;

  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    g_team_list[t].spawn_points.spots = gi.Malloc(sizeof(g_entity_t *) * g_team_list[t].spawn_points.count, MEM_TAG_GAME_LEVEL);
  
    for (i = 0, point = team_spawns[t]; point; point = point->next, i++) {
      g_team_list[t].spawn_points.spots[i] = (g_entity_t *) point->data;
    }
  
    g_slist_free(team_spawns[t]);
  }
  
  g_level.spawn_points.spots = gi.Malloc(sizeof(g_entity_t *) * g_level.spawn_points.count, MEM_TAG_GAME_LEVEL);

  for (i = 0, point = dm_spawns; point; point = point->next, i++) {
    g_level.spawn_points.spots[i] = (g_entity_t *) point->data;
  }

  g_slist_free(dm_spawns);

  G_InitNumTeams();
}

/**
 * @brief Spawns a single tech item at a randomly selected spawn point with a random initial velocity.
 */
void G_SpawnTech(const g_item_t *item) {

  g_entity_t *spawn = G_SelectTechSpawnPoint();

  g_entity_t *ent = G_AllocEntity(item->def.classname);
  ent->s.origin = spawn->s.origin;

  G_SpawnItem(ent, item);
  ent->next_think = 0;
  ent->Think = NULL;

  // Treat spawned techs like dropped items so they can land near spawn points
  // instead of forcing immediate pickup on spawn.
  ent->spawn_flags |= SF_ITEM_DROPPED;
  ent->move_type = MOVE_TYPE_BOUNCE;
  ent->touch_time = g_level.time + 1000;

  vec3_t angles = spawn->s.angles;
  angles.y += RandomRangef(-45.f, 45.f);

  vec3_t forward;
  Vec3_Vectors(angles, &forward, NULL, NULL);

  ent->velocity = Vec3_Scale(forward, 100.f);
  ent->velocity.z = 300.f + (Randomf() * 50.f);

  G_ResetItem(ent);
}

/**
 * @brief Spawn all of the techs
 */
void G_SpawnTechs(void) {

  if (!g_level.techs) {
    return;
  }

  for (g_item_tag_t i = TECH_FIRST; i < TECH_LAST; i++) {
    G_SpawnTech(&g_items[i]);
  }
}

/**
 * @brief Spawns game entities from the BSP entity definition lump.
 */
void G_SpawnEntities(const char *name, const cm_entity_t *props, cm_entity_t *const *entities, size_t num_entities) {

  // Drop bots, they will reconnect via G_Ai_Frame
  G_ForEachClient(cl, {
    if (cl->ai) {
      G_ClientDisconnect(cl);
    }
  });

  gi.FreeTag(MEM_TAG_GAME_LEVEL);

  memset(&g_level, 0, sizeof(g_level));

  g_strlcpy(g_level.name, name, sizeof(g_level.name));

  g_level.frags    = g_array_new(false, false, sizeof(g_frag_t));
  g_level.captures = g_array_new(false, false, sizeof(g_capture_t));

  // Clear real client entity pointers before freeing entities to prevent dangling references
  G_ForEachClient(cl, {
    cl->entity = NULL;
    cl->persistent.score = 0;
    cl->persistent.captures = 0;
    cl->persistent.deaths = 0;
    cl->persistent.team = NULL;
  });

  for (int32_t i = 0; i < sv_max_entities->integer; i++) {
    G_FreeEntity(ge.entities[i]);
  }

  g_map = props;

  G_InitMedia();

  for (size_t i = 0; i < num_entities; i++) {
    G_SpawnEntity(entities[i]);
  }

  G_InitEntityTeams();

  G_CheckHook();

  G_CheckTechs();

  G_ResetTeams();

  if (editor->value) {
    G_info_player_start(G_AllocEntity("info_player_start"));
  }

  G_InitSpawnPoints();

  G_ResetSpawnPoints();

  G_ResetItems();

  G_Ai_Load();

  g_map = NULL;
}

/**
 * @brief CompareFunc to shuffle tracks.
 */
gint G_worldspawn_MusicShuffle(gconstpointer a, gconstpointer b) {
  return RandomRangei(-1, 2);
}

/**
 * @brief `Fs_Enumerator` to collect music track names.
 */
static void G_worldspawn_EnumerateMusic(const char *path, void *data) {
  GArray *tracks = (GArray *) data;
  char name[MAX_QPATH];
  StripExtension(Basename(path), name);
  if (g_strcmp0(name, "gtdstudio-explore") == 0) {
    return;
  }
  g_array_append_val(tracks, name);
}

/**
 * @brief Selects and sets the music tracks for the current level from the music key or available files.
 */
static void G_worldspawn_Music(void) {

  if (*g_level.music == '\0') {

    GArray *tracks = g_array_new(0, 0, MAX_QPATH);
    gi.EnumerateFiles("music/*.ogg", G_worldspawn_EnumerateMusic, tracks);
    g_array_sort(tracks, G_worldspawn_MusicShuffle);

    for (int32_t i = 0; i < MIN(MAX_MUSICS, (int32_t) tracks->len); i++) {
      gi.SetConfigString(CS_MUSICS + i, &g_array_index(tracks, char, i * MAX_QPATH));
    }

    g_array_free(tracks, 1);
    return;
  }

  char buf[MAX_STRING_CHARS];
  g_strlcpy(buf, g_level.music, sizeof(buf));

  int32_t i = 0;
  char *t = strtok(buf, ",");

  while (true) {

    if (!t) {
      break;
    }

    if (i == MAX_MUSICS) {
      break;
    }

    if (*t != '\0') {
      gi.SetConfigString(CS_MUSICS + i++, g_strstrip(t));
    }

    t = strtok(NULL, ",");
  }
}

/*QUAKED worldspawn (0 0 0) ?
 The worldspawn entity defines global conditions and behavior for the entire level. All brushes not belonging to an explicit entity implicitly belong to worldspawn.
 -------- KEYS --------
 message : The map title.
 sky : The sky environment map (default unit1_).
 ambient : The ambient light level (e.g. 0.14 0.11 0.12).
 gravity : Gravity for the level (default 800).
 gameplay : The gameplay mode, one of "deathmatch, instagib, arena."
 hook : Enables the grappling hook (unset for gameplay default, 0 = disabled, 1 = enabled)."
 teams : Enables and enforces teams play (enabled = 1, auto-balance = 2).
 num_teams : Enforces number of teams (disabled = -1, must be between 2 and 4)
 ctf : Enables CTF play (enabled = 1, auto-balance = 2).
 fraglimit : The frag limit (default 20).
 roundlimit : The round limit (default 20).
 capturelimit : The capture limit (default 8).
 timelimit : The time limit in minutes (default 20).
 give : A comma-delimited item string to give each player on spawn.
 items : The item set for this map: "default" (Quetoo weapons) or "quake" (Quake weapons).
 */
static void G_worldspawn(g_entity_t *ent) {

  ent->solid = SOLID_BSP;
  ent->move_type = MOVE_TYPE_NONE;
  ent->s.effects = EF_WORLD;

  // ensure worldspawn has no origin or angles, as these would cause the
  // entire world model to be rendered offset or rotated as an entity
  ent->s.origin = Vec3_Zero();
  ent->s.angles = Vec3_Zero();

  gi.SetModel(ent, "*0");

  ent->s.bounds = ent->bounds;

  if (ent->message && *ent->message) {
    g_strlcpy(g_level.message, ent->message, sizeof(g_level.message));
  } else {
    g_strlcpy(g_level.message, g_level.name, sizeof(g_level.message));
  }

  gi.SetConfigString(CS_MESSAGE, g_level.message);
  gi.SetConfigString(CS_MAX_CLIENTS, va("%d", sv_max_clients->integer));

  const cm_entity_t *gravity_map = G_MapValue("gravity");
  if (gravity_map && (gravity_map->parsed & ENTITY_INTEGER) && gravity_map->integer > 0) { // prefer map metadata gravity
    g_level.gravity = gravity_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *gravity = gi.EntityValue(ent->def, "gravity");
    if (gravity->parsed & ENTITY_INTEGER) {
      g_level.gravity = gravity->integer;
    } else {
      g_level.gravity = DEFAULT_GRAVITY;
    }
  }

  const cm_entity_t *gameplay_map = G_MapValue("gameplay");
  if (g_strcmp0(g_gameplay->string, "default")) { // prefer g_gameplay
    g_level.gameplay = G_GameplayByName(g_gameplay->string);
  } else if (gameplay_map && (gameplay_map->parsed & ENTITY_INTEGER) && gameplay_map->integer > -1) { // then map metadata gameplay
    g_level.gameplay = gameplay_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *gameplay = gi.EntityValue(ent->def, "gameplay");
    if (*gameplay->string) {
      g_level.gameplay = G_GameplayByName(gameplay->string);
    } else {
      g_level.gameplay = GAME_DEATHMATCH;
    }
  }

  gi.SetConfigString(CS_GAMEPLAY, va("%d", g_level.gameplay));

  const cm_entity_t *items = gi.EntityValue(ent->def, "items");
  if (g_ascii_strcasecmp(items->string, "quake") == 0) {
    g_level.items = ITEMS_QUAKE;
  } else {
    g_level.items = ITEMS_DEFAULT;
  }

  gi.SetConfigString(CS_ITEM_SET, va("%d", g_level.items));

  const cm_entity_t *teams_map = G_MapValue("teams");
  if (teams_map && (teams_map->parsed & ENTITY_INTEGER) && teams_map->integer > -1) { // prefer map metadata teams
    g_level.teams = teams_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *teams = gi.EntityValue(ent->def, "teams");
    if (teams->parsed & ENTITY_INTEGER) {
      g_level.teams = teams->integer;
    } else {
      g_level.teams = g_teams->integer;
    }
  }

  const cm_entity_t *num_teams_map = G_MapValue("num_teams");
  if (num_teams_map && (num_teams_map->parsed & ENTITY_INTEGER) && num_teams_map->integer > -1) { // prefer map metadata teams
    g_level.num_teams = num_teams_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *num_teams = gi.EntityValue(ent->def, "num_teams");
    if (num_teams->parsed & ENTITY_INTEGER) {
      g_level.num_teams = num_teams->integer;
    } else {
      if (g_strcmp0(g_num_teams->string, "default")) {
        g_level.num_teams = g_num_teams->integer;
      } else {
        g_level.num_teams = -1; // spawn point function will do this
      }
    }
  }

  if (g_level.num_teams != -1) {
    g_level.num_teams = Clampf(g_level.num_teams, 2, MAX_TEAMS);
  }

  const cm_entity_t *ctf_map = G_MapValue("ctf");
  if (ctf_map && (ctf_map->parsed & ENTITY_INTEGER) && ctf_map->integer > -1) { // prefer map metadata ctf
    g_level.ctf = ctf_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *ctf = gi.EntityValue(ent->def, "ctf");
    if (ctf->parsed & ENTITY_INTEGER) {
      g_level.ctf = ctf->integer;
    } else {
      g_level.ctf = g_ctf->integer;
    }
  }

  const cm_entity_t *hook_map = G_MapValue("hook");
  if (hook_map && (hook_map->parsed & ENTITY_INTEGER) && hook_map->integer > -1) {
    g_level.hook_map = hook_map->integer;
  } else {
    g_level.hook_map = -1;
  }

  const cm_entity_t *techs_map = G_MapValue("techs");
  if (techs_map && (techs_map->parsed & ENTITY_INTEGER) && techs_map->integer > -1) {
    g_level.techs_map = techs_map->integer;
  } else {
    g_level.techs_map = -1;
  }

  const cm_entity_t *min_clients_map = G_MapValue("min_clients");
  if (min_clients_map && (min_clients_map->parsed & ENTITY_INTEGER) && min_clients_map->integer > -1) {
    g_level.min_clients_map = min_clients_map->integer;
  } else {
    g_level.min_clients_map = -1;
  }

  if (g_level.teams && g_level.ctf) { // ctf overrides teams
    g_level.teams = 0;
  }

  gi.SetConfigString(CS_CTF, va("%d", g_level.ctf));
  gi.SetConfigString(CS_HOOK_PULL_SPEED, g_hook_pull_speed->string);

  const cm_entity_t *frag_limit_map = G_MapValue("frag_limit");
  if (frag_limit_map && (frag_limit_map->parsed & ENTITY_INTEGER) && frag_limit_map->integer > -1) { // prefer map metadata frag_limit
    g_level.frag_limit = frag_limit_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *frag_limit = gi.EntityValue(ent->def, "frag_limit");
    if (frag_limit->parsed & ENTITY_INTEGER) {
      g_level.frag_limit = frag_limit->integer;
    } else {
      g_level.frag_limit = g_frag_limit->integer;
    }
  }

  const cm_entity_t *capture_limit_map = G_MapValue("capture_limit");
  if (capture_limit_map && (capture_limit_map->parsed & ENTITY_INTEGER) && capture_limit_map->integer > -1) { // prefer map metadata capture_limit
    g_level.capture_limit = capture_limit_map->integer;
  } else { // or fall back on worldspawn
    const cm_entity_t *capture_limit = gi.EntityValue(ent->def, "capture_limit");
    if (capture_limit->parsed & ENTITY_INTEGER) {
      g_level.capture_limit = capture_limit->integer;
    } else {
      g_level.capture_limit = g_capture_limit->integer;
    }
  }

  float minutes;
  const cm_entity_t *time_limit_map = G_MapValue("time_limit");
  if (time_limit_map && (time_limit_map->parsed & ENTITY_FLOAT) && time_limit_map->value > -1.f) { // prefer map metadata time_limit
    minutes = time_limit_map->value;
  } else { // or fall back on worldspawn
    const cm_entity_t *time_limit = gi.EntityValue(ent->def, "time_limit");
    if (time_limit->parsed & ENTITY_FLOAT) {
      minutes = time_limit->value;
    } else {
      minutes = g_time_limit->value;
    }
  }
  g_level.time_limit = minutes * 60 * 1000;

  const cm_entity_t *give_map = G_MapValue("give");
  if (give_map && *give_map->string) { // prefer map metadata give
    g_strlcpy(g_level.give, give_map->string, sizeof(g_level.give));
  } else { // or fall back on worldspawn
    const cm_entity_t *give = gi.EntityValue(ent->def, "give");
    if (*give->string) {
      g_strlcpy(g_level.give, give->string, sizeof(g_level.give));
    } else {
      g_level.give[0] = '\0';
    }
  }

  const cm_entity_t *music_map = G_MapValue("music");
  if (music_map && *music_map->string) { // prefer map metadata music
    g_strlcpy(g_level.music, music_map->string, sizeof(g_level.music));
  } else { // or fall back on worldspawn
    const cm_entity_t *music = gi.EntityValue(ent->def, "music");
    if (*music->string) {
      g_strlcpy(g_level.music, music->string, sizeof(g_level.music));
    } else {
      g_level.music[0] = '\0';
    }
  }

  G_worldspawn_Music();
}
