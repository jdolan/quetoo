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
 * @brief Returns true if the given means of death should detach the view and
 * play a death camera.
 *
 * @remarks This is a MOD-based heuristic for deaths that are typically more
 * interesting to watch in third person (environmental / self-inflicted / indirect
 * damage), rather than from the first person.
 */
static bool G_IsDeathCamMod(uint32_t mod) {

  if (g_deathCam->integer == 2) {
    return true;
  }

  switch (mod & ~MOD_FRIENDLY_FIRE) {
    case MOD_UNKNOWN:
    case MOD_QUAKE_THUNDERBOLT_DISCHARGE:
    case MOD_LIGHTNING_DISCHARGE:
    case MOD_WATER:
    case MOD_SLIME:
    case MOD_LAVA:
    case MOD_CRUSH:
#if defined(G_HOOK)
    case MOD_HOOK:
#endif
    case MOD_TELEFRAG:
    case MOD_FALLING:
    case MOD_SUICIDE:
    case MOD_EXPLOSIVE:
    case MOD_TRIGGER_HURT:
    case MOD_HANDGRENADE_SUICIDE:
    case MOD_FIREBALL:
    case MOD_ACT_OF_GOD:
    case MOD_BOB:
    case MOD_BALLISTICS_BLASTER:
    case MOD_BALLISTICS_SHOTGUN:
    case MOD_BALLISTICS_SUPER_SHOTGUN:
    case MOD_BALLISTICS_MACHINEGUN:
    case MOD_BALLISTICS_GRENADE:
    case MOD_BALLISTICS_ROCKET:
    case MOD_BALLISTICS_HYPERBLASTER:
    case MOD_BALLISTICS_LIGHTNING:
    case MOD_BALLISTICS_RAILGUN:
    case MOD_BALLISTICS_BFG:
    case MOD_BALLISTICS_QUAKE_SHOTGUN:
    case MOD_BALLISTICS_QUAKE_SUPER_SHOTGUN:
    case MOD_BALLISTICS_QUAKE_NAILGUN:
    case MOD_BALLISTICS_QUAKE_SUPER_NAILGUN:
    case MOD_BALLISTICS_QUAKE_GRENADE:
    case MOD_BALLISTICS_QUAKE_ROCKET:
    case MOD_BALLISTICS_QUAKE_THUNDERBOLT:
    case MOD_BALLISTICS_LASER:
    case MOD_BALLISTICS_GIBLETS:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Make a tasteless death announcement and record scores.
 */
static void G_ClientObituary(GameClient *cl, GameEntity *attacker, uint32_t mod) {
  char buffer[MAX_PRINT_MSG];

  const bool frag = attacker && attacker->client && attacker->client != cl;

  const bool friendlyFire = (mod & MOD_FRIENDLY_FIRE) == MOD_FRIENDLY_FIRE;
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
      case MOD_QUAKE_SHOTGUN:
        msg = "%s chewed on %s's boomstick :shotgun:";
        break;
      case MOD_QUAKE_SUPER_SHOTGUN:
        msg = "%s ate 2 loads of %s's buckshot :sshotgun:";
        break;
      case MOD_QUAKE_NAILGUN:
        msg = "%s was nailed by %s :nailgun:";
        break;
      case MOD_QUAKE_SUPER_NAILGUN:
        msg = "%s was punctured by %s :nailgun:";
        break;
      case MOD_QUAKE_GRENADE:
        msg = "%s ate %s's pineapple :grenade:";
        break;
      case MOD_QUAKE_GRENADE_SPLASH:
        msg = "%s was gibbed by %s's grenade :grenade:";
        break;
      case MOD_QUAKE_ROCKET:
        msg = "%s rides %s's rocket :rocket:";
        break;
      case MOD_QUAKE_ROCKET_SPLASH:
        msg = "%s was gibbed by %s's rocket :rocket:";
        break;
      case MOD_QUAKE_THUNDERBOLT:
        if (attacker->waterLevel > WATER_NONE) {
          msg = "%s accepts %s's discharge :lightning:";
        } else {
          msg = "%s accepts %s's shaft :lightning:";
        }
        break;
      case MOD_QUAKE_THUNDERBOLT_DISCHARGE:
        msg = "%s accepts %s's discharge :lightning:";
        break;
      case MOD_TURRET_BLASTER:
        msg = "%s was lit up by %s's turret :blaster:";
        break;
      case MOD_TURRET_SHOTGUN:
        msg = "%s was peppered by %s's turret :shotgun:";
        break;
      case MOD_TURRET_SUPER_SHOTGUN:
        msg = "%s was blown apart by %s's turret :sshotgun:";
        break;
      case MOD_TURRET_MACHINEGUN:
        msg = "%s was mowed down by %s's turret :machinegun:";
        break;
      case MOD_TURRET_GRENADE:
        msg = "%s caught %s's care package :grenade:";
        break;
      case MOD_TURRET_ROCKET:
        msg = "%s was shelled by %s's turret :rocket:";
        break;
      case MOD_TURRET_HYPERBLASTER:
        msg = "%s was melted by %s's turret :hyperblaster:";
        break;
      case MOD_TURRET_LIGHTNING:
        msg = "%s was electrified by %s's turret :lightning:";
        break;
      case MOD_TURRET_RAILGUN:
        msg = "%s was traced by %s's turret :railgun:";
        break;
      case MOD_TURRET_BFG:
        msg = "%s was annihilated by %s's turret :bfg:";
        break;
      case MOD_TURRET_QUAKE_SHOTGUN:
        msg = "%s ate buckshot from %s's turret :shotgun:";
        break;
      case MOD_TURRET_QUAKE_SUPER_SHOTGUN:
        msg = "%s was torn open by %s's turret :sshotgun:";
        break;
      case MOD_TURRET_QUAKE_NAILGUN:
        msg = "%s was stapled down by %s's turret :nailgun:";
        break;
      case MOD_TURRET_QUAKE_SUPER_NAILGUN:
        msg = "%s was riddled by %s's turret :nailgun:";
        break;
      case MOD_TURRET_QUAKE_GRENADE:
        msg = "%s was bounced a present by %s's turret :grenade:";
        break;
      case MOD_TURRET_QUAKE_ROCKET:
        msg = "%s was launched by %s's turret :rocket:";
        break;
      case MOD_TURRET_QUAKE_THUNDERBOLT:
        msg = "%s took the full charge of %s's turret :lightning:";
        break;
      case MOD_TURRET_LASER:
        msg = "%s was burned through by %s's turret :bfg:";
        break;
      case MOD_TURRET_GIBLETS:
        msg = "%s was fed %s's leftovers :death:";
        break;
      case MOD_TELEFRAG:
        msg = "%s tried to invade %s's personal space :telefrag:";
        break;
#if defined(G_HOOK)
      case MOD_HOOK:
        msg = "%s had their intestines shredded by %s's grappling hook :hook:";
        break;
#endif
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    q_snprintf(buffer, sizeof(buffer), msg, cl->persistent.netName, attacker->client->persistent.netName);
#pragma clang diagnostic pop

    if (friendlyFire) {
      q_strlcat(buffer, " (^1TEAMKILL^7)", sizeof(buffer));
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
      case MOD_BOB:
        msg = "%s was crushed by Bob. Not the builder. :crush:";
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
      case MOD_BALLISTICS_BLASTER:
        msg = "%s got a face full of trap :blaster:";
        break;
      case MOD_BALLISTICS_SHOTGUN:
        msg = "%s walked into the spray :shotgun:";
        break;
      case MOD_BALLISTICS_SUPER_SHOTGUN:
        msg = "%s was shredded at close range :sshotgun:";
        break;
      case MOD_BALLISTICS_MACHINEGUN:
        msg = "%s was ventilated :machinegun:";
        break;
      case MOD_BALLISTICS_GRENADE:
        msg = "%s stepped on it :grenade:";
        break;
      case MOD_BALLISTICS_ROCKET:
        msg = "%s should have taken the other tunnel :rocket:";
        break;
      case MOD_BALLISTICS_HYPERBLASTER:
        msg = "%s was cooked in the corridor :hyperblaster:";
        break;
      case MOD_BALLISTICS_LIGHTNING:
        msg = "%s completed the circuit :lightning:";
        break;
      case MOD_BALLISTICS_RAILGUN:
        msg = "%s was cut down to size :railgun:";
        break;
      case MOD_BALLISTICS_BFG:
        msg = "%s should not have opened that door :bfg:";
        break;
      case MOD_BALLISTICS_QUAKE_SHOTGUN:
        msg = "%s was greeted with buckshot :shotgun:";
        break;
      case MOD_BALLISTICS_QUAKE_SUPER_SHOTGUN:
        msg = "%s was met at the door :sshotgun:";
        break;
      case MOD_BALLISTICS_QUAKE_NAILGUN:
        msg = "%s was nailed to the wall :nailgun:";
        break;
      case MOD_BALLISTICS_QUAKE_SUPER_NAILGUN:
        msg = "%s was perforated :nailgun:";
        break;
      case MOD_BALLISTICS_QUAKE_GRENADE:
        msg = "%s watched it bounce closer :grenade:";
        break;
      case MOD_BALLISTICS_QUAKE_ROCKET:
        msg = "%s never heard it coming :rocket:";
        break;
      case MOD_BALLISTICS_QUAKE_THUNDERBOLT:
        msg = "%s was left twitching :lightning:";
        break;
      case MOD_BALLISTICS_LASER:
        msg = "%s walked into the beam :bfg:";
        break;
      case MOD_BALLISTICS_GIBLETS:
        msg = "%s was pelted to death with meat :death:";
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
        case MOD_QUAKE_GRENADE_SPLASH:
          msg = "%s tries to put the pin back in :grenade:";
          break;
        case MOD_QUAKE_ROCKET_SPLASH:
          msg = "%s becomes bored with life :rocket:";
          break;
        case MOD_QUAKE_THUNDERBOLT_DISCHARGE:
          switch (cl->entity->waterType) {
            case CONTENTS_SLIME:
              msg = "%s discharges into the slime :slime:";
              break;
            case CONTENTS_LAVA:
              msg = "%s discharges into the lava :lava:";
              break;
            default:
              msg = "%s discharges into the water :drown:";
              break;
          }
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
    q_snprintf(buffer, sizeof(buffer), msg, cl->persistent.netName);
#pragma clang diagnostic pop
  }

  gi.BroadcastPrint(PRINT_HIGH, "%s\n", buffer);

  if (frag) {

    if (g_showAttackerStats->integer) {
      int16_t a = 0;
      const GameItem *armor = G_ClientArmor(attacker->client);
      if (armor) {
        a = attacker->client->inventory[armor->def.tag];
      }
      gi.ClientPrint(cl, PRINT_MEDIUM, "%s had %d health and %d armor\n",
                     attacker->client->persistent.netName, attacker->health, a);
    }
  }

  if (frag) {
    if (friendlyFire) {
      attacker->client->persistent.score--;
    } else {
      attacker->client->persistent.score++;
    }

    if (gLevel.teams && cl->persistent.team && attacker->client->persistent.team) {
      if (friendlyFire) {
        attacker->client->persistent.team->score--;
      } else {
        attacker->client->persistent.team->score++;
      }
    }
  } else {
    cl->persistent.score--;

    if (gLevel.teams && cl->persistent.team) {
      cl->persistent.team->score--;
    }
  }
}

/**
 * @brief Play a sloppy sound when impacting the world.
 */
static void G_ClientGiblet_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  // G_TouchOccupy passes no trace, and is the only way a giblet reaches a player
  if (ent->damage && other->client && other != ent->owner && other->takeDamage) {
    if ((uint32_t) ent->count <= gLevel.time) {
      ent->count = gLevel.time + 500;

      G_Damage(&(GameDamage) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = ent->velocity,
        .point = ent->s.origin,
        .normal = trace ? trace->plane.normal : Vec3_Zero(),
        .damage = ent->damage,
        .knockback = ent->knockback,
        .mod = ent->mod
      });
    }
  }

  if (trace == NULL) {
    return;
  }

  if (G_IsSky(trace)) {
    G_FreeEntity(ent);
  } else {
    const float speed = Vec3_Length(ent->velocity);
    if (speed > 40.0 && G_IsStructural(trace)) {

      if (gLevel.time - ent->touchTime > 200) {
        G_MulticastSound(&(const GamePlaySound) {
          .index = ent->sound,
          .entity = ent,
        }, MULTICAST_PHS);
        ent->touchTime = gLevel.time;
      }
    }
  }
}

