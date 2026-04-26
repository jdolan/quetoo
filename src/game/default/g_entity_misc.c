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
 * @brief Handles touch events on a misc_teleporter, warping the touching entity to the destination.
 */
static void G_misc_teleporter_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!G_IsMeat(other)) {
    return;
  }

  const g_entity_t *dest = G_Find(NULL, EOFS(target_name), ent->target);

  if (!dest) {
    gi.Warn("Couldn't find destination\n");
    return;
  }

  // unlink to make sure it can't possibly interfere with G_KillBox
  gi.UnlinkEntity(other);

  other->s.origin = dest->s.origin;
  other->s.origin.z += 8.0;

  vec3_t forward;
  Vec3_Vectors(dest->s.angles, &forward, NULL, NULL);

  if (other->client) {
    // overwrite velocity and hold them in place briefly
    other->client->ps.pm_state.flags &= ~PMF_TIME_MASK;
    other->client->ps.pm_state.flags = PMF_TIME_TELEPORT;

    other->client->ps.pm_state.time = 20;

    // set delta angles
    other->client->ps.pm_state.delta_angles = Vec3_Subtract(dest->s.angles, other->client->cmd_angles);

    other->velocity = Vec3_Scale(forward, other->client->speed);

    other->client->cmd_angles = Vec3_Zero();
    other->client->angles = Vec3_Zero();
  } else {

    const vec3_t vel = Vec3(other->velocity.x, other->velocity.x, 0.0);
    other->velocity = Vec3_Scale(forward, Vec3_Length(vel));

    other->s.angles.y += dest->s.angles.y;
  }

  other->velocity.z = 150.0;

  // create the effects at the teleporter

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_TELEPORT);
  gi.WritePosition(Box3_Center(ent->abs_bounds));
  gi.Multicast(ent->s.origin, MULTICAST_PHS);

  // and set the event on the teleportee

  other->s.event = EV_CLIENT_TELEPORT;
  other->s.angles = Vec3_Zero();

  G_KillBox(other); // telefrag anyone in our spot

  gi.LinkEntity(other);
}

/**
 * @brief Creates bot node links
 */
static void G_misc_teleporter_Think(g_entity_t *ent) {
  const g_entity_t *dest = G_Find(NULL, EOFS(target_name), ent->target);

  if (!dest) {
    gi.Warn("Couldn't find destination\n");
    return;
  }

  // find nodes closest to src and dst
  const ai_node_id_t src_node = G_Ai_Node_FindClosest(ent->s.origin, 512.f, true, true);
  const ai_node_id_t dst_node = G_Ai_Node_FindClosest(dest->s.origin, 512.f, true, true);

  if (src_node != AI_NODE_INVALID && dst_node != AI_NODE_INVALID) {

    // make a new node on top of src so we touch the teleporter, connect
    // it to dst with a small cost

    const ai_node_id_t new_node = G_Ai_Node_Create(ent->s.origin);

    // use default cost for the entrance
    G_Ai_Node_Link(src_node, new_node, Vec3_Distance(G_Ai_Node_GetPosition(src_node), ent->s.origin));

    // small cost for teleport node
    G_Ai_Node_Link(new_node, dst_node, 1.f);

    ent->node = src_node;
  }
}

/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16) no_effects
 Warps players who touch this entity to the targeted misc_teleporter_dest entity.

 -------- Spawn flags --------
 no_effects : Suppress the default teleporter particle effects.
 */
void G_misc_teleporter(g_entity_t *ent) {
  vec3_t v;

  if (!ent->target) {
    G_Debug("No target specified\n");
    G_FreeEntity(ent);
    return;
  }

  ent->solid = SOLID_TRIGGER;
  ent->move_type = MOVE_TYPE_NONE;

  if (ent->model) { // model form, trigger_teleporter
    gi.SetModel(ent, ent->model);
  } else { // or model-less form, misc_teleporter
    ent->bounds = Box3(
      Vec3(-32.0, -32.0, -24.0),
      Vec3(32.0, 32.0, -16.0)
    );

    v = ent->s.origin;
    v.z -= 16.0;

    // add effect if ent is not buried and effect is not inhibited
    if (!gi.PointContents(v) && !(ent->spawn_flags & 1)) {
      ent->s.sound = gi.SoundIndex("misc/teleport_hum");
      ent->s.trail = TRAIL_TELEPORTER;
    }
  }

  ent->Touch = G_misc_teleporter_Touch;
  
  // create link to destination
  if (!G_Ai_InDeveloperMode()) {
    ent->Think = G_misc_teleporter_Think;
    ent->next_think = g_level.time + 1;
  }

  gi.LinkEntity(ent);
}

