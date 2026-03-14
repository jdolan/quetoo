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
 * @brief Make a tasteless death announcement and record scores.
 */
static void G_ClientObituary(g_client_t *cl, g_entity_t *attacker, uint32_t mod) {
  char buffer[MAX_PRINT_MSG];

  const bool frag = attacker && attacker->client && attacker->client != cl;

  const bool friendly_fire = (mod & MOD_FRIENDLY_FIRE) == MOD_FRIENDLY_FIRE;
  mod &= ~MOD_FRIENDLY_FIRE;

  if (frag) { // killed by another player

    const char *msg = "%s was killed by %s";

    switch (mod) {
      case MOD_BLASTER:
        msg = "%s was humiliated by %s's blaster :blaster:";
        break;
      case MOD_SHOTGUN:
        msg = "%s was gunned down by %s's shotgun :shotgun:";
        break;
      case MOD_SUPER_SHOTGUN:
        msg = "%s was blown away by %s's super shotgun :sshotgun:";
        break;
      case MOD_MACHINEGUN:
        msg = "%s was perforated by %s's machinegun :machinegun:";
        break;
      case MOD_GRENADE:
        msg = "%s was popped by %s's grenade :grenade:";
        break;
      case MOD_GRENADE_SPLASH:
        msg = "%s was shredded by %s's shrapnel :grenade:";
        break;
      case MOD_HANDGRENADE:
        msg = "%s caught %s's handgrenade :handgrenade:";
        break;
      case MOD_HANDGRENADE_SPLASH:
        msg = "%s felt the burn from %s's handgrenade :handgrenade:";
        break;
      case MOD_HANDGRENADE_KAMIKAZE:
        msg = "%s felt %s's pain :handgrenade:";
        break;
      case MOD_ROCKET:
        msg = "%s ate %s's rocket :rocket:";
        break;
      case MOD_ROCKET_SPLASH:
        msg = "%s almost dodged %s's rocket :rocket:";
        break;
      case MOD_HYPERBLASTER:
        msg = "%s was melted by %s's hyperblaster :hyperblaster:";
        break;
      case MOD_LIGHTNING:
        msg = "%s got a charge out of %s's lightning :lightning:";
        break;
      case MOD_LIGHTNING_DISCHARGE:
        msg = "%s was shocked by %s's discharge :lightning:";
        break;
      case MOD_RAILGUN:
        msg = "%s was railed by %s :railgun:";
        break;
      case MOD_BFG_LASER:
        msg = "%s saw the pretty lights from %s's BFG :bfg:";
        break;
      case MOD_BFG_BLAST:
        msg = "%s was disintegrated by %s's BFG blast :bfg:";
        break;
      case MOD_TELEFRAG:
        msg = "%s tried to invade %s's personal space :telefrag:";
        break;
      case MOD_HOOK:
        msg = "%s had their intestines shredded by %s's grappling hook :hook:";
        break;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    g_snprintf(buffer, sizeof(buffer), msg, cl->persistent.net_name, attacker->client->persistent.net_name);
#pragma clang diagnostic pop

    if (friendly_fire) {
      g_strlcat(buffer, " (^1TEAMKILL^7)", sizeof(buffer));
    }

  } else { // killed by self or world

    const char *msg = "%s sucks at life :suicide:";

    switch (mod) {
      case MOD_SUICIDE:
        msg = "%s suicides :suicide:";
        break;
      case MOD_FALLING:
        msg = "%s cratered :falldamage:";
        break;
      case MOD_CRUSH:
        msg = "%s was squished :crush:";
        break;
      case MOD_WATER:
        msg = "%s sleeps with the fishes :drown:";
        break;
      case MOD_SLIME:
        msg = "%s melted :slime:";
        break;
      case MOD_LAVA:
        msg = "%s did a back flip into the lava :lava:";
        break;
      case MOD_FIREBALL:
        msg = "%s tasted the lava rainbow :lava:";
        break;
      case MOD_TRIGGER_HURT:
        msg = "%s was in the wrong place :actofgod:";
        break;
      case MOD_ACT_OF_GOD:
        msg = "%s was killed by an act of god :actofgod:";
        break;
    }

    if (attacker == cl->entity) {
      switch (mod) {
        case MOD_GRENADE_SPLASH:
          msg = "%s went pop :explosive:";
          break;
        case MOD_HANDGRENADE_KAMIKAZE:
          msg = "%s tried to put the pin back in :handgrenade:";
          break;
        case MOD_HANDGRENADE_SPLASH:
          msg = "%s has no hair left :explosive:";
          break;
        case MOD_ROCKET_SPLASH:
          msg = "%s blew up :explosive:";
          break;
        case MOD_HYPERBLASTER_CLIMB:
          msg = "%s forgot how to climb :death:";
          break;
        case MOD_LIGHTNING_DISCHARGE:
          msg = "%s took a toaster bath :death:";
          break;
        case MOD_BFG_BLAST:
          msg = "%s should have used a smaller gun :death:";
          break;
      }
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    g_snprintf(buffer, sizeof(buffer), msg, cl->persistent.net_name);
#pragma clang diagnostic pop
  }

  gi.BroadcastPrint(PRINT_HIGH, "%s\n", buffer);

  if (frag) {

    if (g_show_attacker_stats->integer) {
      int16_t a = 0;
      const g_item_t *armor = G_ClientArmor(attacker->client);
      if (armor) {
        a = attacker->client->inventory[armor->index];
      }
      gi.ClientPrint(cl, PRINT_MEDIUM, "%s had %d health and %d armor\n",
                     attacker->client->persistent.net_name, attacker->health, a);
    }
  }

  if (frag) {
    if (friendly_fire) {
      attacker->client->persistent.score--;
    } else {
      attacker->client->persistent.score++;
    }

    if ((g_level.teams || g_level.ctf) && cl->persistent.team && attacker->client->persistent.team) {
      if (friendly_fire) {
        attacker->client->persistent.team->score--;
      } else {
        attacker->client->persistent.team->score++;
      }
    }
  } else {
    cl->persistent.score--;

    if ((g_level.teams || g_level.ctf) && cl->persistent.team) {
      cl->persistent.team->score--;
    }
  }
}

/**
 * @brief Play a sloppy sound when impacting the world.
 */
static void G_ClientGiblet_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (trace == NULL) {
    return;
  }

  if (G_IsSky(trace)) {
    G_FreeEntity(ent);
  } else {
    const float speed = Vec3_Length(ent->velocity);
    if (speed > 40.0 && G_IsStructural(trace)) {

      if (g_level.time - ent->touch_time > 200) {
        G_MulticastSound(&(const g_play_sound_t) {
          .index = ent->sound,
          .entity = ent,
          .atten = SOUND_ATTEN_SQUARE
        }, MULTICAST_PHS);
        ent->touch_time = g_level.time;
      }
    }
  }
}

/**
 * @brief Sink into the floor after a few seconds, providing a window of time for us to be made into
 * giblets or knocked around. This is called by corpses and giblets alike.
 */
static void G_ClientCorpse_Think(g_entity_t *ent) {

  const uint32_t age = g_level.time - ent->timestamp;

  if (ent->s.model1 == MODEL_CLIENT) {
    if (age > 6000) {
      const int32_t dmg = ent->health;

      if (ent->water_type & CONTENTS_LAVA) {
        G_Damage(&(g_damage_t) {
          .target = ent,
          .inflictor = NULL,
          .attacker = NULL,
          .dir = Vec3_Zero(),
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = dmg,
          .knockback = 0,
          .flags = DMG_NO_ARMOR,
          .mod = MOD_LAVA
        });
      }

      if (ent->water_type & CONTENTS_SLIME) {
        G_Damage(&(g_damage_t) {
          .target = ent,
          .inflictor = NULL,
          .attacker = NULL,
          .dir = Vec3_Zero(),
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = dmg,
          .knockback = 0,
          .flags = DMG_NO_ARMOR,
          .mod = MOD_SLIME
        });
      }
    }
  } else {
    const float speed = Vec3_Length(ent->velocity);

    if (!(ent->s.effects & EF_DESPAWN) && speed > 30.0) {
      ent->s.trail = TRAIL_GIB;
    } else {
      ent->s.trail = TRAIL_NONE;
    }
  }

  if (age > 33000) {
    G_FreeEntity(ent);
    return;
  }

  // sink into the floor after a few seconds
  if (age > 30000) {

    ent->s.effects |= EF_DESPAWN;

    ent->move_type = MOVE_TYPE_NONE;
    ent->take_damage = false;

    ent->solid = SOLID_NOT;

    if (ent->ground.ent) {
      ent->s.origin.z -= QUETOO_TICK_SECONDS * 8.0;
    }

    gi.LinkEntity(ent);
  }

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Corpses explode into giblets when killed. Giblets receive the
 * velocity of the corpse, and bounce when damaged. They eventually sink
 * through the floor and disappear.
 */
static void G_ClientCorpse_Die(g_entity_t *ent, g_entity_t *attacker, uint32_t mod) {

  const box3_t bounds[NUM_GIB_MODELS] = {
    Box3f(12.f, 12.f, 12.f),
    Box3f(12.f, 12.f, 12.f),
    Box3f(8.f, 8.f, 8.f),
    Box3f(16.f, 16.f, 16.f),
  };

  const int32_t count = RandomRangei(4, 8);
  for (int32_t i = 0; i < count; i++) {

    int32_t gib_index;
    if (i == 0) { // 0 is always chest
      gib_index = (NUM_GIB_MODELS - 1);
    } else if (i == 1 && !ent->client) { // if we're not client, drop a head
      gib_index = 2;
    } else { // pick forearm/femur
      gib_index = RandomRangei(0, NUM_GIB_MODELS - 2);
    }

    g_entity_t *gib = G_AllocEntity(__func__);

    gib->s.origin = ent->s.origin;

    gib->bounds = bounds[gib_index];

    gib->solid = SOLID_DEAD;

    gib->s.model1 = g_media.models.gibs[gib_index];
    gib->sound = g_media.sounds.gib_hits[i % NUM_GIB_SOUNDS];

    gib->velocity = ent->velocity;

    const int32_t h = Clampf(-5.0 * gib->health, 100, 500);

    gib->velocity.x += RandomRangef(-h, h);
    gib->velocity.y += RandomRangef(-h, h);
    gib->velocity.z += RandomRangef(100.f, 100.f + h);

    for (int32_t i = 0; i < 3; ++i) {
      gib->avelocity.xyz[i] = RandomRangef(-100.f, 100.f);
      gib->s.angles.xyz[i] = RandomRangef(0, 360.f);
    }

    gib->clip_mask = CONTENTS_MASK_CLIP_CORPSE;
    gib->dead = true;
    gib->mass = (gib_index + 1) * 20.0;
    gib->move_type = MOVE_TYPE_BOUNCE;
    gib->next_think = g_level.time + QUETOO_TICK_MILLIS;
    gib->take_damage = true;
    gib->Think = G_ClientCorpse_Think;
    gib->Touch = G_ClientGiblet_Touch;

    gi.LinkEntity(gib);
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_GIB);
  gi.WritePosition(ent->s.origin);
  gi.Multicast(ent->s.origin, MULTICAST_PVS);

  if (ent->client) {
    ent->bounds = bounds[2];

    const int32_t h = Clampf(-5.0 * ent->health, 100, 500);
    
    ent->velocity.x += RandomRangef(-h, h);
    ent->velocity.y += RandomRangef(-h, h);
    ent->velocity.z += RandomRangef(100.f, 100.f + h);

    for (int32_t i = 0; i < 3; ++i) {
      ent->avelocity.xyz[i] = RandomRangef(-100.f, 100.f);
      ent->s.angles.xyz[i] = RandomRangef(0, 360.f);
    }

    ent->Die = NULL;

    ent->client->ps.pm_state.flags |= PMF_GIBLET;

    ent->s.model1 = g_media.models.gibs[2];

    ent->mass = 20.0;

    ent->solid = SOLID_DEAD;

    gi.LinkEntity(ent);
  } else {
    G_FreeEntity(ent);
  }
}

/**
 * @brief Spawns a corpse for the specified client. The corpse will eventually sink into the floor
 * and disappear if not over-killed.
 */
static void G_ClientCorpse(g_client_t *cl) {

  if (cl->entity->sv_flags & SVF_NO_CLIENT) {
    return;
  }

  if (cl->entity->dead == false) {
    return;
  }

  if (cl->entity->s.model1 != MODEL_CLIENT) {
    return;
  }

  g_entity_t *ent = G_AllocEntity(__func__);

  ent->solid = SOLID_DEAD;

  ent->s.origin = cl->entity->s.origin;
  ent->s.angles = cl->entity->s.angles;

  ent->bounds = cl->entity->bounds;

  ent->s.client = cl->entity->s.client;
  ent->s.model1 = cl->entity->s.model1;

  ent->s.animation1 = cl->entity->s.animation1;
  ent->s.animation2 = cl->entity->s.animation2;

  ent->s.effects = EF_CLIENT | EF_CORPSE;

  ent->velocity = cl->entity->velocity;

  ent->clip_mask = CONTENTS_MASK_CLIP_CORPSE;
  ent->dead = true;
  ent->mass = cl->entity->mass;
  ent->move_type = MOVE_TYPE_BOUNCE;
  ent->take_damage = true;
  ent->health = cl->entity->health;
  ent->Die = ent->health > 0 ? G_ClientCorpse_Die : NULL;
  ent->Think = G_ClientCorpse_Think;
  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;

  gi.LinkEntity(ent);
}

#define CLIENT_CORPSE_HEALTH 80

/**
 * @brief A client's health is less than or equal to zero. Render death effects, drop
 * certain items we're holding and force the client into a temporary spectator
 * state with the scoreboard shown.
 */
static void G_ClientDie(g_entity_t *ent, g_entity_t *attacker, uint32_t mod) {

  g_client_t *cl = ent->client;

  G_ClientObituary(cl, attacker, mod);

  G_HookDetach(cl);

  G_TossQuadDamage(cl);

  if (g_level.gameplay == GAME_DEATHMATCH && mod != MOD_TRIGGER_HURT) {
    G_TossWeapon(cl);
  }

  if (g_level.ctf) {
    G_TossFlag(cl);
  }

  if (g_level.techs) {
    G_TossTech(cl);
  }

  if (ent->health <= -CLIENT_CORPSE_HEALTH) {
    G_ClientCorpse_Die(ent, attacker, mod);
  } else {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = gi.SoundIndex("*death_1"),
      .entity = ent,
      .origin = &ent->s.origin, // send the origin in case of fast respawn
      .atten = SOUND_ATTEN_LINEAR
    }, MULTICAST_PHS);

    const float r = Randomf();
    if (r < 0.33) {
      G_SetAnimation(cl, ANIM_BOTH_DEATH1, true);
    } else if (r < 0.66) {
      G_SetAnimation(cl, ANIM_BOTH_DEATH2, true);
    } else {
      G_SetAnimation(cl, ANIM_BOTH_DEATH3, true);
    }

    ent->bounds.maxs.z = 0.0; // corpses are laying down

    ent->health = CLIENT_CORPSE_HEALTH;
    ent->Die = G_ClientCorpse_Die;
  }

  uint32_t nade_hold_time = cl->grenade_hold_time;

  if (nade_hold_time != 0) {

    G_ClientProjectile(cl, NULL, NULL, NULL, &ent->s.origin, 1.0);

    G_HandGrenadeProjectile(
        ent,          // player
        cl->held_grenade, // the grenade
        ent->s.origin,      // starting point
        Vec3_Up(),        // direction
        0,            // how fast it flies
        120,          // damage dealt
        120,          // knockback
        185.0,          // blast radius
        3000 - (g_level.time - nade_hold_time) // time before explode (next think)
    );
  }

  ent->solid = SOLID_DEAD;
  ent->dead = true;

  ent->s.event = EV_NONE;
  ent->s.effects = EF_CLIENT;
  ent->s.trail = TRAIL_NONE;
  ent->s.model2 = 0;
  ent->s.model3 = 0;
  ent->s.model4 = 0;
  ent->s.sound = 0;

  ent->take_damage = true;

  ent->clip_mask = CONTENTS_MASK_CLIP_CORPSE;
  cl->respawn_time = g_level.time + 1800; // respawn after death animation finishes
  cl->show_scores = true;
  cl->persistent.deaths++;

  G_ClientDamageKick(cl, cl->right, 60.0);

  gi.LinkEntity(ent);
}

