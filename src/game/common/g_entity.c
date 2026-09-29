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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
  void (*Init)(GameEntity *ent);
} GameEntityClass;

static void G_worldspawn(GameEntity *ent);

/**
 * @brief The entity classes.
 */
static const GameEntityClass gameEntityClasses[] = {

  { "func_bob", G_func_bob },
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

  { "misc_portal", G_misc_portal },

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

static const CmEntity *gameMapListEntry;

/**
 * @brief The value `key` holds in the map's own metadata, or `NULL` before it is loaded.
 */
static const CmEntity *G_MapListEntryValue(const char *key) {
  return gameMapListEntry ? gi.EntityValue(gameMapListEntry, key) : NULL;
}

/**
 * @brief Populates the entity fields which are common to all classes from its definition.
 */
static void G_InitEntityFields(GameEntity *ent) {

  ent->s.origin = gi.EntityValue(ent->def, "origin")->vec3;
  ent->s.angles = gi.EntityValue(ent->def, "angles")->vec3;

  const CmEntity *angle = gi.EntityValue(ent->def, "angle");
  if (angle->parsed & ENTITY_FLOAT) {
    ent->s.angles = MakeVec3(0.f, angle->value, 0.f);
  }

  ent->model = gi.EntityValue(ent->def, "model")->nullableString;

  ent->spawnFlags = gi.EntityValue(ent->def, "spawnflags")->integer;

  ent->speed = gi.EntityValue(ent->def, "speed")->value;
  ent->accel = gi.EntityValue(ent->def, "accel")->value;
  ent->decel = gi.EntityValue(ent->def, "decel")->value;

  ent->target = gi.EntityValue(ent->def, "target")->nullableString;
  ent->targetName = gi.EntityValue(ent->def, "targetname")->nullableString;
  ent->message = gi.EntityValue(ent->def, "message")->nullableString;
  ent->team = gi.EntityValue(ent->def, "team")->nullableString;

  ent->wait = gi.EntityValue(ent->def, "wait")->value;
  ent->delay = gi.EntityValue(ent->def, "delay")->value;
  ent->random = gi.EntityValue(ent->def, "random")->value;
  ent->count = gi.EntityValue(ent->def, "count")->integer;

  ent->health = gi.EntityValue(ent->def, "health")->integer;
  ent->damage = gi.EntityValue(ent->def, "dmg")->integer;
  ent->mass = gi.EntityValue(ent->def, "mass")->value;
}

/**
 * @brief The tail of the `G_InitEntity` chain: the items, the weapons'
 * projectiles and the built-in classes.
 */
static bool G_InitEntity_Common(GameEntity *ent) {

  // check item spawn functions
  const GameItem *it = G_FindItemByClassName(ent->classname);
  if (it) {
    G_SpawnItem(ent, it);
    return true;
  }

  // check the ballistics entities, which are one classname per weapon
  if (G_ballistics(ent)) {
    return true;
  }

  // check normal spawn functions
  for (size_t i = 0; i < lengthof(gameEntityClasses); i++) {
    const GameEntityClass *clazz = gameEntityClasses + i;

    if (!q_strcmp(clazz->classname, ent->classname)) {
      clazz->Init(ent);
      return true;
    }
  }

  return false;
}

InitEntity G_InitEntity = G_InitEntity_Common;

/**
 * @brief The tail of the `G_LevelWillSpawn` chain: a notification, so it does nothing.
 */
static void G_LevelWillSpawn_Common(void) {
}

LevelWillSpawn G_LevelWillSpawn = G_LevelWillSpawn_Common;

/**
 * @brief Populates common entity fields and then dispatches the class initializer.
 */
static void G_SpawnEntity(CmEntity *def) {

  const char *classname = gi.EntityValue(def, "classname")->string;
  GameEntity *ent = G_AllocEntity(classname);

  ent->def = def;

  G_InitEntityFields(ent);

  if (G_InitEntity(ent)) {
    return;
  }

  G_Warn("%s doesn't have a spawn function\n", etos(ent));

  G_FreeEntity(ent);
}

/**
 * @brief The entity classes which are fully initialized in editor mode, so that
 * their movement can be previewed. All other classes are left as inert editor
 * placeholders.
 */
static const struct {
  const char *classname;
  bool inlineModel;
} gameEditorEntityClasses[] = {

  { "func_bob", true },
  { "func_button", true },
  { "func_conveyor", true },
  { "func_door", true },
  { "func_door_rotating", true },
  { "func_door_secret", true },
  { "func_plat", true },
  { "func_rotating", true },
  { "func_timer", false },
  { "func_train", true },
};

/**
 * @brief Resolves the class initializer to run for the given editor entity, or `NULL`
 * if it should remain an inert placeholder.
 */
static const GameEntityClass *G_EditorEntityClass(const GameEntity *ent) {

  for (size_t i = 0; i < lengthof(gameEditorEntityClasses); i++) {

    if (q_strcmp(gameEditorEntityClasses[i].classname, ent->classname)) {
      continue;
    }

    if (gameEditorEntityClasses[i].inlineModel) {
      if (!ent->model || ent->model[0] != '*') {
        G_Warn("%s has no inline model\n", etos(ent));
        return NULL;
      }
    }

    for (size_t j = 0; j < lengthof(gameEntityClasses); j++) {
      if (!q_strcmp(gameEntityClasses[j].classname, ent->classname)) {
        return gameEntityClasses + j;
      }
    }

    G_Error("%s has no spawn function\n", etos(ent));
  }

  return NULL;
}

/**
 * @brief Frees the editor entity at the given number, as well as any entities it owns
 * or that share its definition.
 * @details Movers such as doors and platforms allocate their own proximity triggers,
 * which must not outlive them. `G_UseTargets` delay temporaries alias the firing
 * entity's definition, which the server frees once this returns.
 */
void G_FreeEditorEntity(int32_t number) {

  GameEntity *ent = ge.entities[number];

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {

    GameEntity *e = ge.entities[i];

    if (e == ent || !e->inUse || e->client) {
      continue;
    }

    if (e->owner == ent || e->enemy == ent || (ent->def && e->def == ent->def)) {
      G_FreeEntity(e);
    }
  }

  G_FreeEntity(ent);
}

/**
 * @brief Spawns the editor entity at its BSP entity index, so that entity numbers
 * and config string slots remain stable for the editor.
 * @details Movers are fully initialized so that they can be previewed in motion.
 * All other classes become inert placeholders, which the server configures for
 * presentation in `Sv_ConfigureEditorEntity`.
 */
void G_SpawnEditorEntity(int32_t number, CmEntity *def) {

  if (ge.entities[number]->inUse) {
    G_FreeEditorEntity(number);
  }

  const char *classname = gi.EntityValue(def, "classname")->string;
  GameEntity *ent = G_AllocEntityAt(number, classname);

  ent->def = def;

  G_InitEntityFields(ent);

  const GameEntityClass *clazz = G_EditorEntityClass(ent);
  if (clazz) {
    clazz->Init(ent);
  } else {
    ent->solid = SOLID_EDITOR;
    gi.LinkEntity(ent);
  }
}

/**
 * @brief Chain together all entities with a matching team field.
 *
 * All but the first will have the `FL_TEAM_SLAVE` flag set.
 * All but the last will have the `teamNext` field set to the next one.
 */
static void G_InitEntityTeams(void) {

  int32_t teams = 0, teamEntities = 0;

  G_ForEachEntity(ent, {

    if (!ent->team) {
      continue;
    }

    if (ent->flags & FL_TEAM_SLAVE) {
      continue;
    }

    GameEntity *team = ent;
    ent->teamMaster = ent;

    teams++;
    teamEntities++;

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

      if (!q_strcmp(ent->team, e->team)) {

        e->teamMaster = ent;
        e->flags |= FL_TEAM_SLAVE;

        team->teamNext = e;
        team = e;

        teamEntities++;
      }
    });
  });

  G_Debug("%i teams with %i entities\n", teams, teamEntities);
}