/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
 Teleport destination for misc_teleporters.

 -------- Keys --------
 angle : Direction in which player will look when teleported.
 targetname : The target name of this entity.
 */
void G_misc_teleporter_dest(g_entity_t *ent) {
  G_InitPlayerSpawn(ent);
}

/**
 * @brief Think callback for a flying fireball, destroying it when it lands or freeing it otherwise.
 */
static void G_misc_fireball_Think(g_entity_t *ent) {

  if (ent->ground.ent) {
    ent->solid = SOLID_NOT;

    ent->s.effects = EF_DESPAWN;

    ent->move_type = MOVE_TYPE_NO_CLIP;
    ent->velocity.z = -8.0;

    ent->Think = G_FreeEntity;
    ent->next_think = g_level.time + 3000;

    gi.LinkEntity(ent);
  } else {
    G_FreeEntity(ent);
  }
}

/**
 * @brief Handles touch events on a fireball projectile, dealing damage to entities it strikes.
 */
static void G_misc_fireball_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (g_level.time - ent->touch_time > 500) {
    ent->touch_time = g_level.time;

    G_Damage(&(g_damage_t) {
      .target = other,
      .inflictor = ent,
      .attacker = NULL,
      .dir = ent->velocity,
      .point = ent->s.origin,
      .normal = Vec3_Zero(),
      .damage = ent->damage,
      .knockback = ent->damage,
      .flags = 0,
      .mod = MOD_FIREBALL
    });
  }
}

/**
 * @brief Spawns a new fireball projectile and schedules the next emission from the emitter.
 */
static void G_misc_fireball_Fly(g_entity_t *ent) {
  static uint32_t count;

  g_entity_t *fireball = G_AllocEntity(__func__);

  fireball->s.origin = ent->s.origin;

  fireball->bounds = Box3f(6.f, 6.f, 6.f);

  Vec3_Vectors(ent->s.angles, &fireball->velocity, NULL, NULL);
  fireball->velocity = Vec3_Scale(fireball->velocity, ent->speed);

  for (int32_t i = 0; i < 3; i++) {
    fireball->velocity.xyz[i] += RandomRangef(-30.f, 30.f);
  }

  fireball->avelocity = Vec3(RandomRangef(-10.f, 10.f), RandomRangef(-10.f, 10.f), RandomRangef(-20.f, 20.f));

  fireball->s.trail = TRAIL_FIREBALL;

  fireball->solid = SOLID_TRIGGER;
  fireball->move_type = MOVE_TYPE_BOUNCE;

  fireball->s.model1 = g_media.models.fireball;
  fireball->damage = ent->damage;

  fireball->Touch = G_misc_fireball_Touch;

  fireball->Think = G_misc_fireball_Think;
  fireball->next_think = g_level.time + 3000;

  gi.LinkEntity(fireball);

  if (Randomf() < 0.33) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = gi.SoundIndex(va("ambient/lava_%d", (count++ % 3) + 1)),
      .entity = ent,
      .atten = SOUND_ATTEN_SQUARE
    }, MULTICAST_PHS);
  }

  ent->next_think = g_level.time + (ent->wait * 1000.0) + (ent->random * 1000 * RandomRangef(-1.f, 1.f));
}

/*QUAKED misc_fireball (1 0.3 0.1) (-6 -6 -6) (6 6 6)
 Spawns an intermittent fireball that damages players. These are typically used above lava traps for ambiance.

 -------- Keys --------
 angles : The angles at which the fireball will fly (default randomized).
 dmg : The damage inflicted to entities that touch the fireball (default 4).
 random : Random time variance in seconds added to "wait" delay (default 0.5 * wait).
 speed : The speed at which the fireball will fly (default 600.0).
 wait : The interval in seconds between fireball emissions (default 5.0).
 */
void G_misc_fireball(g_entity_t *ent) {

  for (int32_t i = 1; i < 4; i++) {
    gi.SoundIndex(va("ambient/lava_%d", i));
  }

  if (Vec3_Equal(ent->s.angles, Vec3_Zero())) {
    ent->s.angles = Vec3(-90.0, 0.0, 0.0);
  }

  if (ent->damage == 0) {
    ent->damage = 4;
  }

  if (ent->speed == 0.0) {
    ent->speed = 600.0;
  }

  if (ent->wait == 0.0) {
    ent->wait = 5.0;
  }

  if (ent->random == 0.0) {
    ent->random = ent->wait * 0.5;
  }

  ent->Think = G_misc_fireball_Fly;
  ent->next_think = g_level.time + (Randomf() * 1000);
}