/**
 * @brief Stocks client's inventory with specified item. Weapons receive
 * specified quantity of ammo, while health and armor are set to
 * the specified quantity.
 */
static void G_Give(g_client_t *cl, char *it, int16_t quantity) {

  if (!g_ascii_strcasecmp(it, "Health")) {
    cl->entity->health = quantity;
    return;
  }

  const g_item_t *item = G_FindItem(it);

  if (!item) {
    return;
  }

  const uint16_t index = item->index;

  if (item->type == ITEM_WEAPON) { // weapons receive quantity as ammo
    cl->inventory[index]++;

    if (item->ammo) {
      const g_item_t *ammo = item->ammo_item;
      if (ammo) {
        const uint16_t ammo_index = ammo->index;

        if (quantity > -1) {
          cl->inventory[ammo_index] = quantity;
        } else {
          cl->inventory[ammo_index] = ammo->quantity;
        }
      }
    }
  } else { // while other items receive quantity directly
    if (quantity > -1) {
      cl->inventory[index] = quantity;
    } else {
      cl->inventory[index] = item->quantity;
    }
  }
}

/**
 * @brief
 */
static bool G_GiveLevelLocals(g_client_t *cl) {
  char buf[512], *it, *q;
  int32_t quantity;

  if (*g_level.give == '\0') {
    return false;
  }

  g_strlcpy(buf, g_level.give, sizeof(buf));

  it = strtok(buf, ",");

  while (true) {

    if (!it) {
      break;
    }

    it = g_strstrip(it);

    if (*it != '\0') {

      if ((q = strrchr(it, ' '))) {
        quantity = atoi(q + 1);

        if (quantity > -1) { // valid quantity
          *q = '\0';
        }
      } else {
        quantity = -1;
      }

      G_Give(cl, it, quantity);
    }

    it = strtok(NULL, ",");
  }

  return true;
}