/**
 * @brief Resolves references to frequently accessed media.
 */
static void G_InitMedia_Common(void) {
  uint16_t i;

  memset(&gameMedia, 0, sizeof(gameMedia));

  // preload items/set item media ptrs
  G_InitItems();

  // precache player sounds; clients will load these when a new player model gets loaded.
  gi.SoundIndex("*death_1");
  gi.SoundIndex("*death_2");
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

  gameMedia.models.grenade = gi.ModelIndex("models/projectiles/grenade/tris");
  gameMedia.models.quakeGrenade = gi.ModelIndex("models/projectiles/quake_grenade/tris");
  gameMedia.models.quakeNail = gi.ModelIndex("models/projectiles/quake_nail/tris");
  gameMedia.models.rocket = gi.ModelIndex("models/projectiles/rocket/tris");
  gameMedia.models.quakeRocket = gi.ModelIndex("models/projectiles/quake_rocket/tris");
  gameMedia.models.fireball = gi.ModelIndex("models/fireball/tris");
  gameMedia.sounds.bfgHit = gi.SoundIndex("weapons/bfg/hit");
  gameMedia.sounds.bfgPrime = gi.SoundIndex("weapons/bfg/prime");
  gameMedia.sounds.death[0] = gi.SoundIndex("*death_1");
  gameMedia.sounds.death[1] = gi.SoundIndex("*death_2");
  gameMedia.sounds.gasp = gi.SoundIndex("*gasp_1");
  gameMedia.sounds.gurp = gi.SoundIndex("*gurp_1");
  gameMedia.sounds.pain[0] = gi.SoundIndex("*pain25_1");
  gameMedia.sounds.pain[1] = gi.SoundIndex("*pain50_1");
  gameMedia.sounds.pain[2] = gi.SoundIndex("*pain75_1");
  gameMedia.sounds.pain[3] = gi.SoundIndex("*pain100_1");
  gameMedia.sounds.grenadeHit = gi.SoundIndex("projectiles/grenade/hit");
  gameMedia.sounds.grenadeClang = gi.SoundIndex("weapons/handgrenades/hg_clang");
  gameMedia.sounds.grenadeThrow = gi.SoundIndex("weapons/handgrenades/hg_throw");
  gameMedia.sounds.grenadeTick = gi.SoundIndex("weapons/handgrenades/hg_tick.ogg");
  gameMedia.sounds.quakeGrenadeHit = gi.SoundIndex("projectiles/quake_grenade/hit");
  gameMedia.sounds.quakeNailHit = gi.SoundIndex("projectiles/quake_nail/hit");
  gameMedia.sounds.rocketFly = gi.SoundIndex("projectiles/rocket/fly");
  gameMedia.sounds.lightningFly = gi.SoundIndex("weapons/lightning/fly");
  gameMedia.sounds.laserFly = gi.SoundIndex("trigger/laser/fly");
  gameMedia.sounds.quadAttack = gi.SoundIndex("powerups/quad/attack");
  gameMedia.sounds.quadExpire = gi.SoundIndex("powerups/quad/expire");
  gameMedia.sounds.invulnerabilityPickup = gi.SoundIndex("powerups/invulnerability/pickup");
  gameMedia.sounds.invulnerabilityExpire = gi.SoundIndex("powerups/invulnerability/expire");
  gameMedia.sounds.invulnerabilityProtect = gi.SoundIndex("powerups/invulnerability/protect");
  gameMedia.sounds.invisibilityPickup = gi.SoundIndex("powerups/invisibility/pickup");
  gameMedia.sounds.invisibilityExpire = gi.SoundIndex("powerups/invisibility/expire");
  gameMedia.sounds.teleport = gi.SoundIndex("misc/teleport");

  for (i = 0; i < lengthof(gameMedia.sounds.quakeTeleport); i++) {
    gameMedia.sounds.quakeTeleport[i] = gi.SoundIndex(va("misc/quake_teleport%d", i + 1));
  }

  gameMedia.sounds.waterIn = gi.SoundIndex("misc/water_in");
  gameMedia.sounds.waterOut = gi.SoundIndex("misc/water_out");

  gameMedia.sounds.weaponNoAmmo = gi.SoundIndex("weapons/common/no_ammo");
  gameMedia.sounds.weaponSwitch = gi.SoundIndex("weapons/common/switch");

  gameMedia.sounds.chat = gi.SoundIndex("misc/chat");

  for (i = 0; i < NUM_GIB_MODELS; i++) {
    gameMedia.models.gibs[i] = gi.ModelIndex(va("models/gibs/gib_%i/tris", i + 1));
  }

  for (i = 0; i < lengthof(gameMedia.sounds.lava); i++) {
    gameMedia.sounds.lava[i] = gi.SoundIndex(va("ambient/lava_%d", i + 1));
  }

  for (i = 0; i < NUM_GIB_SOUNDS; i++) {
    gameMedia.sounds.gibHits[i] = gi.SoundIndex(va("gibs/gib_%i/hit", i + 1));
  }

  for (i = 1; i < lengthof(gameMedia.sounds.countdown); i++) {
    gameMedia.sounds.countdown[i] = gi.SoundIndex(va("misc/countdown_%d", i));
  }

  gameMedia.sounds.roar = gi.SoundIndex("misc/ominous_bwah");

  gameMedia.images.health = gi.ImageIndex("pics/health");
}