/**
 * @brief How long a corpse and its giblets remain before they begin to despawn.
 */
#define CORPSE_LIFETIME 30000

/**
 * @brief The window a corpse or giblet spends fading out once its time is up, which must outlast
 * the three seconds the client takes to fade it.
 */
#define DESPAWN_TIME 4000

/**
 * @brief Think for giblets spawned with an explicit lifetime. Bleeds while in flight, then sinks
 * and fades from the deadline held in `timestamp`, and frees itself once faded.
 * @remarks Giblets only bleed while their think is running, so giblets given a lifetime would
 * otherwise fly clean; the trail is assigned by `G_ClientCorpse_Think`, which they never run,
 * along with the despawn this mirrors.
 */
static void G_Giblet_Think(GameEntity *ent) {

  if (gLevel.time >= ent->timestamp) {

    if (gLevel.time >= ent->timestamp + DESPAWN_TIME) {
      G_FreeEntity(ent);
      return;
    }

    // sink and fade out as a corpse's giblets do, rather than blinking out of the world
    ent->s.effects |= EF_DESPAWN;
    ent->s.trail = TRAIL_NONE;

    ent->moveType = MOVE_TYPE_NONE;
    ent->takeDamage = false;
    ent->solid = SOLID_NOT;

    if (ent->ground.ent) {
      ent->s.origin.z -= QUETOO_TICK_SECONDS * 4.f;
    }

    gi.LinkEntity(ent);
  } else {
    if (Vec3_Length(ent->velocity) > 30.f) {
      ent->s.trail = TRAIL_GIB;
    } else {
      ent->s.trail = TRAIL_NONE;
    }
  }

  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Sink into the floor after a few seconds, providing a window of time for us to be made into
 * giblets or knocked around. This is called by corpses and giblets alike.
 */
static void G_ClientCorpse_Think(GameEntity *ent) {

  const uint32_t age = gLevel.time - ent->timestamp;

  if (ent->s.model1 == MODEL_CLIENT) {
    if (age > 6000) {
      const int32_t dmg = ent->health;

      if (ent->waterType & CONTENTS_LAVA) {
        G_Damage(&(GameDamage) {
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

      if (ent->waterType & CONTENTS_SLIME) {
        G_Damage(&(GameDamage) {
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

  if (age > CORPSE_LIFETIME + DESPAWN_TIME) {
    G_FreeEntity(ent);
    return;
  }

  // sink into the floor after a while
  if (age > CORPSE_LIFETIME) {

    ent->s.effects |= EF_DESPAWN;

    ent->moveType = MOVE_TYPE_NONE;
    ent->takeDamage = false;

    ent->solid = SOLID_NOT;

    if (ent->ground.ent) {
      ent->s.origin.z -= QUETOO_TICK_SECONDS * 4.f;
    }

    gi.LinkEntity(ent);
  }

  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Fires a ragdoll jolt event; the client handles all animation locally.
 */
static void G_ClientCorpse_Pain(GameEntity *ent, GameEntity *attacker, int16_t damage, int16_t knockback) {
  ent->s.event = EV_CLIENT_RAGDOLL;
}

/**
 * @brief Scatters a burst of giblets from the given origin, inheriting the given velocity.
 * Giblets bounce when damaged, and eventually sink through the floor and disappear. When
 * `damage` is set they also injure whatever they strike, credited to `attacker`.
 */
void G_Giblets(const GameGiblets *giblets) {

  const Box3 bounds[NUM_GIB_MODELS] = {
    Box3f(12.f, 12.f, 12.f),
    Box3f(12.f, 12.f, 12.f),
    Box3f(8.f, 8.f, 8.f),
    Box3f(16.f, 16.f, 16.f),
  };

  for (int32_t i = 0; i < giblets->count; i++) {

    int32_t gibIndex;
    if (i == 0) { // 0 is always chest
      gibIndex = (NUM_GIB_MODELS - 1);
    } else if (i == 1 && giblets->head) {
      gibIndex = 2;
    } else { // pick forearm/femur
      gibIndex = RandomRangei(0, NUM_GIB_MODELS - 2);
    }

    GameEntity *gib = G_AllocEntity(__func__);

    gib->s.origin = giblets->origin;

    gib->bounds = bounds[gibIndex];

    gib->solid = giblets->damage ? SOLID_TRIGGER : SOLID_DEAD;

    gib->s.model1 = gMedia.models.gibs[gibIndex];
    gib->sound = gMedia.sounds.gibHits[i % NUM_GIB_SOUNDS];

    gib->velocity = giblets->velocity;

    const int32_t h = Clampf(-5.0 * gib->health, 100, 500);

    gib->velocity.x += RandomRangef(-h, h);
    gib->velocity.y += RandomRangef(-h, h);
    gib->velocity.z += RandomRangef(giblets->lift, giblets->lift + h);

    for (int32_t i = 0; i < 3; ++i) {
      gib->avelocity.xyz[i] = RandomRangef(-100.f, 100.f);
      gib->s.angles.xyz[i] = RandomRangef(0, 360.f);
    }

    gib->clipMask = CONTENTS_MASK_CLIP_CORPSE;
    gib->dead = true;
    gib->mass = (gibIndex + 1) * 20.0;
    gib->moveType = MOVE_TYPE_BOUNCE;
    gib->takeDamage = true;
    gib->owner = giblets->attacker;
    gib->damage = giblets->damage;
    gib->knockback = giblets->knockback;
    gib->mod = giblets->mod;
    gib->Touch = G_ClientGiblet_Touch;

    if (giblets->lifetime) {
      gib->timestamp = gLevel.time + giblets->lifetime;
      gib->Think = G_Giblet_Think;
      gib->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
    } else {
      gib->Think = G_ClientCorpse_Think;
      gib->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
    }

    gi.LinkEntity(gib);
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_GIB);
  gi.WritePosition(giblets->origin);
  gi.Multicast(giblets->origin, MULTICAST_PVS);
}

/**
 * @brief Corpses explode into giblets when killed. Giblets receive the
 * velocity of the corpse, and bounce when damaged. They eventually sink
 * through the floor and disappear.
 */
static void G_ClientCorpse_Die(GameEntity *ent, GameEntity *attacker, uint32_t mod) {

  G_Giblets(&(const GameGiblets) {
    .origin = ent->s.origin,
    .velocity = ent->velocity,
    .count = RandomRangei(4, 8),
    .head = ent->client == NULL,
    .lift = 100.f,
  });

  if (ent->client) {
    ent->bounds = Box3f(8.f, 8.f, 8.f);

    const int32_t h = Clampf(-5.0 * ent->health, 100, 500);
    
    ent->velocity.x += RandomRangef(-h, h);
    ent->velocity.y += RandomRangef(-h, h);
    ent->velocity.z += RandomRangef(100.f, 100.f + h);

    for (int32_t i = 0; i < 3; ++i) {
      ent->avelocity.xyz[i] = RandomRangef(-100.f, 100.f);
      ent->s.angles.xyz[i] = RandomRangef(0, 360.f);
    }

    ent->Die = NULL;

    ent->client->ps.pmState.flags |= PMF_GIBLET;

    ent->s.model1 = gMedia.models.gibs[2];

    ent->mass = 20.0;

    ent->solid = SOLID_DEAD;

    gi.LinkEntity(ent);
  } else {
    G_FreeEntity(ent);
  }
}

/**
 * @return The animation a corpse should be left in, given the one its client died playing.
 * @details A player who respawns before their death animation has run out is still mid-death,
 * and G_ClientAnimation settles that only for a client, which a corpse no longer has. The
 * client also restarts the animation of any entity it has not seen before, so a corpse handed
 * a death animation plays the whole thing over again where it lies. Hand it the rest pose that
 * follows instead.
 */
static uint8_t G_ClientCorpseAnimation(uint8_t animation) {

  const uint8_t value = animation & ANIM_MASK_VALUE;

  switch (value) {
    case ANIM_BOTH_DEATH1:
    case ANIM_BOTH_DEATH2:
    case ANIM_BOTH_DEATH3:
      return value + 1;
    default:
      return value;
  }
}

/**
 * @brief Claims a slot of CS_CORPSES for @p corpse, snapshotting the client info @p cl is
 * wearing as it dies.
 * @details A corpse outlives its client's identity: they may change skin, or disconnect, and
 * were they still resolved through CS_CLIENTS the bodies they left would follow them, or fall
 * back to the default model when their entry is cleared. The snapshot is what they wore.
 * @remarks The ring is also the corpse limit. Whoever still holds the slot we come around to is
 * gibbed rather than quietly deleted, so a body always leaves in a way the player can read.
 */
static void G_ClientCorpseSlot(GameEntity *corpse, const GameClient *cl) {

  static uint32_t index;

  const uint8_t slot = index++ % MAX_CORPSES;

  G_ForEachEntity(ent, {
    if (ent != corpse && (ent->s.effects & EF_CORPSE) && ent->s.client == slot) {
      G_ClientCorpse_Die(ent, ent, MOD_CRUSH);
    }
  });

  corpse->s.client = slot;

  gi.SetConfigString(CS_CORPSES + slot, gi.GetConfigString(CS_CLIENTS + cl->ps.client));
}

/**
 * @brief Spawns a corpse for the specified client. The corpse will eventually sink into the floor
 * and disappear if not over-killed.
 */
static void G_ClientCorpse(GameClient *cl) {

  if (cl->entity->svFlags & SVF_NO_CLIENT) {
    return;
  }

  if (cl->entity->dead == false) {
    return;
  }

  if (cl->entity->s.model1 != MODEL_CLIENT) {
    return;
  }

  GameEntity *ent = G_AllocEntity(__func__);

  ent->solid = SOLID_DEAD;

  ent->s.origin = cl->entity->s.origin;
  ent->s.angles = cl->entity->s.angles;

  ent->bounds = cl->entity->bounds;

  ent->s.model1 = cl->entity->s.model1;

  ent->s.animation1 = G_ClientCorpseAnimation(cl->entity->s.animation1);
  ent->s.animation2 = G_ClientCorpseAnimation(cl->entity->s.animation2);

  ent->s.effects = EF_CLIENT | EF_CORPSE;

  // claim the slot before the corpse is linked, so that its client info is known to everyone
  // by the time it first appears in a frame
  G_ClientCorpseSlot(ent, cl);

  ent->velocity = cl->entity->velocity;

  ent->clipMask = CONTENTS_MASK_CLIP_CORPSE;
  ent->dead = true;
  ent->mass = cl->entity->mass;
  ent->moveType = MOVE_TYPE_BOUNCE;
  ent->takeDamage = true;
  ent->health = cl->entity->health;
  ent->Die = ent->health > 0 ? G_ClientCorpse_Die : NULL;
  ent->Pain = ent->health > 0 ? G_ClientCorpse_Pain : NULL;
  ent->Think = G_ClientCorpse_Think;
  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;

  gi.LinkEntity(ent);
}

#define CLIENT_CORPSE_HEALTH 80

/**
 * @brief A client's health is less than or equal to zero. Render death effects, drop
 * certain items we're holding and force the client into a temporary spectator
 * state with the scoreboard shown.
 */
static void G_ClientDie(GameEntity *ent, GameEntity *attacker, uint32_t mod) {

  GameClient *cl = ent->client;

  // gibbing randomizes the corpse velocity, so seed the death camera from the
  // velocity we actually died with
  const Vec3 velocity = ent->velocity;

  G_ClientObituary(cl, attacker, mod);

  G_TossInventory(cl);
  G_TossInvisibility(cl);
  G_TossInvulnerability(cl);

  if ((gLevel.gameplay & ~GAMEPLAY_TEAMS) == GAMEPLAY_DEATHMATCH && mod != MOD_TRIGGER_HURT) {
    G_TossWeapon(cl);
  }

  const bool gibbed = ent->health <= -CLIENT_CORPSE_HEALTH;

  if (gibbed) {
    G_ClientCorpse_Die(ent, attacker, mod);
  } else {
    G_MulticastSound(&(const GamePlaySound) {
      .index = gMedia.sounds.death[Randomi() % lengthof(gMedia.sounds.death)],
      .entity = ent,
      .origin = &ent->s.origin, // send the origin in case of fast respawn
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
    ent->Pain = G_ClientCorpse_Pain;
  }

  uint32_t nadeHoldTime = cl->grenadeHoldTime;

  if (nadeHoldTime != 0) {

    G_ClientProjectile(cl, NULL, NULL, NULL, &ent->s.origin, 1.0);

    G_HandGrenadeProjectile(
        ent,          // player
        cl->heldGrenade, // the grenade
        ent->s.origin,      // starting point
        Vec3_Up(),        // direction
        0,            // how fast it flies
        120,          // damage dealt
        120,          // knockback
        185.0,          // blast radius
        Clampf(3000 - (int32_t) (gLevel.time - nadeHoldTime), 1, 3000) // time before explode (next think)
    );
  }

  cl->grenadeHoldTime = 0;
  cl->grenadeHoldFrame = 0;
  cl->heldGrenade = NULL;

  ent->solid = SOLID_DEAD;
  ent->dead = true;

  ent->s.event = EV_NONE;
  ent->s.effects = gibbed ? 0 : EF_CLIENT;
  ent->s.trail = TRAIL_NONE;
  ent->s.model2 = 0;
  ent->s.model3 = 0;
  ent->s.model4 = 0;
  ent->s.sound = 0;

  ent->takeDamage = true;

  ent->clipMask = CONTENTS_MASK_CLIP_CORPSE;
  cl->respawnTime = gLevel.time + 1800; // respawn delay, independent of death animation length
  cl->deathTime = gLevel.time; // used to gate the DEATHx -> DEADx animation transition below
  cl->showScores = true;
  cl->persistent.deaths++;

  if (g_deathCam->integer && G_IsDeathCamMod(mod)) {

    // capture the eye position before the corpse's view offset takes effect
    cl->deathCamOrigin = Vec3_Add(ent->s.origin, cl->ps.pmState.viewOffset);

    cl->deathCamVelocity = Vec3_Fmaf(
      Vec3_Scale(Vec3_Up(), g_deathCamRise->value),
      g_deathCamVelocity->value,
      velocity
    );

    // settle behind and above where we were looking when we died
    cl->deathCamOffset = Vec3_Fmaf(
      Vec3_Scale(Vec3_Up(), g_deathCamHeight->value),
      -g_deathCamDistance->value,
      cl->forward
    );

    cl->deathCamAngles = cl->angles;
    cl->deathCamTime = gLevel.time;
    cl->ps.pmState.flags |= PMF_DEATH_CAM;
  } else {
    G_ClientDamageKick(cl, cl->right, 60.0);
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Stocks client's inventory with specified item. Weapons receive
 * specified quantity of ammo, while health and armor are set to
 * the specified quantity.
 */
static void G_Give(GameClient *cl, char *it, int16_t quantity) {

  if (!q_strcasecmp(it, "Health")) {
    cl->entity->health = quantity;
    return;
  }

  const GameItem *item = G_FindItem(it);

  if (!item) {
    return;
  }

  const GameItemTag index = item->def.tag;

  if (item->def.type == ITEM_TYPE_WEAPON) { // weapons receive quantity as ammo
    cl->inventory[index]++;

    if (item->def.ammo) {
      const GameItem *ammo = &gItems[item->def.ammo];
      const GameItemTag ammoIndex = ammo->def.tag;

      if (quantity > -1) {
        cl->inventory[ammoIndex] = quantity;
      } else {
        cl->inventory[ammoIndex] = ammo->def.quantity;
      }
    }
  } else { // while other items receive quantity directly
    if (quantity > -1) {
      cl->inventory[index] = quantity;
    } else {
      cl->inventory[index] = item->def.quantity;
    }
  }
}

/**
 * @brief Initializes a client's starting inventory based on the current game mode.
 */
static void G_InitInventory_Common(GameClient *cl) {
  const GameItem *item;

  // instagib gets railgun and slugs, both in normal mode and warmup
  if ((gLevel.gameplay & ~GAMEPLAY_TEAMS) == GAMEPLAY_INSTAGIB) {
    G_Give(cl, "Railgun", 1);
    G_Give(cl, "Grenades", 1);
    item = &gItems[WEAPON_RAILGUN];
  }
  // arena yields all weapons, health, etc..
  else if ((gLevel.gameplay & ~GAMEPLAY_TEAMS) == GAMEPLAY_ARENA) {
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

    item = &gItems[WEAPON_ROCKET_LAUNCHER];
  }
  // dm gets the blaster, or the quake shotgun + shells in quake item sets
  else if (gLevel.items == ITEMS_QUAKE) {
    G_Give(cl, "Shotgun", 10);
    item = &gItems[WEAPON_QUAKE_SHOTGUN];
  } else {
    G_Give(cl, "Blaster", -1);
    item = &gItems[WEAPON_BLASTER];
  }

  G_UseWeapon(cl, item);
}

InitInventory G_InitInventory = G_InitInventory_Common;

/**
 * @brief Returns the distance to the nearest enemy from the given spot.
 */
static float G_EnemyRangeFromSpot(GameClient *cl, GameEntity *spot) {
  float dist, bestDist;
  Vec3 v;

  bestDist = 9999999.0;

  G_ForEachClient(enemy, {
    if (!enemy->entity || enemy->entity->health <= 0) {
      continue;
    }

    if (enemy->persistent.spectator) {
      continue;
    }

    v = Vec3_Subtract(spot->s.origin, enemy->entity->s.origin);
    dist = Vec3_Length(v);

    if (gLevel.teams) { // avoid collision with team mates

      if (enemy->persistent.team == cl->persistent.team) {
        if (dist > 64.0) { // if they're far away, ignore them
          continue;
        }
      }
    }

    if (dist < bestDist) {
      bestDist = dist;
    }
  });

  return bestDist;
}

/**
 * @brief Checks if spawning a player in this spot would cause a telefrag.
 */
static bool G_WouldTelefrag(const Vec3 spot) {
  GameEntity *ents[MAX_ENTITIES];
  Box3 bounds = Box3_Translate(G_PlayerBounds(), spot);

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
 * @brief Selects a random unoccupied spawn point from the given set.
 */
static GameEntity *G_SelectRandomSpawnPoint(const GameSpawnPoints *spawnPoints) {

  if (!spawnPoints->count) {
    if (spawnPoints == &gLevel.spawnPoints) {
      G_Error("No spawn points on %s\n", gLevel.name);
    }
    return G_SelectRandomSpawnPoint(&gLevel.spawnPoints);
  }

  uint32_t emptySpawns[spawnPoints->count];
  uint32_t numEmptySpawns = 0;

  for (uint32_t i = 0; i < spawnPoints->count; i++) {

    if (!G_WouldTelefrag(spawnPoints->spots[i]->s.origin)) {
      emptySpawns[numEmptySpawns++] = i;
    }
  }

  if (numEmptySpawns) {
    return spawnPoints->spots[emptySpawns[RandomRangeu(0, numEmptySpawns)]];
  }

  return spawnPoints->spots[RandomRangeu(0, spawnPoints->count)];
}

/**
 * @brief Adds unique spawn pointers from @c points into @c pool.
 */
static uint32_t G_CollectSpawnPoints(GameEntity **pool, uint32_t count, const GameSpawnPoints *points) {

  for (uint32_t i = 0; i < points->count; i++) {
    GameEntity *spot = points->spots[i];
    bool exists = false;

    for (uint32_t j = 0; j < count; j++) {
      if (pool[j] == spot) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      pool[count++] = spot;
    }
  }

  return count;
}

/**
 * @brief Selects a random unoccupied spawn point from a flat list.
 */
static GameEntity *G_SelectRandomSpawnPointFromPool(GameEntity **spots, const uint32_t count) {

  if (!count) {
    return G_SelectRandomSpawnPoint(&gLevel.spawnPoints);
  }

  uint32_t emptySpawns[count];
  uint32_t numEmptySpawns = 0;

  for (uint32_t i = 0; i < count; i++) {
    if (!G_WouldTelefrag(spots[i]->s.origin)) {
      emptySpawns[numEmptySpawns++] = i;
    }
  }

  if (numEmptySpawns) {
    return spots[emptySpawns[RandomRangeu(0, numEmptySpawns)]];
  }

  return spots[RandomRangeu(0, count)];
}

/**
 * @brief Selects the spawn point farthest from all enemies for the given client.
 */
static GameEntity *G_SelectFarthestSpawnPoint(GameClient *cl, const GameSpawnPoints *spawnPoints) {
  GameEntity *spot, *bestSpot;
  float dist, bestDist;

  spot = bestSpot = NULL;
  bestDist = 0.0;

  for (size_t i = 0; i < spawnPoints->count; i++) {

    spot = spawnPoints->spots[i];
    dist = G_EnemyRangeFromSpot(cl, spot);

    if (dist > bestDist && !G_WouldTelefrag(spot->s.origin)) {
      bestSpot = spot;
      bestDist = dist;
    }
  }

  if (bestSpot) {
    return bestSpot;
  }

  return G_SelectRandomSpawnPoint(spawnPoints);
}

/**
 * @brief Selects the farthest spawn point from a flat list.
 */
static GameEntity *G_SelectFarthestSpawnPointFromPool(GameClient *cl, GameEntity **spots, const uint32_t count) {
  GameEntity *bestSpot = NULL;
  float bestDist = 0.0;

  for (uint32_t i = 0; i < count; i++) {
    GameEntity *spot = spots[i];
    const float dist = G_EnemyRangeFromSpot(cl, spot);

    if (dist > bestDist && !G_WouldTelefrag(spot->s.origin)) {
      bestSpot = spot;
      bestDist = dist;
    }
  }

  if (bestSpot) {
    return bestSpot;
  }

  return G_SelectRandomSpawnPointFromPool(spots, count);
}

/**
 * @brief Selects an appropriate deathmatch spawn point for the given client.
 */
static GameEntity *G_SelectDeathmatchSpawnPoint(GameClient *cl) {
  // Include team spawns in non-team modes to improve spawn distribution on maps
  // that define both DM and team spawn entities.
  GameEntity *pool[MAX_ENTITIES];
  uint32_t count = 0;
  
  count = G_CollectSpawnPoints(pool, count, &gLevel.spawnPoints);

  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    count = G_CollectSpawnPoints(pool, count, &gTeamList[t].spawnPoints);
  }

  if (g_spawnFarthest->value) {
    return G_SelectFarthestSpawnPointFromPool(cl, pool, count);
  }

  return G_SelectRandomSpawnPointFromPool(pool, count);
}

/**
 * @brief Selects an appropriate team spawn point for the given client.
 */
static GameEntity *G_SelectTeamSpawnPoint(GameClient *cl) {

  if (!cl->persistent.team) {
    return NULL;
  }

  if (g_spawnFarthest->value) {
    return G_SelectFarthestSpawnPoint(cl, &cl->persistent.team->spawnPoints);
  }

  return G_SelectRandomSpawnPoint(&cl->persistent.team->spawnPoints);
}

/**
 * @brief Selects the most appropriate spawn point for the given client.
 */
static GameEntity *G_SelectSpawnPoint(GameClient *cl) {
  GameEntity *spawn = NULL;

  if (gLevel.teams) { // try team spawns first if applicable
    spawn = G_SelectTeamSpawnPoint(cl);
  }

  if (spawn == NULL) { // fall back on DM spawns (e.g CTF games on DM maps)
    spawn = G_SelectDeathmatchSpawnPoint(cl);
  }

  return spawn;
}

/**
 * @brief The tail of the `G_PrepareSpawn` chain: the spawn point as selected.
 */
static void G_PrepareSpawn_Common(GameClient *cl, GameClientSpawn *spawn) {
}

PrepareSpawn G_PrepareSpawn = G_PrepareSpawn_Common;

/**
 * @brief The tail of the `G_ClientWillBegin` chain: a notification, so it does nothing.
 */
static void G_ClientWillBegin_Common(GameClient *cl) {
}

ClientWillBegin G_ClientWillBegin = G_ClientWillBegin_Common;

/**
 * @brief The tail of the `G_ClientDidBegin` chain: a notification, so it does nothing.
 */
static void G_ClientDidBegin_Common(GameClient *cl) {
}

ClientDidBegin G_ClientDidBegin = G_ClientDidBegin_Common;

/**
 * @brief The tail of the `G_ClientWillChangeUserInfo` chain: a notification, so it does nothing.
 */
static void G_ClientWillChangeUserInfo_Common(GameClient *cl, const char *userInfo) {
}

ClientWillChangeUserInfo G_ClientWillChangeUserInfo = G_ClientWillChangeUserInfo_Common;

/**
 * @brief The tail of the `G_ClientDidChangeUserInfo` chain: a notification, so it does nothing.
 */
static void G_ClientDidChangeUserInfo_Common(GameClient *cl) {
}

ClientDidChangeUserInfo G_ClientDidChangeUserInfo = G_ClientDidChangeUserInfo_Common;

/**
 * @brief The tail of the `G_ClientWillDisconnect` chain: a notification, so it does nothing.
 */
static void G_ClientWillDisconnect_Common(GameClient *cl) {
}

ClientWillDisconnect G_ClientWillDisconnect = G_ClientWillDisconnect_Common;

/**
 * @brief The tail of the `G_ClientDidDisconnect` chain: a notification, so it does nothing.
 */
static void G_ClientDidDisconnect_Common(GameClient *cl) {
}

ClientDidDisconnect G_ClientDidDisconnect = G_ClientDidDisconnect_Common;

/**
 * @brief The tail of the `G_ClientWillThink` chain: a notification, so it does nothing.
 */
static void G_ClientWillThink_Common(GameClient *cl, const PlayerMoveCmd *cmd) {
}

ClientWillThink G_ClientWillThink = G_ClientWillThink_Common;

/**
 * @brief The tail of the `G_ClientDidMove` chain: a notification, so it does nothing.
 */
static void G_ClientDidMove_Common(GameClient *cl, const PlayerMoveCmd *cmd) {
}

ClientDidMove G_ClientDidMove = G_ClientDidMove_Common;

/**
 * @brief Spawns the client's entity, as a player or a spectator.
 */
static void G_ClientRespawn_(GameClient *cl) {

  if (!cl->entity) {
    return;
  }

#if defined(G_HOOK)
  G_HookDetach(cl);
#endif

  gi.UnlinkEntity(cl->entity);

  // leave a corpse in our place if applicable
  G_ClientCorpse(cl);

  // clear the client, retaining only critical fields
  const GameClient tmp = *cl;
  memset(cl, 0, sizeof(*cl));

  cl->entity = tmp.entity;
  cl->ps.client = tmp.ps.client;
  cl->ping = tmp.ping;
  cl->inUse = tmp.inUse;
  cl->ai = tmp.ai;
  cl->persistent = tmp.persistent;
  memmove(cl->userInfo, cl->persistent.userInfo, sizeof(cl->userInfo));

  GameEntity *ent = cl->entity;

  // find a spawn point
  const GameEntity *spawn = G_SelectSpawnPoint(cl);
  assert(spawn);

  GameClientSpawn place = {
    .origin = spawn->s.origin,
    .angles = spawn->s.angles,
    .clipMask = cl->ai ? CONTENTS_MASK_CLIP_MONSTER : CONTENTS_MASK_CLIP_PLAYER,
    .killBox = true,
  };

  G_PrepareSpawn(cl, &place);

  // move to the spawn origin
  ent->s.origin = place.origin;
  ent->s.origin.z += PM_STEP_HEIGHT;

  // snap view angles directly to the spawn point; the client will snap
  // cl.angles to match, so no delta_angles compensation is needed
  ent->s.angles = Vec3_Zero();
  cl->angles = place.angles;

  // pack the new origin and view angles into the player state
  cl->ps.pmState.origin = ent->s.origin;
  cl->ps.pmState.viewAngles = place.angles;
  cl->ps.pmState.deltaAngles = Vec3_Zero();
  cl->ps.entity = ent->s.number;

  // signal the client to snap to view_angles; only for player spawns, not spectators
  if (!cl->persistent.spectator && !editor->value) {
    gi.WriteByte(SV_CMD_SNAP_ANGLES);
    gi.WriteAngles(place.angles);
    gi.Unicast(cl, true);
  }

  ent->velocity = Vec3_Zero();

  ent->s.effects = EF_CLIENT | EF_MODULATE;
  ent->s.model1 = 0;
  ent->s.model2 = 0;
  ent->s.model3 = 0;
  ent->s.model4 = 0;

  if (cl->persistent.spectator || editor->value) { // spawn a spectator
    ent->classname = "spectator";

    ent->bounds = Box3_Zero();

    ent->solid = SOLID_NOT;
    ent->svFlags = SVF_NO_CLIENT;

    ent->moveType = MOVE_TYPE_NO_CLIP;
    ent->dead = true;
    ent->takeDamage = false;

    cl->chaseTarget = NULL;
    cl->weapon = NULL;

    cl->persistent.team = NULL;
  } else { // spawn an active client
    ent->classname = "client";

    ent->solid = SOLID_BOX;
    ent->svFlags = 0;

    ent->bounds = G_PlayerBounds();

    ent->s.model1 = MODEL_CLIENT;
    ent->s.event = EV_CLIENT_TELEPORT;

    const int32_t teleportSound = gLevel.items == ITEMS_QUAKE
      ? gMedia.sounds.quakeTeleport[RandomRangei(0, 5)]
      : gMedia.sounds.teleport;

    G_MulticastSound(&(const GamePlaySound) {
      .index = teleportSound,
      .origin = &ent->s.origin,
    }, MULTICAST_PHS);

    G_SetAnimation(cl, ANIM_TORSO_STAND1, true);
    G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);

    ent->clipMask = place.clipMask;

    ent->dead = false;
    ent->Die = G_ClientDie;
    memset(&ent->ground, 0, sizeof(ent->ground));
    ent->maxHealth = 100;
    ent->health = ent->maxHealth + 5;
    ent->moveType = MOVE_TYPE_WALK;
    ent->mass = 200.0;
    ent->takeDamage = true;
    ent->waterLevel = WATER_UNKNOWN;
    ent->waterType = 0;
    ent->rippleSize = 32.0;

    cl->boostTime = gLevel.time + 1000;
    cl->maxArmor = 200;
    cl->maxBoostHealth = ent->maxHealth + 100;

    // hold in place briefly
    cl->ps.pmState.flags = PMF_TIME_TELEPORT;
    cl->ps.pmState.time = 20;

    // setup inventory/weapon
    if (!G_Ai_InDeveloperMode()) {
      G_InitInventory(cl);
    }

    // briefly disable firing, since we likely just clicked to respawn
    cl->weaponFireTime = gLevel.time + 250;

    if (place.killBox) {
      G_KillBox(ent); // kill anyone in our spot
    }
  }

  gi.LinkEntity(ent);
}

/**
 * @brief In this case, voluntary means that the client has explicitly requested
 * a respawn by changing their spectator status.
 */
void G_ClientRespawn(GameClient *cl, bool voluntary) {

  G_ClientRespawn_(cl);

  // clear scores on voluntary changes
  if (cl->persistent.spectator && voluntary) {
    cl->persistent.score = cl->persistent.deaths = 0;
#if defined(G_CTF)
    cl->persistent.captures = 0;
#endif
  }

  cl->respawnTime = gLevel.time;
  cl->respawnProtectionTime = gLevel.time + g_respawnProtection->value * 1000;

  if (cl->ai) {
    G_Ai_Respawn(cl);
  }

  if (!voluntary) { // don't announce involuntary spectator changes
    return;
  }

  if (cl->persistent.spectator) {
    gi.BroadcastPrint(PRINT_HIGH, "%s likes to watch\n", cl->persistent.netName);
  } else if (cl->persistent.team) {
    gi.BroadcastPrint(PRINT_HIGH, "%s has joined %s\n", cl->persistent.netName, cl->persistent.team->name);
  } else {
    gi.BroadcastPrint(PRINT_HIGH, "%s wants some\n", cl->persistent.netName);
  }
}

/**
 * @brief Called when a client has finished connecting, and is ready
 * to be placed into the game. This will happen every level load.
 */
void G_ClientBegin(GameClient *cl) {
  char welcome[MAX_STRING_CHARS];

  G_ClientWillBegin(cl);

  // setup the client's entity
  cl->entity = G_AllocEntity("client");
  cl->entity->client = cl;
  cl->entity->s.client = cl->ps.client;
  cl->entity->s.effects = EF_CLIENT | EF_MODULATE;
  cl->ps.entity = cl->entity->s.number;

  cl->cmdAngles = Vec3_Zero();
  cl->persistent.firstFrame = gLevel.frameNum;

  // ensure CS_CLIENTS is set before the entity becomes visible in frames
  G_ClientUserInfoChanged(cl, cl->persistent.userInfo);

  if (editor->value) {
    cl->persistent.spectator = true;
  }
  else {
    if (gLevel.teams) {
      if (g_autoJoin->value || cl->ai) {
        G_AddClientToTeam(cl, G_SmallestTeam()->name);
      } else {
        cl->persistent.spectator = true;
      }
    }
  }

  G_ClientRespawn(cl, true);

  if (gLevel.intermissionTime) {
    G_ClientToIntermission(cl);
  } else {
    q_snprintf(welcome, sizeof(welcome), "^2Welcome to ^7%s", sv_hostname->string);

    if (*g_motd->string) {
      char motd[MAX_QPATH];
      q_snprintf(motd, sizeof(motd), "\n%s^7", g_motd->string);

      q_strlcat(welcome, motd, sizeof(welcome));
    }

    q_strlcat(welcome, "\n^2Gameplay is ^1", sizeof(welcome));
    q_strlcat(welcome, G_GameplayById(gLevel.gameplay)->label, sizeof(welcome));

    q_strlcat(welcome, "\n^2Movement is ^1", sizeof(welcome));
    q_strlcat(welcome, Pm_Movement(gLevel.movement)->label, sizeof(welcome));

    if (gLevel.teams) {
      q_strlcat(welcome, "\n^2Teams are enabled", sizeof(welcome));
    }

    gi.ClientPrint(cl, PRINT_HIGH, "%s\n", welcome);
  }

  // make sure all view stuff is valid
  G_ClientEndFrame(cl);

  G_ClientDidBegin(cl);
}

/**
 * @brief The client's standing box, which the client game sizes their model by:
 * their own movement parameters once they have moved, and the level's until then.
 */
Box3 G_ClientStandingBounds(const GameClient *cl) {

  const Box3 bounds = cl->ps.pmState.params.bounds;

  return Box3_Size(bounds).z > 0.f ? bounds : G_PlayerBounds();
}

/**
 * @brief Applies updates from a client's user info string to their persistent state.
 */
void G_ClientUserInfoChanged(GameClient *cl, const char *userInfo) {
  char name[MAX_NET_NAME];

  G_ClientWillChangeUserInfo(cl, userInfo);

  // check for malformed or illegal info strings
  if (!InfoString_Validate(userInfo)) {
    G_Warn("Invalid user info\n");
    userInfo = DEFAULT_USER_INFO;
  }

  // save off the user_info in case we want to check something later
  const size_t len = q_strlen(userInfo);
  memmove(cl->userInfo, userInfo, len + 1);
  memmove(cl->persistent.userInfo, userInfo, len + 1);

  G_Debug("%s\n", userInfo);

  // set name, use a temp buffer to compute length and crutch up bad names
  const char *s = InfoString_Get(userInfo, "name");

  q_strlcpy(name, s, sizeof(name));

  bool color = false;
  char *c = name;
  int32_t i = 0;

  // trim to 15 printable chars
  while (i < MAX_NET_NAME_PRINTABLE) {

    if (!*c) {
      break;
    }

    if (q_striscolor(c)) {
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
    q_strlcat(name, "^7", sizeof(name));
  }

  if (q_strncmp(cl->persistent.netName, name, sizeof(cl->persistent.netName))) {

    if (*cl->persistent.netName != '\0') {
      gi.BroadcastPrint(PRINT_MEDIUM, "%s changed name to %s\n", cl->persistent.netName, name);
    }

    q_strlcpy(cl->persistent.netName, name, sizeof(cl->persistent.netName));
  }

  const GameTeam *team = cl->persistent.team;

  // set skin
  if (team) { // players must use team_skin to change
    s = InfoString_Get(userInfo, "skin");

    char *p;
    if (q_strlen(s) && (p = q_strchr(s, '/'))) {
      *p = 0;
      s = va("%s/%s", s, DEFAULT_TEAM_SKIN);
    } else {
      s = va("%s/%s", DEFAULT_USER_MODEL, DEFAULT_TEAM_SKIN);
    }
  } else {
    s = InfoString_Get(userInfo, "skin");
  }

  if (q_strlen(s) && !q_strstr(s, "..")) { // something valid-ish was provided
    q_strlcpy(cl->persistent.skin, s, sizeof(cl->persistent.skin));
  } else {
    q_strlcpy(cl->persistent.skin, DEFAULT_USER_MODEL "/" DEFAULT_USER_SKIN, sizeof(cl->persistent.skin));
  }

  // set effect color
  if (team) { // players must use team_skin to change
    cl->persistent.color = team->color;
  } else {
    s = InfoString_Get(userInfo, "color");

    cl->persistent.color = -1;

    if (q_strlen(s) && q_strcmp(s, "default")) { // not default
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

    s = InfoString_Get(userInfo, "shirt");
    if (!Color_Parse(s, &cl->persistent.shirt)) {
      cl->persistent.shirt = color_white;
    }

    s = InfoString_Get(userInfo, "pants");
    if (!Color_Parse(s, &cl->persistent.pants)) {
      cl->persistent.pants = color_white;
    }

    s = InfoString_Get(userInfo, "helmet");
    if (!Color_Parse(s, &cl->persistent.helmet)) {
      cl->persistent.helmet = color_white;
    }
  }

  char clientInfo[MAX_INFO_STRING_STRING] = { '\0' };

  // build the client info string
  q_strlcat(clientInfo, va("%d", team ? team->id : TEAM_NONE), sizeof(clientInfo));

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, cl->persistent.netName, sizeof(clientInfo));

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, cl->persistent.skin, sizeof(clientInfo));

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, Color_Unparse(cl->persistent.shirt), sizeof(clientInfo));

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, Color_Unparse(cl->persistent.pants), sizeof(clientInfo));

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, Color_Unparse(cl->persistent.helmet), sizeof(clientInfo));

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, va("%i", cl->persistent.color), sizeof(clientInfo));

  cl->persistent.standingBounds = G_ClientStandingBounds(cl);

  q_strlcat(clientInfo, "\\", sizeof(clientInfo));
  q_strlcat(clientInfo, va("%g/%g", cl->persistent.standingBounds.mins.z,
                            cl->persistent.standingBounds.maxs.z), sizeof(clientInfo));

  // send it to clients
  gi.SetConfigString(CS_CLIENTS + cl->ps.client, clientInfo);

  // set hand, if anything should go wrong, it defaults to 0 (centered)
  cl->persistent.hand = (GameHand) strtol(InfoString_Get(userInfo, "hand"), NULL, 10);

  if (cl->entity) {
    s = InfoString_Get(userInfo, "active");
    if (q_strcmp(s, "0") == 0) {
      cl->entity->s.effects |= EF_INACTIVE;
    } else {
      cl->entity->s.effects &= ~(EF_INACTIVE);
    }
  }

  // auto-switch
  uint16_t autoSwitch = strtoul(InfoString_Get(userInfo, "auto_switch"), NULL, 10);
  cl->persistent.autoSwitch = autoSwitch;

#if defined(G_HOOK)
  // hook style
  G_SetClientHookStyle(cl);
#endif

  // stats guid
  q_strlcpy(cl->persistent.guid, InfoString_Get(userInfo, "guid"), sizeof(cl->persistent.guid));

  G_ClientDidChangeUserInfo(cl);
}

/**
 * @brief Called when a player begins connecting to the server.
 * The game can refuse entrance to a client by returning false.
 * If the client is allowed, the connection process will continue
 * and eventually get to `G_Begin()`.
 * Changing levels will NOT cause this to be called again.
 */
bool G_ClientConnect(GameClient *cl, char *userInfo) {

  // check password
  if (q_strlen(g_password->string) && !cl->ai) {
    if (q_strcmp(g_password->string, InfoString_Get(userInfo, "password"))) {
      InfoString_Set(userInfo, "rejmsg", "Password required or incorrect.");
      return false;
    }
  }

  memset(&cl->persistent, 0, sizeof(cl->persistent));

  // set name, skin, etc..
  G_ClientUserInfoChanged(cl, userInfo);

  cl->inUse = true;

  gi.BroadcastPrint(PRINT_HIGH, "%s connected\n", cl->persistent.netName);



  return true;
}

/**
 * @brief Called when a player drops from the server. Not be called between levels.
 */
void G_ClientDisconnect(GameClient *cl) {

  G_ClientWillDisconnect(cl);

  // client numbers are reused, so a mute left behind would silence whoever inherits the slot
  const uint64_t bit = (uint64_t) 1 << cl->ps.client;

  G_ForEachClient(other, {
    other->persistent.mutedClients &= ~bit;
  });

  cl->persistent.mutedClients = 0;

  if (cl->entity) {
    G_TossInventory(cl);
    G_TossInvisibility(cl);
    G_TossInvulnerability(cl);
  }

  cl->inUse = false;

  gi.BroadcastPrint(PRINT_HIGH, "%s disconnected\n", cl->persistent.netName);

  if (G_IsMeat(cl->entity)) {
    gi.WriteByte(SV_CMD_MUZZLE_FLASH);
    gi.WriteShort(cl->entity->s.number);
    gi.WriteByte(MZ_LOGOUT);
    gi.Multicast(cl->entity->s.origin, MULTICAST_PHS);
  }


  const uint8_t client = cl->ps.client;
  gi.SetConfigString(CS_CLIENTS + client, "");

  if (cl->ai) {
    G_Ai_Disconnect(cl);
  }

  if (cl->entity) {
    G_FreeEntity(cl->entity);
    cl->entity = NULL;
  }

  G_ClientDidDisconnect(cl);

  memset(cl, 0, sizeof(GameClient));
  cl->ps.client = client;
}

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static CmTrace G_ClientMove_Trace(const Vec3 start, const Vec3 end, const Box3 bounds) {
  const GameEntity *self = gLevel.currentEntity;

  return gi.Trace(start, end, bounds, self, self->clipMask);
}

#if defined(_DEBUG)
static bool gRecordingPmove = false;
static bool gPlayPmove = false;
static File *gPmoveFile;
static uint64_t pmoveFrame = 0;
static uint64_t pmoveFrames = 0;

/**
 * @brief Begins recording every `Pm_Move` to a file, for playback through `G_PlayPmove`.
 */
void G_RecordPmove(void) {
  if (gPlayPmove) {
    return;
  }

  if (gRecordingPmove) {
    gi.CloseFile(gPmoveFile);
    gRecordingPmove = false;
    gi.Print("Closing pmove recording\n");
    return;
  }

  gRecordingPmove = true;
  gPmoveFile = gi.OpenFileWrite("pmove.deboog");
  gi.Print("Starting pmove recording\n");
}

/**
 * @brief Begins replaying a `G_RecordPmove` file through `Pm_Move`, frame for frame.
 */
void G_PlayPmove(void) {
  if (gRecordingPmove) {
    return;
  }

  if (gPlayPmove) {
    gi.CloseFile(gPmoveFile);
    gPlayPmove = false;
    gi.Print("Closing pmove recording\n");
    return;
  }

  gPlayPmove = true;
  pmoveFrames = gi.LoadFile("pmove.deboog", NULL) / sizeof(PlayerMove);
  gPmoveFile = gi.OpenFile("pmove.deboog");
  gi.Print("Starting pmove playback\n");

  if (gi.Argc() > 1) {
    pmoveFrame = strtoull(gi.Argv(1), NULL, 10);
    gi.SeekFile(gPmoveFile, sizeof(PlayerMove) * pmoveFrame);
  } else {
    pmoveFrame = 0;
  }
}
#endif

/**
 * @brief The tail of the `G_PrepareMove` chain, handing the move the entity's
 * own velocity.
 */
static void G_PrepareMove_Common(GameClient *cl, PlayerMove *pm) {

  pm->s.velocity = cl->entity->velocity;
}

PrepareMove G_PrepareMove = G_PrepareMove_Common;

/**
 * @brief The `G_ClipEntity` chain has no tail: it is `NULL` until a module
 * installs a link, and `G_Init` exports whatever is installed, so that a game
 * with nothing to say is never asked.
 */
ClipEntity G_ClipEntity = NULL;

/**
 * @brief Process the movement command, call `Pm_Move` and act on the result.
 */
static void G_ClientMove(GameClient *cl, PlayerMoveCmd *cmd) {
  Vec3 oldVelocity, velocity;

  GameEntity *ent = cl->entity;

  // save the raw angles sent over in the command
  cl->cmdAngles = cmd->angles;

  // set the move type
  if (ent->moveType == MOVE_TYPE_NO_CLIP) {
    cl->ps.pmState.type = PM_SPECTATOR;
  } else if (ent->dead) {
    cl->ps.pmState.type = PM_DEAD;
  } else {
    cl->ps.pmState.type = PM_NORMAL;
  }

  // hydrate the current movement parameters (gravity, accel, friction, speeds);
  // a class-based mod could override fields here for per-player physics
  cl->ps.pmState.params = G_MovementParams();

  PlayerMove pm;
  memset(&pm, 0, sizeof(pm));

#if defined(_DEBUG)
  if (gPlayPmove) {
    if (!gi.ReadFile(gPmoveFile, &pm, sizeof(pm), 1)) {
      gPlayPmove = false;
      gi.CloseFile(gPmoveFile);
      gi.Print("Finished pmove playback\n");
    } else {
      gi.Print("PMove frame %8" PRIu64 " / %8" PRIu64, pmoveFrame, pmoveFrames);
      pmoveFrame++;
    }
  }

  if (!gPlayPmove) {
#endif
    pm.s = cl->ps.pmState;

    pm.s.origin = ent->s.origin;

    G_PrepareMove(cl, &pm);

    pm.cmd = *cmd;
    pm.ground = ent->ground;
#if defined(_DEBUG)
  }
#endif

  pm.PointContents = gi.PointContents;
  pm.BoxContents = gi.BoxContents;
  
  pm.Trace = G_ClientMove_Trace;

  pm.Debug = gi.Debug;
  pm.DebugMask = gi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_SERVER;

#if defined(_DEBUG)
  if (gRecordingPmove) {
    gi.WriteFile(gPmoveFile, &pm, sizeof(pm), 1);
  }
#endif

  // perform a move
  Pm_Move(&pm);

  // save results of move
  cl->ps.pmState = pm.s;

  oldVelocity = ent->velocity;

  ent->s.stepOffset = roundf(pm.s.stepOffset);
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
      if (gLevel.time - 100 > cl->jumpTime) {
        Vec3 angles, forward, point;
        CmTrace tr;

        angles = MakeVec3(0.0, ent->s.angles.y, 0.0);
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
        if (pm.waterLevel < WATER_UNDER && ent->s.event != EV_CLIENT_LAND) {
          ent->s.event = EV_CLIENT_JUMP;
        }

        cl->jumpTime = gLevel.time;
      }
    } else if (pm.s.flags & PMF_TIME_WATER_JUMP) {
      if (gLevel.time - 2000 > cl->jumpTime) {

        G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);

        ent->s.event = EV_CLIENT_JUMP;
        cl->jumpTime = gLevel.time;
      }
    } else if (pm.s.flags & PMF_TIME_LAND) {
      if (gLevel.time - 800 > cl->landTime) {
        GameEntityEvent event = EV_CLIENT_LAND;

        if (G_IsAnimation(cl, ANIM_LEGS_JUMP2)) {
          G_SetAnimation(cl, ANIM_LEGS_LAND2, true);
        } else {
          G_SetAnimation(cl, ANIM_LEGS_LAND1, true);
        }

        if (oldVelocity.z <= PM_SPEED_FALL) { // player will take damage
          int32_t damage = ((int32_t) - ((oldVelocity.z - PM_SPEED_FALL) * 0.05));

          damage >>= pm.waterLevel; // water breaks the fall

          if (damage < 1) {
            damage = 1;
          }

          damage = (int32_t) (damage * g_fallDamage->value); // scale fall damage; 0 disables it (cf. Quake2 DF_NO_FALLING)

          if (oldVelocity.z <= PM_SPEED_FALL_FAR) {
            event = EV_CLIENT_FALL_FAR;
          } else {
            event = EV_CLIENT_FALL;
          }

          if (damage >= 1) {
            cl->painTime = gLevel.time; // suppress pain sound

            // TODO: get normal from what we've landed on
            G_Damage(&(GameDamage) {
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
        }

        ent->s.event = event;
        cl->landTime = gLevel.time;
      }
    } else if (pm.s.flags & PMF_ON_LADDER) {
      if (gLevel.time - 400 > cl->jumpTime) {
        if (fabs(ent->velocity.z) > 20.0) {

          G_SetAnimation(cl, ANIM_LEGS_JUMP1, true);

          ent->s.event = EV_CLIENT_JUMP;
          cl->jumpTime = gLevel.time;
        }
      }
    }

    // detect hitting the ground to help with animation blending
    if (pm.ground.ent && !ent->ground.ent) {
      cl->groundTime = gLevel.time;
    }
  }

  // copy ground and water state back into entity
  ent->ground = pm.ground;
  ent->waterLevel = pm.waterLevel;
  ent->waterType = pm.waterType;

  // and finally link them back in to collide with others below
  gi.LinkEntity(ent);

  // touch every object we collided with objects
  if (ent->moveType != MOVE_TYPE_NO_CLIP) {

    const CmTrace *touched = pm.touched;
    for (int32_t i = 0; i < pm.numTouched; i++, touched++) {
      GameEntity *other = touched->ent;

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
static void G_ClientInventoryThink(GameClient *cl) {

  if (cl->inventory[POWERUP_QUAD]) { // if they have quad

    if (cl->quadCountdownTime && cl->quadCountdownTime < gLevel.time) { // play the countdown sound      
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.quadExpire,
        .entity = cl->entity,
      }, MULTICAST_PHS);
      
      cl->quadCountdownTime += 1000;
    }

    if (cl->quadDamageTime < gLevel.time) { // expire it

      cl->quadDamageTime = 0.0;
      cl->inventory[POWERUP_QUAD] = 0;

      cl->entity->s.effects &= ~EF_QUAD;
    }
  }

  // other power-ups and things can be timed out here as well

  if (cl->inventory[POWERUP_INVISIBILITY]) {

    if (cl->invisibilityTime < gLevel.time) {
      cl->invisibilityTime = 0;
      cl->inventory[POWERUP_INVISIBILITY] = 0;
      cl->entity->s.effects &= ~EF_INVISIBILITY;

      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.invisibilityExpire,
        .entity = cl->entity,
      }, MULTICAST_PHS);
    }
  }

  if (cl->inventory[POWERUP_INVULNERABILITY]) {

    if (cl->invulnerabilityCountdownTime && cl->invulnerabilityCountdownTime < gLevel.time) {
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.invulnerabilityExpire,
        .entity = cl->entity,
      }, MULTICAST_PHS);

      cl->invulnerabilityCountdownTime += 1000;
    }

    if (cl->invulnerabilityTime < gLevel.time) {
      cl->invulnerabilityTime = 0;
      cl->invulnerabilityCountdownTime = 0;
      cl->inventory[POWERUP_INVULNERABILITY] = 0;
      cl->entity->s.effects &= ~EF_INVULNERABILITY;
    }
  }

  if (cl->respawnProtectionTime > gLevel.time) {
    cl->entity->s.effects |= EF_RESPAWN;
  } else {
    cl->entity->s.effects &= ~EF_RESPAWN;
  }

  // decrement any boosted health from items
  if (cl->boostTime != 0 && cl->boostTime < gLevel.time) {
    if (cl->entity->health > cl->entity->maxHealth) {
      cl->entity->health -= 1;
      cl->boostTime = gLevel.time + 1000;
    } else {
      cl->boostTime = 0;
    }
  }
}

/**
 * @brief This will be called once for each client frame, which will usually be a
 * couple times for each server frame.
 */
void G_ClientThink(GameClient *cl, PlayerMoveCmd *cmd) {

  if (gLevel.intermissionTime) {
    return;
  }

  gLevel.currentEntity = cl->entity;

  if (cl->chaseTarget) { // ensure chase is valid
    if (!G_IsMeat(cl->chaseTarget->entity)) {
      G_ClientChaseNext(cl);
    }
  }

  // setup buttons now, since pmove won't modify them
  cl->oldButtons = cl->buttons;
  cl->buttons = cmd->buttons;
  cl->latchedButtons |= cl->buttons & ~cl->oldButtons;

  G_ClientWillThink(cl, cmd);

  if (!cl->chaseTarget) { // move through the world

#if defined(G_HOOK)
    if (cl->hook.thinkTime < gLevel.time) {
      G_HookThink(cl, false);
    }
#endif

    G_ClientMove(cl, cmd);

    G_ClientDidMove(cl, cmd);
  }

  cl->cmd = *cmd;

  // fire weapon if requested
  if (cl->latchedButtons & BUTTON_ATTACK) {
    if (cl->persistent.spectator) {

      cl->latchedButtons = 0;

      if (cl->chaseTarget) { // toggle chase camera
        cl->chaseTarget = cl->oldChaseTarget = NULL;
      } else {
        G_ClientChaseTarget(cl);
      }

      G_ClientChaseThink(cl);
    } else if (cl->weaponThinkTime < gLevel.time) {
      G_ClientWeaponThink(cl);
    }
  }

  G_ClientInventoryThink(cl);

  // update anyone chasing us
  G_ForEachClient(chaser, {
    if (chaser->chaseTarget == cl) {
      G_ClientChaseThink(chaser);
    }
  });

  // if we're the first player in a game, send our client over
  // to the AI system in case it needs to make nodes
  if (cl->ps.client == 0) {
    G_Ai_Node_PlayerRoam(cl, cmd);
  }
}

/**
 * @brief This will be called once for each server frame, before running
 * any other entities in the world.
 */
void G_ClientBeginFrame(GameClient *cl) {

  if (gLevel.intermissionTime) {
    return;
  }

  GameEntity *ent = cl->entity;

  if ((G_IsMeat(ent) && ent->dead) ||  ((cl->buttons | cl->latchedButtons) & BUTTON_SCORE)) {
    cl->showScores = true;
  } else {
    cl->showScores = false;
  }

  // run weapon think if it hasn't been done by a command
  if (cl->weaponThinkTime < gLevel.time) {
    G_ClientWeaponThink(cl);
  }

#if defined(G_HOOK)
  if (cl->hook.thinkTime < gLevel.time) {
    G_HookThink(cl, false);
  }
#endif

#if defined(G_TECH)
  const bool wasDead = ent->dead;
#endif

  if (ent->dead) {
    if (gLevel.time > cl->respawnTime) {
      if (cl->latchedButtons & BUTTON_ATTACK) {
        G_ClientRespawn(cl, false);
      }
    }
  }

#if defined(G_TECH)
  // the respawn above clears ent->dead, so this asks what it was, which is the
  // question the guarded code replaced an `else` on
  if (!wasDead) {
    G_Tech_ClientThink(ent);
  }
#endif

  cl->latchedButtons = 0;
}

/**
 * @brief Returns true if `listener` may hear `speaker` on `channel`.
 * @details Voice is governed by the same rules as chat: an administratively muted player is not
 * heard, the team channel reaches only teammates, and a spectator is held to other spectators
 * wherever g_spectatorChat says chat would be.
 */
bool G_ClientCanHearVoice(const GameClient *speaker, const GameClient *listener, uint8_t channel) {

  if (!speaker || !listener) {
    return false;
  }

  if (speaker->persistent.muted) {
    return false;
  }

  switch (channel) {

    case VOICE_CHANNEL_TEAM:
      return G_OnSameTeam(speaker, listener);

    case VOICE_CHANNEL_ALL:
      if (speaker->persistent.spectator && !g_spectatorChat->integer) {
        return listener->persistent.spectator;
      }
      return true;

    default:
      // a channel this game does not define is refused, rather than widened to everyone: the byte
      // arrives from a client and nothing stops it being arbitrary
      return false;
  }
}