/**
 * @brief
 */
static void G_InitClientInventory(g_client_t *cl) {
  const g_item_t *item;

  // instagib gets railgun and slugs, both in normal mode and warmup
  if (g_level.gameplay == GAME_INSTAGIB) {
    G_Give(cl, "Railgun", 1);
    G_Give(cl, "Grenades", 1);
    item = g_media.items.weapons[WEAPON_RAILGUN];
  }
  // arena yields all weapons, health, etc..
  else if (g_level.gameplay == GAME_ARENA) {
    G_Give(cl, "Railgun", 50);
    G_Give(cl, "Lightning Gun", 200);
    G_Give(cl, "Hyperblaster", 200);
    G_Give(cl, "Rocket Launcher", 50);
    G_Give(cl, "Hand Grenades", 1);
    G_Give(cl, "Grenade Launcher", 50);
    G_Give(cl, "Machinegun", 200);
    G_Give(cl, "Super Shotgun", 80);
    G_Give(cl, "Shotgun", 80);
    G_Give(cl, "Blaster", 0);

    G_Give(cl, "Body Armor", -1);

    item = g_media.items.weapons[WEAPON_ROCKET_LAUNCHER];
  }
  // dm gets the blaster
  else {
    G_Give(cl, "Blaster", -1);
    item = g_media.items.weapons[WEAPON_BLASTER];
  }

  // use the best weapon given by the level
  if (G_GiveLevelLocals(cl)) {
    G_UseBestWeapon(cl);
  } else { // or the one given by the gameplay type above
    G_UseWeapon(cl, item);
  }
}

/**
 * @brief Returns the distance to the nearest enemy from the given spot.
 */