InitMedia G_InitMedia = G_InitMedia_Common;


/**
 * @brief Collects the deathmatch spawn points, falling back on the single
 * player start for maps that provide none.
 */
static Vector *G_InitDeathmatchSpawns(void) {

  Vector *spawns = NULL;

  G_CollectSpawns("info_player_deathmatch", &spawns);

  if (!spawns) {
    G_CollectSpawns("info_player_start", &spawns);
  }

  return spawns;
}

/**
 * @brief Collects each team's spawn points, and the flag it defends where the
 * module has flags. An `info_player_team_any` spawn belongs to every team, and
 * precedes that team's own spawns.
 */
static void G_InitTeamSpawns(Vector *teamSpawns[MAX_TEAMS]) {

#if defined(G_CTF)
  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    gameTeamList[t].flagEntity = G_Find(NULL, EOFS(classname), gameTeamList[t].flag);
  }
#endif

  GameEntity *spot = NULL;

  while ((spot = G_Find(spot, EOFS(classname), "info_player_team_any")) != NULL) {
    for (int32_t t = 0; t < MAX_TEAMS; t++) {
      G_AddSpawn(&teamSpawns[t], spot);
    }
  }

  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    G_CollectSpawns(gameTeamList[t].spawn, &teamSpawns[t]);
  }
}