static float G_EnemyRangeFromSpot(g_client_t *cl, g_entity_t *spot) {
  float dist, best_dist;
  vec3_t v;

  best_dist = 9999999.0;

  G_ForEachClient(enemy, {
    if (enemy->entity->health <= 0) {
      continue;
    }

    if (enemy->persistent.spectator) {
      continue;
    }

    v = Vec3_Subtract(spot->s.origin, enemy->entity->s.origin);
    dist = Vec3_Length(v);

    if (g_level.teams || g_level.ctf) { // avoid collision with team mates

      if (enemy->persistent.team == cl->persistent.team) {
        if (dist > 64.0) { // if they're far away, ignore them
          continue;
        }
      }
    }

    if (dist < best_dist) {
      best_dist = dist;
    }
  });

  return best_dist;
}

/**
 * @brief Checks if spawning a player in this spot would cause a telefrag.
 */
static bool G_WouldTelefrag(const vec3_t spot) {
  g_entity_t *ents[MAX_ENTITIES];
  box3_t bounds = Box3_Translate(PM_BOUNDS, spot);

  bounds.mins.z -= PM_STEP_HEIGHT;
  bounds.maxs.z += PM_STEP_HEIGHT;

  const size_t len = gi.BoxEntities(bounds, ents, lengthof(ents), BOX_COLLIDE);

  for (size_t i = 0; i < len; i++) {

    if (G_IsMeat(ents[i])) {
      return true;
    }
  }

  return false;
}

/**
 * @brief
 */
static g_entity_t *G_SelectRandomSpawnPoint(const g_spawn_points_t *spawn_points) {
  uint32_t empty_spawns[spawn_points->count];
  uint32_t num_empty_spawns = 0;

  for (uint32_t i = 0; i < spawn_points->count; i++) {

    if (!G_WouldTelefrag(spawn_points->spots[i]->s.origin)) {
      empty_spawns[num_empty_spawns++] = i;
    }
  }

  // none empty, pick randomly
  if (!num_empty_spawns) {

    if (!spawn_points->count) {
      return G_SelectRandomSpawnPoint(&g_level.spawn_points);
    }

    return spawn_points->spots[RandomRangeu(0, spawn_points->count)];
  }

  return spawn_points->spots[empty_spawns[RandomRangeu(0, num_empty_spawns)]];
}

/**
 * @brief
 */
static g_entity_t *G_SelectFarthestSpawnPoint(g_client_t *cl, const g_spawn_points_t *spawn_points) {
  g_entity_t *spot, *best_spot;
  float dist, best_dist;

  spot = best_spot = NULL;
  best_dist = 0.0;

  for (size_t i = 0; i < spawn_points->count; i++) {

    spot = spawn_points->spots[i];
    dist = G_EnemyRangeFromSpot(cl, spot);

    if (dist > best_dist && !G_WouldTelefrag(spot->s.origin)) {
      best_spot = spot;
      best_dist = dist;
    }
  }

  if (best_spot) {
    return best_spot;
  }

  return G_SelectRandomSpawnPoint(spawn_points);
}

/**
 * @brief
 */
static g_entity_t *G_SelectDeathmatchSpawnPoint(g_client_t *cl) {

  if (g_spawn_farthest->value) {
    return G_SelectFarthestSpawnPoint(cl, &g_level.spawn_points);
  }

  return G_SelectRandomSpawnPoint(&g_level.spawn_points);
}

/**
 * @brief
 */
static g_entity_t *G_SelectTeamSpawnPoint(g_client_t *cl) {

  if (!cl->persistent.team) {
    return NULL;
  }

  if (g_spawn_farthest->value) {
    return G_SelectFarthestSpawnPoint(cl, &cl->persistent.team->spawn_points);
  }

  return G_SelectRandomSpawnPoint(&cl->persistent.team->spawn_points);
}

/**
 * @brief Selects the most appropriate spawn point for the given client.
 */
static g_entity_t *G_SelectSpawnPoint(g_client_t *cl) {
  g_entity_t *spawn = NULL;

  if (g_level.teams || g_level.ctf) { // try team spawns first if applicable
    spawn = G_SelectTeamSpawnPoint(cl);
  }

  if (spawn == NULL) { // fall back on DM spawns (e.g CTF games on DM maps)
    spawn = G_SelectDeathmatchSpawnPoint(cl);
  }

  return spawn;
}

/**
 * @brief The grunt work of putting the client into the server on [re]spawn.
 */
static void G_ClientRespawn_(g_client_t *cl) {
  vec3_t delta_angles;

  if (!cl->entity) {
    return;
  }

  G_HookDetach(cl);

  gi.UnlinkEntity(cl->entity);

  // leave a corpse in our place if applicable
  G_ClientCorpse(cl);

  // clear the client, retaining only critical fields
  const g_client_t tmp = *cl;
  memset(cl, 0, sizeof(*cl));

  cl->entity = tmp.entity;
  cl->ps.client = tmp.ps.client;
  cl->ping = tmp.ping;
  cl->in_use = tmp.in_use;
  cl->ai = tmp.ai;
  cl->persistent = tmp.persistent;

  g_entity_t *ent = cl->entity;

  // find a spawn point
  const g_entity_t *spawn = G_SelectSpawnPoint(cl);
  assert(spawn);

  // move to the spawn origin
  ent->s.origin = spawn->s.origin;
  ent->s.origin.z += PM_STEP_HEIGHT;

  // and calculate delta angles
  ent->s.angles = Vec3_Zero();
  cl->angles = spawn->s.angles;
  delta_angles = Vec3_Subtract(spawn->s.angles, tmp.cmd_angles);

  // pack the new origin and delta angles into the player state
  cl->ps.pm_state.origin = ent->s.origin;
  cl->ps.pm_state.delta_angles = delta_angles;
  cl->ps.entity = ent->s.number;

  ent->velocity = Vec3_Zero();

  ent->s.effects = EF_CLIENT;
  ent->s.model1 = 0;
  ent->s.model2 = 0;
  ent->s.model3 = 0;
  ent->s.model4 = 0;

  if (cl->persistent.spectator || editor->value) { // spawn a spectator
    ent->classname = "spectator";

    ent->bounds = Box3_Zero();

    ent->solid = SOLID_NOT;
    ent->sv_flags = SVF_NO_CLIENT;

    ent->move_type = MOVE_TYPE_NO_CLIP;
    ent->dead = true;
    ent->take_damage = false;

    cl->chase_target = NULL;
    cl->weapon = NULL;

    cl->persistent.team = NULL;
  } else { // spawn an active client
    ent->classname = "client";

    ent->solid = SOLID_BOX;
    ent->sv_flags = 0;

    ent->bounds = Box3_Scale(PM_BOUNDS, PM_SCALE);

    ent->s.model1 = MODEL_CLIENT;
    ent->s.event = EV_CLIENT_TELEPORT;

    G_SetAnimation(cl, ANIM_TORSO_STAND1, true);
    G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);

    ent->clip_mask = CONTENTS_MASK_CLIP_PLAYER;

    if (cl->ai) {
      ent->clip_mask = CONTENTS_MASK_CLIP_MONSTER;
    }

    ent->dead = false;
    ent->Die = G_ClientDie;
    memset(&ent->ground, 0, sizeof(ent->ground));
    ent->max_health = 100;
    ent->health = ent->max_health + 5;
    ent->move_type = MOVE_TYPE_WALK;
    ent->mass = 200.0;
    ent->take_damage = true;
    ent->water_level = WATER_UNKNOWN;
    ent->water_type = 0;
    ent->ripple_size = 32.0;

    cl->boost_time = g_level.time + 1000;
    cl->max_armor = 200;
    cl->max_boost_health = ent->max_health + 100;

    // hold in place briefly
    cl->ps.pm_state.flags = PMF_TIME_TELEPORT;
    cl->ps.pm_state.time = 20;

    // setup inventory/weapon
    if (!G_Ai_InDeveloperMode()) {
      G_InitClientInventory(cl);
    }

    // briefly disable firing, since we likely just clicked to respawn
    cl->weapon_fire_time = g_level.time + 250;

    G_KillBox(ent); // kill anyone in our spot
  }

  gi.LinkEntity(ent);
}

/**
 * @brief In this case, voluntary means that the client has explicitly requested
 * a respawn by changing their spectator status.
 */
void G_ClientRespawn(g_client_t *cl, bool voluntary) {

  G_ClientRespawn_(cl);

  // clear scores on voluntary changes
  if (cl->persistent.spectator && voluntary) {
    cl->persistent.score = cl->persistent.deaths = cl->persistent.captures = 0;
  }

  cl->respawn_time = g_level.time;
  cl->respawn_protection_time = g_level.time + g_respawn_protection->value * 1000;

  if (cl->ai) {
    G_Ai_Respawn(cl);
  }

  if (!voluntary) { // don't announce involuntary spectator changes
    return;
  }

  if (cl->persistent.spectator) {
    gi.BroadcastPrint(PRINT_HIGH, "%s likes to watch\n", cl->persistent.net_name);
  } else if (cl->persistent.team) {
    gi.BroadcastPrint(PRINT_HIGH, "%s has joined %s\n", cl->persistent.net_name, cl->persistent.team->name);
  } else {
    gi.BroadcastPrint(PRINT_HIGH, "%s wants some\n", cl->persistent.net_name);
  }
}

/**
 * @brief Called when a client has finished connecting, and is ready
 * to be placed into the game. This will happen every level load.
 */
void G_ClientBegin(g_client_t *cl) {
  char welcome[MAX_STRING_CHARS];

  // setup the client's entity
  cl->entity = G_AllocEntity("client");
  cl->entity->client = cl;
  cl->entity->s.client = cl->ps.client;
  cl->entity->s.effects = EF_CLIENT;
  cl->ps.entity = cl->entity->s.number;

  cl->cmd_angles = Vec3_Zero();
  cl->persistent.first_frame = g_level.frame_num;

  if (editor->value) {
    cl->persistent.spectator = true;
  }
  else {
    if (g_level.teams || g_level.ctf) {
      if (g_auto_join->value || cl->ai) {
        G_AddClientToTeam(cl, G_SmallestTeam()->name);
      } else {
        cl->persistent.spectator = true;
      }
    }
  }

  G_ClientRespawn(cl, true);

  if (g_level.intermission_time) {
    G_ClientToIntermission(cl);
  } else {
    g_snprintf(welcome, sizeof(welcome), "^2Welcome to ^7%s", sv_hostname->string);

    if (*g_motd->string) {
      char motd[MAX_QPATH];
      g_snprintf(motd, sizeof(motd), "\n%s^7", g_motd->string);

      strncat(welcome, motd, sizeof(welcome) - strlen(welcome) - 1);
    }

    strncat(welcome, "\n^2Gameplay is ^1", sizeof(welcome) - strlen(welcome) - 1);
    strncat(welcome, G_GameplayName(g_level.gameplay), sizeof(welcome) - strlen(welcome) - 1);

    if (g_level.teams) {
      strncat(welcome, "\n^2Teams are enabled", sizeof(welcome) - strlen(welcome) - 1);
    }

    if (g_level.ctf) {
      strncat(welcome, "\n^2CTF is enabled", sizeof(welcome) - strlen(welcome) - 1);
    }

    // FIXME: Move these tidbits into ConfigStrings so that the client can display a menu
#if 0
    gi.WriteByte(SV_CMD_CENTER_PRINT);
    gi.WriteString(welcome);
    gi.Unicast(ent, true);
#endif
  }

  // make sure all view stuff is valid
  G_ClientEndFrame(cl);
}

/**
 * @brief Set the hook style of the player, respecting server properties.
 */
void G_SetClientHookStyle(g_client_t *cl) {

  if (!cl->in_use) {
    return;
  }

  g_hook_style_t hook_style;

  // respect user_info on default
  if (!g_strcmp0(g_hook_style->string, "default")) {
    hook_style = G_HookStyleByName(InfoString_Get(cl->persistent.user_info, "hook_style"));
  } else {
    hook_style = G_HookStyleByName(g_hook_style->string);
  }

  cl->persistent.hook_style = hook_style;
}

/**
 * @brief
 */