/**
 * @brief Publishes the collected spawn points and derives the team count.
 * @details A map providing only team spawns still needs a deathmatch set, and
 * conversely a team game on a map with no team spawns draws everyone from the
 * deathmatch set. Most maps have no team spawns at all, so capture play leans
 * on that fallback rather than treating it as an oddity.
 */
static void G_ResolveSpawnPoints(Vector *dmSpawns, Vector *teamSpawns[MAX_TEAMS]) {

  if (!dmSpawns) {
    for (int32_t t = 0; t < MAX_TEAMS; t++) {
      for (uint32_t i = 0; teamSpawns[t] && i < teamSpawns[t]->count; i++) {
        G_AddSpawn(&dmSpawns, VectorValue(teamSpawns[t], GameEntity *, i));
      }
    }

    if (!dmSpawns) {
      G_Error("Map has no spawn points\n");
    }
  }

  G_SetSpawnPoints(&gameLevel.spawnPoints, dmSpawns);
  release(dmSpawns);

  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    G_SetSpawnPoints(&gameTeamList[t].spawnPoints, teamSpawns[t]);

    if (teamSpawns[t]) {
      release(teamSpawns[t]);
    }
  }

  G_InitNumTeams();
}

/**
 * @brief Resolves the spawn points, and the team each belongs to.
 */
static void G_InitSpawnPoints(void) {

  Vector *dmSpawns = G_InitDeathmatchSpawns();

  Vector *teamSpawns[MAX_TEAMS];
  memset(teamSpawns, 0, sizeof(teamSpawns));

  G_InitTeamSpawns(teamSpawns);

  G_ResolveSpawnPoints(dmSpawns, teamSpawns);
}

/**
 * @brief The tail of the `G_ConfigureLevel` chain. Deathmatch reads everything
 * it needs from the worldspawn itself.
 */
static void G_ConfigureLevel_Common(void) {
}

ConfigureLevel G_ConfigureLevel = G_ConfigureLevel_Common;

/**
 * @brief Spawns game entities from the BSP entity definition lump.
 */