void G_ClientUserInfoChanged(g_client_t *cl, const char *user_info) {
  char name[MAX_NET_NAME];

  // check for malformed or illegal info strings
  if (!InfoString_Validate(user_info)) {
    printf("invalid\n");
    user_info = DEFAULT_USER_INFO;
  }

  // save off the user_info in case we want to check something later
  const size_t len = strlen(user_info);
  memmove(cl->persistent.user_info, user_info, len + 1);

  G_Debug("%s\n", user_info);

  // set name, use a temp buffer to compute length and crutch up bad names
  const char *s = InfoString_Get(user_info, "name");

  g_strlcpy(name, s, sizeof(name));

  bool color = false;
  char *c = name;
  int32_t i = 0;

  // trim to 15 printable chars
  while (i < MAX_NET_NAME_PRINTABLE) {

    if (!*c) {
      break;
    }

    if (StrIsColor(c)) {
      color = true;
      c += 2;
      continue;
    }

    c++;
    i++;
  }
  name[c - name] = '\0';

  if (!i) { // name had nothing printable
    strcpy(name, "newbie");
  }

  if (color) { // reset to white
    strcat(name, "^7");
  }

  if (strncmp(cl->persistent.net_name, name, sizeof(cl->persistent.net_name))) {

    if (*cl->persistent.net_name != '\0') {
      gi.BroadcastPrint(PRINT_MEDIUM, "%s changed name to %s\n", cl->persistent.net_name, name);
    }

    g_strlcpy(cl->persistent.net_name, name, sizeof(cl->persistent.net_name));
  }

  const g_team_t *team = cl->persistent.team;

  // set skin
  if (team) { // players must use team_skin to change
    s = InfoString_Get(user_info, "skin");

    char *p;
    if (strlen(s) && (p = strchr(s, '/'))) {
      *p = 0;
      s = va("%s/%s", s, DEFAULT_TEAM_SKIN);
    } else {
      s = va("%s/%s", DEFAULT_USER_MODEL, DEFAULT_TEAM_SKIN);
    }
  } else {
    s = InfoString_Get(user_info, "skin");
  }

  if (strlen(s) && !strstr(s, "..")) { // something valid-ish was provided
    g_strlcpy(cl->persistent.skin, s, sizeof(cl->persistent.skin));
  } else {
    g_strlcpy(cl->persistent.skin, DEFAULT_USER_MODEL "/" DEFAULT_USER_SKIN, sizeof(cl->persistent.skin));
  }

  // set effect color
  if (team) { // players must use team_skin to change
    cl->persistent.color = team->color;
  } else {
    s = InfoString_Get(user_info, "color");

    cl->persistent.color = -1;

    if (strlen(s) && strcmp(s, "default")) { // not default
      const int32_t hue = atoi(s);
      if (hue >= 0) {
        cl->persistent.color = Minf(hue, 361);
      }
    }
  }

  // set shirt, pants and head colors

  cl->persistent.shirt.a = 0;
  cl->persistent.pants.a = 0;
  cl->persistent.helmet.a = 0;

  if (team) {

    cl->persistent.shirt = team->shirt;
    cl->persistent.pants = team->pants;
    cl->persistent.helmet = team->helmet;

  } else {

    s = InfoString_Get(user_info, "shirt");
    if (!Color_Parse(s, &cl->persistent.shirt)) {
      cl->persistent.shirt = color_white;
    }

    s = InfoString_Get(user_info, "pants");
    if (!Color_Parse(s, &cl->persistent.pants)) {
      cl->persistent.pants = color_white;
    }

    s = InfoString_Get(user_info, "helmet");
    if (!Color_Parse(s, &cl->persistent.helmet)) {
      cl->persistent.helmet = color_white;
    }
  }

  gchar client_info[MAX_INFO_STRING_STRING] = { '\0' };

  // build the client info string
  g_strlcat(client_info, va("%d", team ? team->id : TEAM_NONE), sizeof(client_info));

  g_strlcat(client_info, "\\", sizeof(client_info));
  g_strlcat(client_info, cl->persistent.net_name, sizeof(client_info));

  g_strlcat(client_info, "\\", sizeof(client_info));
  g_strlcat(client_info, cl->persistent.skin, sizeof(client_info));

  g_strlcat(client_info, "\\", sizeof(client_info));
  g_strlcat(client_info, Color_Unparse(cl->persistent.shirt), sizeof(client_info));

  g_strlcat(client_info, "\\", sizeof(client_info));
  g_strlcat(client_info, Color_Unparse(cl->persistent.pants), sizeof(client_info));

  g_strlcat(client_info, "\\", sizeof(client_info));
  g_strlcat(client_info, Color_Unparse(cl->persistent.helmet), sizeof(client_info));

  g_strlcat(client_info, "\\", sizeof(client_info));
  g_strlcat(client_info, va("%i", cl->persistent.color), sizeof(client_info));

  // send it to clients
  gi.SetConfigString(CS_CLIENTS + cl->ps.client, client_info);

  // set hand, if anything should go wrong, it defaults to 0 (centered)
  cl->persistent.hand = (g_hand_t) strtol(InfoString_Get(user_info, "hand"), NULL, 10);

  if (cl->entity) {
    s = InfoString_Get(user_info, "active");
    if (g_strcmp0(s, "0") == 0) {
      cl->entity->s.effects |= EF_INACTIVE;
    } else {
      cl->entity->s.effects &= ~(EF_INACTIVE);
    }
  }

  // auto-switch
  uint16_t auto_switch = strtoul(InfoString_Get(user_info, "auto_switch"), NULL, 10);
  cl->persistent.auto_switch = auto_switch;

  // hook style
  G_SetClientHookStyle(cl);
}

/**
 * @brief Called when a player begins connecting to the server.
 * The game can refuse entrance to a client by returning false.
 * If the client is allowed, the connection process will continue
 * and eventually get to `G_Begin()`.
 * Changing levels will NOT cause this to be called again.
 */
bool G_ClientConnect(g_client_t *cl, char *user_info) {

  // check password
  if (strlen(g_password->string) && !cl->ai) {
    if (g_strcmp0(g_password->string, InfoString_Get(user_info, "password"))) {
      InfoString_Set(user_info, "rejmsg", "Password required or incorrect.");
      return false;
    }
  }

  memset(&cl->persistent, 0, sizeof(cl->persistent));

  // set name, skin, etc..
  G_ClientUserInfoChanged(cl, user_info);

  cl->in_use = true;

  gi.BroadcastPrint(PRINT_HIGH, "%s connected\n", cl->persistent.net_name);

  int32_t count = 0;
  G_ForEachClient(cl, {
    if (cl->in_use) {
      count++;
    }
  });

  gi.SetConfigString(CS_NUM_CLIENTS, va("%d", count));

  return true;
}

/**
 * @brief Called when a player drops from the server. Not be called between levels.
 */
void G_ClientDisconnect(g_client_t *cl) {

  G_TossQuadDamage(cl);
  G_TossFlag(cl);
  G_TossTech(cl);
  G_HookDetach(cl);

  if (cl->ai) {
    G_Ai_Disconnect(cl);
  }

  gi.BroadcastPrint(PRINT_HIGH, "%s disconnected\n", cl->persistent.net_name);

  if (G_IsMeat(cl->entity)) {
    gi.WriteByte(SV_CMD_MUZZLE_FLASH);
    gi.WriteShort(cl->entity->s.number);
    gi.WriteByte(MZ_LOGOUT);
    gi.Multicast(cl->entity->s.origin, MULTICAST_PHS);
  }

  const uint8_t client = cl->ps.client;
  gi.SetConfigString(CS_CLIENTS + client, "");

  G_FreeEntity(cl->entity);

  memset(cl, 0, sizeof(g_client_t));
  cl->ps.client = client;

  int32_t count = 0;
  G_ForEachClient(cl, {
    if (cl->in_use) {
      count++;
    }
  });

  gi.SetConfigString(CS_NUM_CLIENTS, va("%d", count));
}

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static cm_trace_t G_ClientMove_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {
  const g_entity_t *self = g_level.current_entity;

  return gi.Trace(start, end, bounds, self, self->clip_mask);
}