void G_SpawnEntities(const char *name, const CmEntity *mapListEntry, CmEntity *const *entities, size_t numEntities) {

  // Drop bots, they will reconnect via G_Ai_Frame
  G_ForEachClient(cl, {
    if (cl->ai) {
      G_ClientDisconnect(cl);
    }
  });

  gameLevel.frags = release(gameLevel.frags);

#if defined(G_CTF)
  gameLevel.captures = release(gameLevel.captures);
#endif

  gi.FreeTag(MEM_TAG_GAME_LEVEL);

  memset(&gameLevel, 0, sizeof(gameLevel));

  q_strlcpy(gameLevel.name, name, sizeof(gameLevel.name));

  G_LevelWillSpawn();

  gameLevel.frags = $(alloc(Vector), initWithSize, sizeof(GameFrag));

#if defined(G_CTF)
  gameLevel.captures = $(alloc(Vector), initWithSize, sizeof(GameCapture));
#endif

  // Clear real client entity pointers before freeing entities to prevent dangling references
  G_ForEachClient(cl, {
    cl->entity = NULL;
#if defined(G_HOOK)
    // the grapple's entities are freed below with all the rest, so the state
    // referring to them must not survive into the next level
    memset(&cl->hook, 0, sizeof(cl->hook));
#endif
    cl->persistent.score = 0;
#if defined(G_CTF)
    cl->persistent.captures = 0;
#endif
    cl->persistent.deaths = 0;
    cl->persistent.team = NULL;
  });

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {
    G_FreeEntity(ge.entities[i]);
  }

  gameMapListEntry = mapListEntry;

  G_InitMedia();

  for (size_t i = 0; i < numEntities; i++) {
    if (editor->value) {
      G_SpawnEditorEntity((int32_t) i, entities[i]);
    } else {
      G_SpawnEntity(entities[i]);
    }
  }

  G_InitEntityTeams();

  G_ConfigureLevel();

  G_ResetTeams();

  if (editor->value) {
    G_info_player_start(G_AllocEntity("info_player_start"));
  }

  G_InitSpawnPoints();

  G_ResetSpawnPoints();

  G_ResetItems();

  G_Ai_Load();

  gameMapListEntry = NULL;
}

/**
 * @brief CompareFunc to shuffle tracks.
 */
int32_t G_worldspawn_MusicShuffle(const ident a, const ident b) {
  return (Order) RandomRangei(-1, 2);
}

/**
 * @brief `Fs_Enumerator` to collect music track names.
 */
static void G_worldspawn_EnumerateMusic(const char *path, void *data) {
  Vector *tracks = (Vector *) data;
  char name[MAX_QPATH];
  StripExtension(Basename(path), name);
  if (q_strcmp(name, "gtdstudio-explore") == 0) {
    return;
  }
  $(tracks, add, name);
}

/**
 * @brief Selects and sets the music tracks for the current level from the music key or available files.
 */
static void G_worldspawn_Music(void) {

  if (*gameLevel.music == '\0') {

    Vector *tracks = $(alloc(Vector), initWithSize, MAX_QPATH);
    gi.EnumerateFiles("music/*.ogg", G_worldspawn_EnumerateMusic, tracks);
    $(tracks, sort, G_worldspawn_MusicShuffle);

    for (size_t i = 0; i < Minz(MAX_MUSICS, tracks->count); i++) {
      gi.SetConfigString(CS_MUSICS + (int32_t) i, (char *) tracks->elements + i * MAX_QPATH);
    }

    release(tracks);
    return;
  }

  char buf[MAX_STRING_CHARS];
  q_strlcpy(buf, gameLevel.music, sizeof(buf));

  int32_t i = 0;
  char *t = strtok(buf, ",");

  while (true) {

    if (!t) {
      break;
    }

    if (i == MAX_MUSICS) {
      break;
    }

    while (isspace((unsigned char) *t)) { t++; }
    char *_end = t + q_strlen(t) - 1;
    while (_end >= t && isspace((unsigned char) *_end)) { *_end-- = '\0'; }

    if (*t != '\0') {
      gi.SetConfigString(CS_MUSICS + i++, t);
    }

    t = strtok(NULL, ",");
  }
}