#ifdef _DEBUG
static bool g_recording_pmove = false;
static bool g_play_pmove = false;
static file_t *g_pmove_file;
static uint64_t pmove_frame = 0;
static uint64_t pmove_frames = 0;

void G_RecordPmove(void) {
  if (g_play_pmove) {
    return;
  }

  if (g_recording_pmove) {
    gi.CloseFile(g_pmove_file);
    g_recording_pmove = false;
    gi.Print("Closing pmove recording\n");
    return;
  }

  g_recording_pmove = true;
  g_pmove_file = gi.OpenFileWrite("pmove.deboog");
  gi.Print("Starting pmove recording\n");
}

void G_PlayPmove(void) {
  if (g_recording_pmove) {
    return;
  }

  if (g_play_pmove) {
    gi.CloseFile(g_pmove_file);
    g_play_pmove = false;
    gi.Print("Closing pmove recording\n");
    return;
  }

  g_play_pmove = true;
  pmove_frames = gi.LoadFile("pmove.deboog", NULL) / sizeof(pm_move_t);
  g_pmove_file = gi.OpenFile("pmove.deboog");
  gi.Print("Starting pmove playback\n");

  if (gi.Argc() > 1) {
    pmove_frame = strtoull(gi.Argv(1), NULL, 10);
    gi.SeekFile(g_pmove_file, sizeof(pm_move_t) * pmove_frame);
  } else {
    pmove_frame = 0;
  }
}
#endif

/**
 * @brief Process the movement command, call Pm_Move and act on the result.
 */
static void G_ClientMove(g_client_t *cl, pm_cmd_t *cmd) {
  vec3_t old_velocity, velocity;

  g_entity_t *ent = cl->entity;

  // save the raw angles sent over in the command
  cl->cmd_angles = cmd->angles;

  // set the move type
  if (ent->move_type == MOVE_TYPE_NO_CLIP) {
    cl->ps.pm_state.type = PM_SPECTATOR;
  } else if (ent->dead) {
    cl->ps.pm_state.type = PM_DEAD;
  } else {
    cl->ps.pm_state.type = PM_NORMAL;
  }

  // copy the current gravity in
  cl->ps.pm_state.gravity = g_level.gravity;

  pm_move_t pm;
  memset(&pm, 0, sizeof(pm));

#ifdef _DEBUG
  if (g_play_pmove) {
    if (!gi.ReadFile(g_pmove_file, &pm, sizeof(pm), 1)) {
      g_play_pmove = false;
      gi.CloseFile(g_pmove_file);
      gi.Print("Finished pmove playback\n");
    } else {
      gi.Print("PMove frame %8" PRIu64 " / %8" PRIu64, pmove_frame, pmove_frames);
      pmove_frame++;
    }
  }

  if (!g_play_pmove) {
#endif
    pm.s = cl->ps.pm_state;

    pm.s.origin = ent->s.origin;

    if (cl->hook_pull) {

      if (cl->persistent.hook_style == HOOK_PULL) {
        pm.s.type = PM_HOOK_PULL;
      } else if (cl->persistent.hook_style == HOOK_SWING_MANUAL) {
        pm.s.type = PM_HOOK_SWING_MANUAL;
      } else if (cl->persistent.hook_style == HOOK_SWING_AUTO) {
        pm.s.type = PM_HOOK_SWING_AUTO;
      } else {
        pm.s.type = PM_HOOK_PULL;
      }
    } else {
      pm.s.velocity = ent->velocity;
    }

    pm.cmd = *cmd;
    pm.ground = ent->ground;
    pm.hook_pull_speed = g_hook_pull_speed->value;
#ifdef _DEBUG
  }
#endif

  pm.PointContents = gi.PointContents;
  pm.BoxContents = gi.BoxContents;
  
  pm.Trace = G_ClientMove_Trace;

  pm.Debug = gi.Debug_;
  pm.DebugMask = gi.DebugMask;
  pm.debug_mask = DEBUG_PMOVE_SERVER;

#ifdef _DEBUG
  if (g_recording_pmove) {
    gi.WriteFile(g_pmove_file, &pm, sizeof(pm), 1);
  }
#endif

  // perform a move
  Pm_Move(&pm);

  // save results of move
  cl->ps.pm_state = pm.s;

  old_velocity = ent->velocity;

  ent->s.step_offset = roundf(pm.s.step_offset);
  ent->s.origin = pm.s.origin;
  ent->velocity = pm.s.velocity;

  ent->bounds = pm.bounds;

  // copy the clamped angles out
  cl->angles = pm.angles;

  // update the directional vectors based on new view angles
  Vec3_Vectors(cl->angles, &cl->forward, &cl->right, &cl->up);

  // update the horizontal speed scalar based on new velocity
  velocity = ent->velocity;
  velocity.z = 0.0;

  velocity = Vec3_NormalizeLength(velocity, &cl->speed);

  // blend animations for live players
  if (ent->dead == false) {

    if (pm.s.flags & PMF_JUMPED) {
      if (g_level.time - 100 > cl->jump_time) {
        vec3_t angles, forward, point;
        cm_trace_t tr;

        angles = Vec3(0.0, ent->s.angles.y, 0.0);
        Vec3_Vectors(angles, &forward, NULL, NULL);

        point = Vec3_Fmaf(ent->s.origin, cl->speed * .4f, velocity);

        // trace towards our jump destination to see if we have room to backflip
        tr = gi.Trace(ent->s.origin, point, ent->bounds, ent, CONTENTS_MASK_CLIP_PLAYER);

        if (Vec3_Dot(velocity, forward) < -0.1 && tr.fraction == 1.0 && cl->speed > 200.0) {
          G_SetAnimation(cl, ANIM_LEGS_JUMP2, true);
        } else {
          G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);
        }

        // landing events take priority over jump events
        if (pm.water_level < WATER_UNDER && ent->s.event != EV_CLIENT_LAND) {
          ent->s.event = EV_CLIENT_JUMP;
        }

        cl->jump_time = g_level.time;
      }
    } else if (pm.s.flags & PMF_TIME_WATER_JUMP) {
      if (g_level.time - 2000 > cl->jump_time) {

        G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);

        ent->s.event = EV_CLIENT_JUMP;
        cl->jump_time = g_level.time;
      }
    } else if (pm.s.flags & PMF_TIME_LAND) {
      if (g_level.time - 800 > cl->land_time) {
        g_entity_event_t event = EV_CLIENT_LAND;

        if (G_IsAnimation(cl, ANIM_LEGS_JUMP2)) {
          G_SetAnimation(cl, ANIM_LEGS_LAND2, true);
        } else {
          G_SetAnimation(cl, ANIM_LEGS_LAND1, true);
        }

        if (old_velocity.z <= PM_SPEED_FALL) { // player will take damage
          int32_t damage = ((int32_t) - ((old_velocity.z - PM_SPEED_FALL) * 0.05));

          damage >>= pm.water_level; // water breaks the fall

          if (damage < 1) {
            damage = 1;
          }

          if (old_velocity.z <= PM_SPEED_FALL_FAR) {
            event = EV_CLIENT_FALL_FAR;
          } else {
            event = EV_CLIENT_FALL;
          }

          cl->pain_time = g_level.time; // suppress pain sound
          
          // TODO: get normal from what we've landed on
          G_Damage(&(g_damage_t) {
            .target = ent,
            .inflictor = NULL,
            .attacker = NULL,
            .dir = Vec3_Up(),
            .point = ent->s.origin,
            .normal = Vec3_Zero(),
            .damage = damage,
            .knockback = 0,
            .flags = DMG_NO_ARMOR,
            .mod = MOD_FALLING
          });
        }

        ent->s.event = event;
        cl->land_time = g_level.time;
      }
    } else if (pm.s.flags & PMF_ON_LADDER) {
      if (g_level.time - 400 > cl->jump_time) {
        if (fabs(ent->velocity.z) > 20.0) {

          G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);

          ent->s.event = EV_CLIENT_JUMP;
          cl->jump_time = g_level.time;
        }
      }
    }

    // detect hitting the ground to help with animation blending
    if (pm.ground.ent && !ent->ground.ent) {
      cl->ground_time = g_level.time;
    }
  }

  // copy ground and water state back into entity
  ent->ground = pm.ground;
  ent->water_level = pm.water_level;
  ent->water_type = pm.water_type;

  // and finally link them back in to collide with others below
  gi.LinkEntity(ent);

  // touch every object we collided with objects
  if (ent->move_type != MOVE_TYPE_NO_CLIP) {

    const cm_trace_t *touched = pm.touched;
    for (int32_t i = 0; i < pm.num_touched; i++, touched++) {
      g_entity_t *other = touched->ent;

      if (!other->Touch) {
        continue;
      }

      other->Touch(other, ent, touched);
    }

    G_TouchOccupy(ent);
  }
}

/**
 * @brief Expire any items which are time-sensitive.
 */
static void G_ClientInventoryThink(g_client_t *cl) {

  if (cl->inventory[g_media.items.powerups[POWERUP_QUAD]->index]) { // if they have quad

    if (cl->quad_countdown_time && cl->quad_countdown_time < g_level.time) { // play the countdown sound      
      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.quad_expire,
        .entity = cl->entity,
        .atten = SOUND_ATTEN_LINEAR
      }, MULTICAST_PHS);
      
      cl->quad_countdown_time += 1000;
    }

    if (cl->quad_damage_time < g_level.time) { // expire it

      cl->quad_damage_time = 0.0;
      cl->inventory[g_media.items.powerups[POWERUP_QUAD]->index] = 0;

      cl->entity->s.effects &= ~EF_QUAD;
    }
  }

  // other power-ups and things can be timed out here as well

  if (cl->respawn_protection_time > g_level.time) {
    cl->entity->s.effects |= EF_RESPAWN;
  } else {
    cl->entity->s.effects &= ~EF_RESPAWN;
  }

  // decrement any boosted health from items
  if (cl->boost_time != 0 && cl->boost_time < g_level.time) {
    if (cl->entity->health > cl->entity->max_health) {
      cl->entity->health -= 1;
      cl->boost_time = g_level.time + 1000;
    } else {
      cl->boost_time = 0;
    }
  }
}

/**
 * @brief This will be called once for each client frame, which will usually be a
 * couple times for each server frame.
 */
void G_ClientThink(g_client_t *cl, pm_cmd_t *cmd) {

  if (g_level.intermission_time) {
    return;
  }

  g_level.current_entity = cl->entity;

  if (cl->chase_target) { // ensure chase is valid
    if (!G_IsMeat(cl->chase_target->entity)) {
      G_ClientChaseNext(cl);
    }
  }

  // setup buttons now, since pmove won't modify them
  cl->old_buttons = cl->buttons;
  cl->buttons = cmd->buttons;
  cl->latched_buttons |= cl->buttons & ~cl->old_buttons;

  if (!cl->chase_target) { // move through the world

    // process hook buttons
    if (cl->hook_think_time < g_level.time) {
      G_HookThink(cl, false);
    }

    G_ClientMove(cl, cmd);
  }

  cl->cmd = *cmd;

  // fire weapon if requested
  if (cl->latched_buttons & BUTTON_ATTACK) {
    if (cl->persistent.spectator) {

      cl->latched_buttons = 0;

      if (cl->chase_target) { // toggle chase camera
        cl->chase_target = cl->old_chase_target = NULL;
      } else {
        G_ClientChaseTarget(cl);
      }

      G_ClientChaseThink(cl);
    } else if (cl->weapon_think_time < g_level.time) {
      G_ClientWeaponThink(cl);
    }
  }

  G_ClientInventoryThink(cl);

  // update anyone chasing us
  G_ForEachClient(chaser, {
    if (chaser->chase_target == cl) {
      G_ClientChaseThink(chaser);
    }
  });

  // if we're the first player in a game, send our client over
  // to the AI system in case it needs to make nodes
  if (cl->ps.client == 0) {
    Ai_Node_PlayerRoam(cl, cmd);
  }
}

/**
 * @brief This will be called once for each server frame, before running
 * any other entities in the world.
 */
void G_ClientBeginFrame(g_client_t *cl) {

  if (g_level.intermission_time) {
    return;
  }

  g_entity_t *ent = cl->entity;

  if ((G_IsMeat(ent) && ent->dead) ||  ((cl->buttons | cl->latched_buttons) & BUTTON_SCORE)) {
    cl->show_scores = true;
  } else {
    cl->show_scores = false;
  }

  // run weapon think if it hasn't been done by a command
  if (cl->weapon_think_time < g_level.time) {
    G_ClientWeaponThink(cl);
  }

  if (cl->hook_think_time < g_level.time) {
    G_HookThink(cl, false);
  }

  if (ent->dead) {
    if (g_level.time > cl->respawn_time) {
      if (cl->latched_buttons & BUTTON_ATTACK) {
        G_ClientRespawn(cl, false);
      }
    }
  } else {
    if (G_HasTech(cl, TECH_REGEN)) {
      if (cl->regen_time < g_level.time) {
        cl->regen_time = g_level.time + TECH_REGEN_TICK_TIME;
        if (ent->health < ent->max_health) {
          ent->health = Minf(ent->health + TECH_REGEN_HEALTH, ent->max_health);
          G_PlayTechSound(cl);
        }
      }
    }
  }

  cl->latched_buttons = 0;
}