/*QUAKED worldspawn (0 0 0) ?
 The worldspawn entity defines global conditions and behavior for the entire level. All brushes not belonging to an explicit entity implicitly belong to worldspawn.

The map list's entry for the level takes precedence over the gameplay, movement, gravity, limits and music given here. Explicit server settings take precedence for gameplay, movement and gravity; limits use these values before falling back to server defaults.
 -------- KEYS --------
 message : The map title.
 games : Space-delimited games this map is made for, one or more of dm, tdm, duel, ctf, instagib, race (default dm). Shown and filtered in the Create Server map browser.
 sky : The sky environment map (default template).
 ambient : The ambient light level, as one scalar or as three to tint it (e.g. 0.14 0.11 0.12).
 gravity : Gravity for the level; unset, the movement's applies, 800 for most.
 gameplay : The gameplay mode, one of deathmatch, team_deathmatch, instagib, team_instagib, arena, team_arena (default deathmatch).
 movement : The player movement, one of quetoo, race, quake, quake2, quake3 (default quetoo, or race in the race module).
 items : The item set for this map: "default" (Quetoo weapons) or "quake" (Quake weapons).
 frag_limit : The frag limit (default g_fragLimit).
 capture_limit : The capture limit, in CTF (default g_captureLimit).
 time_limit : The time limit in minutes (default g_timeLimit).
 music : A comma-delimited list of music tracks; unset, all tracks are shuffled.
 author : The creator of the map, and any other credits. Metadata only: the game does not read it.
 version : The version of the map, as major.minor.build. Metadata only: the game does not read it.
 */
static void G_worldspawn(GameEntity *ent) {

  ent->solid = SOLID_BSP;
  ent->moveType = MOVE_TYPE_NONE;
  ent->s.effects = EF_WORLD;

  // ensure worldspawn has no origin or angles, as these would cause the
  // entire world model to be rendered offset or rotated as an entity
  ent->s.origin = Vec3_Zero();
  ent->s.angles = Vec3_Zero();

  gi.SetModel(ent, "*0");

  ent->s.bounds = ent->bounds;

  if (ent->message && *ent->message) {
    q_strlcpy(gameLevel.message, ent->message, sizeof(gameLevel.message));
  } else {
    q_strlcpy(gameLevel.message, gameLevel.name, sizeof(gameLevel.message));
  }

  gi.SetConfigString(CS_MESSAGE, gameLevel.message);

  const CmEntity *gravityMap = G_MapListEntryValue("gravity");
  if (q_strcmp(g_gravity->string, g_gravity->defaultString)) { // prefer an explicit g_gravity override
    gameLevel.gravity = g_gravity->integer;
  } else if (gravityMap && (gravityMap->parsed & ENTITY_INTEGER) && gravityMap->integer > 0) { // then map metadata gravity
    gameLevel.gravity = gravityMap->integer;
  } else { // or worldspawn; unset, it stays zero, and the movement's gravity applies
    const CmEntity *gravity = gi.EntityValue(ent->def, "gravity");
    if (gravity->parsed & ENTITY_INTEGER) {
      if (gravity->integer) {
        gameLevel.gravity = gravity->integer;
      } else {
        G_Warn("%s has gravity 0, which is unset: the movement's applies\n", gameLevel.name);
      }
    }
  }

  // clear the flag the cvar carried from creation so the first G_CheckRules frame
  // can't overwrite the resolved value with g_gravity's default (runtime changes still apply)
  g_gravity->modified = false;

  // the gameplay is g_gameplay if the admin named one, else this level's
  // metadata, else its worldspawn, else deathmatch, as the movement is below
  const CmEntity *gameplayMap = G_MapListEntryValue("gameplay");
  const char *gameplay = (gameplayMap && (gameplayMap->parsed & ENTITY_INTEGER) && gameplayMap->integer > -1)
                         ? G_GameplayById(gameplayMap->integer)->name
                         : gi.EntityValue(ent->def, "gameplay")->string;

  gameLevel.gameplay = G_ResolveGameplay(gameplay);

  gi.SetConfigString(CS_GAMEPLAY, va("%d", gameLevel.gameplay));

  const CmEntity *items = gi.EntityValue(ent->def, "items");
  if (q_strcasecmp(items->string, "quake") == 0) {
    gameLevel.items = ITEMS_QUAKE;
  } else {
    gameLevel.items = ITEMS_DEFAULT;
  }

  gi.SetConfigString(CS_ITEM_SET, va("%d", gameLevel.items));

  // the movement takes the same precedence gameplay does: what the admin asked
  // for, else this level's metadata, else its worldspawn, else Quetoo's. It needs
  // no config string, because it reaches the client inside the movement
  // parameters, which are networked per-player
  const CmEntity *movementMap = G_MapListEntryValue("movement");
  const char *movement = (movementMap && *movementMap->string)
                         ? movementMap->string
                         : gi.EntityValue(ent->def, "movement")->string;

  gameLevel.movement = G_ResolveMovement(movement);

  gi.Print("  Movement:   ^2%s^7\n", Pm_Movement(gameLevel.movement)->name);

  gameLevel.teams = (gameLevel.gameplay & GAMEPLAY_TEAMS) != 0;

  if (q_strcmp(g_numTeams->string, "default")) {
    gameLevel.numTeams = Clampf(g_numTeams->integer, 2, MAX_TEAMS);
  } else {
    gameLevel.numTeams = -1; // G_InitSpawnPoints derives it from the spawn points
  }

  const CmEntity *minClientsMap = G_MapListEntryValue("min_clients");
  if (minClientsMap && (minClientsMap->parsed & ENTITY_INTEGER) && minClientsMap->integer > -1) {
    gameLevel.minClientsMap = minClientsMap->integer;
  } else {
    gameLevel.minClientsMap = -1;
  }

  const CmEntity *fragLimitMap = G_MapListEntryValue("frag_limit");
  if (fragLimitMap && (fragLimitMap->parsed & ENTITY_INTEGER) && fragLimitMap->integer > -1) { // prefer map metadata fragLimit
    gameLevel.fragLimit = fragLimitMap->integer;
  } else { // or fall back on worldspawn
    const CmEntity *fragLimit = gi.EntityValue(ent->def, "frag_limit");
    if (fragLimit->parsed & ENTITY_INTEGER) {
      gameLevel.fragLimit = fragLimit->integer;
    } else {
      gameLevel.fragLimit = g_fragLimit->integer;
    }
  }

#if defined(G_CTF)
  const CmEntity *captureLimitMap = G_MapListEntryValue("capture_limit");
  if (captureLimitMap && (captureLimitMap->parsed & ENTITY_INTEGER) && captureLimitMap->integer > -1) { // prefer map metadata captureLimit
    gameLevel.captureLimit = captureLimitMap->integer;
  } else { // or fall back on worldspawn
    const CmEntity *captureLimit = gi.EntityValue(ent->def, "capture_limit");
    if (captureLimit->parsed & ENTITY_INTEGER) {
      gameLevel.captureLimit = captureLimit->integer;
    } else {
      gameLevel.captureLimit = g_captureLimit->integer;
    }
  }
#endif

  float minutes;
  const CmEntity *timeLimitMap = G_MapListEntryValue("time_limit");
  if (timeLimitMap && (timeLimitMap->parsed & ENTITY_FLOAT) && timeLimitMap->value > -1.f) { // prefer map metadata timeLimit
    minutes = timeLimitMap->value;
  } else { // or fall back on worldspawn
    const CmEntity *timeLimit = gi.EntityValue(ent->def, "time_limit");
    if (timeLimit->parsed & ENTITY_FLOAT) {
      minutes = timeLimit->value;
    } else {
      minutes = g_timeLimit->value;
    }
  }
  gameLevel.timeLimit = minutes * 60 * 1000;

  const CmEntity *musicMap = G_MapListEntryValue("music");
  if (musicMap && *musicMap->string) { // prefer map metadata music
    q_strlcpy(gameLevel.music, musicMap->string, sizeof(gameLevel.music));
  } else { // or fall back on worldspawn
    const CmEntity *music = gi.EntityValue(ent->def, "music");
    if (*music->string) {
      q_strlcpy(gameLevel.music, music->string, sizeof(gameLevel.music));
    } else {
      gameLevel.music[0] = '\0';
    }
  }

  G_worldspawn_Music();
}
