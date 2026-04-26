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
 * @brief Finalizes linear movement, snapping the entity to its destination and translating any riding entities.
 */
static void G_MoveInfo_Linear_Done(g_entity_t *ent) {

  const vec3_t snap = Vec3_Subtract(ent->move_info.dest, ent->s.origin);

  const box3_t old_bounds = ent->abs_bounds;

  ent->s.origin = ent->move_info.dest;

  gi.LinkEntity(ent);

  // Translate riding entities by the same snap delta so they
  // maintain ground contact. Without this, the position snap
  // bypasses G_Physics_Push_Translate and riders lose their
  // ground entity reference.
  if (!Vec3_Equal(snap, Vec3_Zero())) {
    const box3_t total_bounds = Box3_Union(old_bounds, ent->abs_bounds);
    g_entity_t *others[MAX_ENTITIES];
    const size_t len = gi.BoxEntities(total_bounds, others, lengthof(others), BOX_ALL);
    for (size_t i = 0; i < len; i++) {
      if (others[i]->ground.ent == ent) {
        others[i]->s.origin = Vec3_Add(others[i]->s.origin, snap);
        gi.LinkEntity(others[i]);
      }
    }
  }

  ent->velocity = Vec3_Zero();

  ent->move_info.current_speed = 0.0;

  ent->move_info.Done(ent);
}

/**
 * @brief Called each tick during the final approach of linear movement to detect when the destination is reached.
 */
static void G_MoveInfo_Linear_Final(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;

  vec3_t dir;
  const float distance = Vec3_DistanceDir(move->dest, ent->s.origin, &dir);

  if (distance == 0.0 || Vec3_Dot(dir, move->dir) < 0.0) {
    G_MoveInfo_Linear_Done(ent);
    return;
  }

  ent->Think = G_MoveInfo_Linear_Final;
  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Starts a move with constant velocity. The entity will think again when it
 * has reached its destination.
 */
static void G_MoveInfo_Linear_Constant(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;

  const float distance = Vec3_Distance(move->dest, ent->s.origin);
  move->const_frames = distance / move->speed * QUETOO_TICK_RATE;

  ent->velocity = Vec3_Scale(move->dir, move->speed);

  ent->next_think = g_level.time + move->const_frames * QUETOO_TICK_MILLIS;
  ent->Think = G_MoveInfo_Linear_Final;
}

/**
 * @brief Sets up a non-constant move, i.e. one that will accelerate near the beginning
 * and decelerate towards the end.
 */
static void G_MoveInfo_Linear_Accelerate(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;

  if (move->current_speed == 0.0) { // starting or restarting after being blocked

    const float distance = Vec3_Distance(move->dest, ent->s.origin);

    const float accel_time = move->speed / move->accel;
    const float decel_time = move->speed / move->decel;

    move->accel_frames = (int32_t)(accel_time * QUETOO_TICK_RATE);
    move->decel_frames = (int32_t)(decel_time * QUETOO_TICK_RATE);

    const float avg_speed = move->speed * 0.5f;

    const float accel_distance = avg_speed * accel_time;
    const float decel_distance = avg_speed * decel_time;

    if (accel_distance + decel_distance > distance) {
      G_Debug("Clamping acceleration for %s\n", etos(ent));

      // The continuous-time ramp formula is inaccurate for the small frame
      // counts that arise when the travel distance is shorter than the full
      // acceleration ramp.  Derive the correct frame counts directly from
      // the discrete per-tick physics model instead:
      //
      //   accel distance over N_a frames:  accel * dt^2 * N_a * (N_a+1) / 2
      //   decel distance over N_d frames:  decel * dt^2 * N_d * (N_d-1) / 2
      //   peak-speed constraint:           N_d = floor(N_a * accel / decel)
      //
      // Any gap not covered by accel/decel is padded with const frames at
      // peak speed so Final is only called with a sub-frame residual.
      const float dt2 = QUETOO_TICK_SECONDS * QUETOO_TICK_SECONDS;

      move->accel_frames = 0;
      move->decel_frames = 0;
      move->const_frames = 0;

      for (int32_t n = 1; ; n++) {
        const float peak = (float)n * move->accel * QUETOO_TICK_SECONDS;
        if (peak >= move->speed) {
          break; // reached full speed — should not occur in the clamped path
        }

        const int32_t nd = (int32_t)(peak / (move->decel * QUETOO_TICK_SECONDS));
        const float ad = move->accel * dt2 * (float)(n * (n + 1)) * 0.5f;
        const float dd = move->decel * dt2 * (float)(nd * (nd - 1)) * 0.5f;

        if (ad + dd > distance) {
          break;
        }

        move->accel_frames = n;
        move->decel_frames = nd;
      }

      const float peak_speed = (float)move->accel_frames * move->accel * QUETOO_TICK_SECONDS;
      if (peak_speed > 0.f) {
        const float ad = move->accel * dt2 * (float)(move->accel_frames * (move->accel_frames + 1)) * 0.5f;
        const float dd = move->decel * dt2 * (float)(move->decel_frames * (move->decel_frames - 1)) * 0.5f;
        const float remaining = distance - ad - dd;
        move->const_frames = remaining > 0.f ? (int32_t)floorf(remaining / (peak_speed * QUETOO_TICK_SECONDS)) : 0;
      }
    } else {
      const float const_distance = distance - accel_distance - decel_distance;
      move->const_frames = (int32_t)(const_distance / move->speed * QUETOO_TICK_RATE);
    }
  }

  // accelerate
  if (move->accel_frames) {
    move->current_speed += move->accel * QUETOO_TICK_SECONDS;
    if (move->current_speed > move->speed) {
      move->current_speed = move->speed;
    }
    move->accel_frames--;
  }

  // maintain speed — current_speed carries forward from the accel phase:
  // move->speed for the non-clamped path, or peak speed for the clamped path.
  else if (move->const_frames) {
    move->const_frames--;
  }

  // decelerate
  else if (move->decel_frames) {
    move->current_speed -= move->decel * QUETOO_TICK_SECONDS;
    if (move->current_speed < move->speed * 0.05f) {
      move->current_speed = move->speed * 0.05f;
    }
    move->decel_frames--;
  }

  // done
  else {
    G_MoveInfo_Linear_Final(ent);
    return;
  }

  ent->velocity = Vec3_Scale(move->dir, move->current_speed);

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
  ent->Think = G_MoveInfo_Linear_Accelerate;
}

/**
 * @brief Sets up movement for the specified entity. Both constant and
 * accelerative movements are initiated through this function. Animations are
 * also kicked off here.
 */
static void G_MoveInfo_Linear_Init(g_entity_t *ent, const vec3_t dest, void (*Done)(g_entity_t *)) {
  g_move_info_t *move = &ent->move_info;

  ent->velocity = Vec3_Zero();
  move->current_speed = 0.0;

  move->dest = dest;
  move->dir = Vec3_Subtract(dest, ent->s.origin);
  move->dir = Vec3_Normalize(move->dir);

  move->Done = Done;

  if (move->accel == 0.0 && move->decel == 0.0) { // constant
    const g_entity_t *master = (ent->flags & FL_TEAM_SLAVE) ? ent->team_master : ent;
    if (g_level.current_entity == master) {
      G_MoveInfo_Linear_Constant(ent);
    } else {
      ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
      ent->Think = G_MoveInfo_Linear_Constant;
    }
  } else { // accelerative
    ent->Think = G_MoveInfo_Linear_Accelerate;
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
  }
}

/**
 * @brief Finalizes angular movement by zeroing angular velocity and invoking the Done callback.
 */
static void G_MoveInfo_Angular_Done(g_entity_t *ent) {

  ent->avelocity = Vec3_Zero();
  ent->move_info.Done(ent);
}

/**
 * @brief Called during the final tick of angular movement to snap the entity to its exact destination angles.
 */
static void G_MoveInfo_Angular_Final(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;
  vec3_t delta;

  if (move->state == MOVE_STATE_GOING_UP) {
    delta = Vec3_Subtract(move->end_angles, ent->s.angles);
  } else {
    delta = Vec3_Subtract(move->start_angles, ent->s.angles);
  }

  if (Vec3_Equal(delta, Vec3_Zero())) {
    G_MoveInfo_Angular_Done(ent);
    return;
  }

  ent->avelocity = Vec3_Scale(delta, 1.0 / QUETOO_TICK_SECONDS);

  ent->Think = G_MoveInfo_Angular_Done;
  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Begins angular movement by computing angular velocity toward the target angles.
 */
static void G_MoveInfo_Angular_Begin(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;
  vec3_t delta;

  // set move to the vector needed to move
  if (move->state == MOVE_STATE_GOING_UP) {
    delta = Vec3_Subtract(move->end_angles, ent->s.angles);
  } else {
    delta = Vec3_Subtract(move->start_angles, ent->s.angles);
  }

  // calculate length of vector
  const float len = Vec3_Length(delta);

  // divide by speed to get time to reach dest
  const float time = len / move->speed;

  if (time < QUETOO_TICK_SECONDS) {
    G_MoveInfo_Angular_Final(ent);
    return;
  }

  const float frames = floor(time / QUETOO_TICK_SECONDS);

  // scale the move vector by the time spent traveling to get velocity
  ent->avelocity = Vec3_Scale(delta, 1.0 / time);

  // set next_think to trigger a think when dest is reached
  ent->next_think = g_level.time + frames * QUETOO_TICK_MILLIS;
  ent->Think = G_MoveInfo_Angular_Final;
}

/**
 * @brief Initializes angular movement for the specified entity, scheduling the beginning of rotation.
 */
static void G_MoveInfo_Angular_Init(g_entity_t *ent, void (*Done)(g_entity_t *)) {

  ent->avelocity = Vec3_Zero();

  ent->move_info.Done = Done;

  const g_entity_t *master = (ent->flags & FL_TEAM_SLAVE) ? ent->team_master : ent;
  if (g_level.current_entity == master) {
    G_MoveInfo_Angular_Begin(ent);
  } else {
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
    ent->Think = G_MoveInfo_Angular_Begin;
  }
}

/**
 * @brief When a `MOVE_TYPE_PUSH` or `MOVE_TYPE_STOP` is blocked, deal with the
 * obstacle by damaging it.
 */
static void G_MoveType_Push_Blocked(g_entity_t *ent, g_entity_t *other) {

  const vec3_t dir = ent->velocity;

  if (g_level.time < ent->touch_time) {
    return;
  }

  ent->touch_time = g_level.time + 1000;

  if (G_IsMeat(other)) {

    if (other->solid == SOLID_DEAD) {
      G_Damage(&(g_damage_t) {
        .target = other,
        .inflictor = ent,
        .attacker = NULL,
        .dir = dir,
        .point = other->s.origin,
        .normal = Vec3_Up(),
        .damage = 999,
        .knockback = 0,
        .flags = DMG_NO_ARMOR,
        .mod = MOD_CRUSH
      });
      if (other->in_use) {
        if (other->client) {
          gi.WriteByte(SV_CMD_TEMP_ENTITY);
          gi.WriteByte(TE_GIB);
          gi.WritePosition(other->s.origin);
          gi.Multicast(other->s.origin, MULTICAST_PVS);

          other->solid = SOLID_NOT;
        } else {
          G_Gib(other);
        }
      }
    } else if (other->solid == SOLID_BOX) {
      G_Damage(&(g_damage_t) {
        .target = other,
        .inflictor = ent,
        .attacker = NULL,
        .dir = dir,
        .point = other->s.origin,
        .normal = Vec3_Up(),
        .damage = ent->damage,
        .knockback = 0,
        .flags = DMG_NO_ARMOR,
        .mod = MOD_CRUSH
      });
    } else {
      G_Debug("Unhandled blocker: %s: %s\n", etos(ent), etos(other));
    }
  } else {
    G_Damage(&(g_damage_t) {
      .target = other,
      .inflictor = ent,
      .attacker = NULL,
      .dir = dir,
      .point = other->s.origin,
      .normal = Vec3_Up(),
      .damage = 999,
      .knockback = 0,
      .flags = 0,
      .mod = MOD_CRUSH
    });
    if (other->in_use) {
      G_Explode(other, 60, 60, 80.0, 0);
    }
  }
}

#define PLAT_LOW_TRIGGER  1

static void G_func_plat_GoingDown(g_entity_t *ent);

/**
 * @brief Called when a platform reaches its top (raised) position, scheduling the return trip.
 */
static void G_func_plat_Top(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_end) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_end,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = 0;
  }

  ent->move_info.state = MOVE_STATE_TOP;

  ent->Think = G_func_plat_GoingDown;
  ent->next_think = g_level.time + 3000;
}

/**
 * @brief Called when a platform reaches its bottom (lowered) position, marking movement complete.
 */
static void G_func_plat_Bottom(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_end) {
      vec3_t pos;

      pos = Box3_Center(ent->abs_bounds);
      pos.z = ent->abs_bounds.maxs.z;

      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_end,
        .origin = &pos,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = 0;
  }

  ent->move_info.state = MOVE_STATE_BOTTOM;
}

/**
 * @brief Initiates downward movement of a platform toward its lowered position.
 */
static void G_func_plat_GoingDown(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_start) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = ent->move_info.sound_middle;
  }

  ent->move_info.state = MOVE_STATE_GOING_DOWN;
  G_MoveInfo_Linear_Init(ent, ent->move_info.end_origin, G_func_plat_Bottom);
}

/**
 * @brief Initiates upward movement of a platform toward its raised position.
 */
static void G_func_plat_GoingUp(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_start) {
      vec3_t pos;

      pos = Box3_Center(ent->abs_bounds);
      pos.z = ent->abs_bounds.maxs.z;

      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .origin = &pos,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = ent->move_info.sound_middle;
  }

  ent->move_info.state = MOVE_STATE_GOING_UP;
  G_MoveInfo_Linear_Init(ent, ent->move_info.start_origin, G_func_plat_Top);
}

/**
 * @brief Handles a platform blocked by an obstacle, reversing its direction of travel.
 */
static void G_func_plat_Blocked(g_entity_t *ent, g_entity_t *other) {

  G_MoveType_Push_Blocked(ent, other);

  if (ent->move_info.state == MOVE_STATE_GOING_UP) {
    G_func_plat_GoingDown(ent);
  } else if (ent->move_info.state == MOVE_STATE_GOING_DOWN) {
    G_func_plat_GoingUp(ent);
  }
}

/**
 * @brief Handles use activation of a platform, starting its downward descent.
 */
static void G_func_plat_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->Think) {
    return; // already down
  }

  G_func_plat_GoingDown(ent);
}

/**
 * @brief Handles touch events on the platform trigger, sending the platform upward when a player steps on it.
 */
static void G_func_plat_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->client) {
    return;
  }

  if (other->dead) {
    return;
  }

  ent = ent->enemy; // now point at the plat, not the trigger

  if (ent->move_info.state == MOVE_STATE_BOTTOM) {
    G_func_plat_GoingUp(ent);
  } else if (ent->move_info.state == MOVE_STATE_TOP) {
    ent->next_think = g_level.time + 1000; // the player is still on the plat, so delay going down
  }
}

/**
 * @brief Creates the touch trigger volume used to automatically raise the platform.
 */
static void G_func_plat_CreateTrigger(g_entity_t *ent, float lip) {
  g_entity_t *trigger;

  // middle trigger
  trigger = G_AllocEntity(__func__);
  trigger->Touch = G_func_plat_Touch;
  trigger->move_type = MOVE_TYPE_NONE;
  trigger->solid = SOLID_TRIGGER;
  trigger->enemy = ent;

  box3_t bounds = Box3_Expand3(ent->bounds, Vec3(-16.f, -16.f, 0.f));
  bounds.maxs.z += lip;

  bounds.mins.z = bounds.maxs.z - (ent->pos1.z - ent->pos2.z + lip);

  if (ent->spawn_flags & PLAT_LOW_TRIGGER) {
    bounds.maxs.z = bounds.mins.z + lip;
  }

  if (bounds.maxs.x - bounds.mins.x <= 0) {
    bounds.mins.x = (ent->bounds.mins.x + ent->bounds.maxs.x) * 0.5;
    bounds.maxs.x = bounds.mins.x + 1;
  }
  if (bounds.maxs.y - bounds.mins.y <= 0) {
    bounds.mins.y = (ent->bounds.mins.y + ent->bounds.maxs.y) * 0.5;
    bounds.maxs.y = bounds.mins.y + 1;
  }

  trigger->bounds = bounds;

  gi.LinkEntity(trigger);
}

/*QUAKED func_plat (0 .5 .8) ? low_trigger
 Rising platform the player can ride to reach higher places. Platforms must be placed in the raised position, so they will operate and be lit correctly, but they spawn in the lowered position. If the platform is the target of a trigger or button, it will start out disabled and in the extended position.

 -------- Keys --------
 speed : The speed with which the platform moves (default 200).
 accel : The platform acceleration (default 500).
 lip : The lip remaining at end of move (default 8 units).
 height : If set, this will determine the extent of the platform's movement, rather than implicitly using the platform's height.
 sounds : The sound set for the platform (0 default, 1 stone, -1 silent).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 low_trigger : If set, the touch field for this platform will only exist at the lower position.
 */
void G_func_plat(g_entity_t *ent) {

  ent->s.angles = Vec3_Zero();

  ent->solid = SOLID_BSP;
  ent->move_type = MOVE_TYPE_PUSH;

  gi.SetModel(ent, ent->model);

  ent->Blocked = G_func_plat_Blocked;

  if (!ent->speed) {
    ent->speed = 200.0;
  }

  if (!ent->accel) {
    ent->accel = ent->speed * 2.0;
  }

  if (!ent->decel) {
    ent->decel = ent->accel;
  }

  if (!ent->damage) {
    ent->damage = 2;
  }

  float lip = gi.EntityValue(ent->def, "lip")->value;
  if (!lip) {
    lip = 8.0;
  }

  // pos1 is the top position, pos2 is the bottom
  ent->pos1 = ent->s.origin;
  ent->pos2 = ent->s.origin;

  const cm_entity_t *height = gi.EntityValue(ent->def, "height");
  if (height->parsed & ENTITY_INTEGER) { // use the specified height
    ent->pos2.z -= height->integer;
  } else { // or derive it from the model height
    ent->pos2.z -= Box3_Size(ent->bounds).z - lip;
  }

  ent->Use = G_func_plat_Use;

  G_func_plat_CreateTrigger(ent, lip); // the "start moving" trigger

  if (ent->target_name) {
    ent->move_info.state = MOVE_STATE_GOING_UP;
  } else {
    ent->s.origin = ent->pos2;
    ent->move_info.state = MOVE_STATE_BOTTOM;
  }

  gi.LinkEntity(ent);

  ent->move_info.speed = ent->speed;
  ent->move_info.accel = ent->accel;
  ent->move_info.decel = ent->decel;
  ent->move_info.wait = ent->wait;

  ent->move_info.start_origin = ent->pos1;
  ent->move_info.start_angles = ent->s.angles;
  ent->move_info.end_origin = ent->pos2;
  ent->move_info.end_angles = ent->s.angles;

  const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
  if (s != -1) {
    ent->move_info.sound_start = gi.SoundIndex(va("func/plat_start_%d", s + 1));
    ent->move_info.sound_middle = gi.SoundIndex(va("func/plat_middle_%d", s + 1));
    ent->move_info.sound_end = gi.SoundIndex(va("func/plat_end_%d", s + 1));
  }
}

#define ROTATE_START_ON    0x1
#define ROTATE_REVERSE    0x2
#define ROTATE_AXIS_X    0x4
#define ROTATE_AXIS_Y    0x8
#define ROTATE_TOUCH_PAIN  0x10
#define ROTATE_TOUCH_STOP  0x20

/**
 * @brief Damages entities that touch a rotating brush while it is in motion.
 */
static void G_func_rotating_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (ent->damage) {
    if (!Vec3_Equal(ent->avelocity, Vec3_Zero())) {
      G_Damage(&(g_damage_t) {
        .target = other,
        .inflictor = ent,
        .attacker = NULL,
        .dir = Vec3_Zero(),
        .point = other->s.origin,
        .normal = Vec3_Zero(),
        .damage = ent->damage,
        .knockback = 1,
        .flags = 0,
        .mod = MOD_CRUSH
      });
    }
  }
}

/**
 * @brief Toggles rotation of a func_rotating brush on or off when triggered.
 */
static void G_func_rotating_Use(g_entity_t *ent, g_entity_t *other,
                                g_entity_t *activator) {

  if (!Vec3_Equal(ent->avelocity, Vec3_Zero())) {
    ent->s.sound = 0;
    ent->avelocity = Vec3_Zero();
    ent->Touch = NULL;
  } else {
    ent->s.sound = ent->move_info.sound_middle;
    ent->avelocity = Vec3_Scale(ent->move_dir, ent->speed);
    if (ent->spawn_flags & ROTATE_TOUCH_PAIN) {
      ent->Touch = G_func_rotating_Touch;
    }
  }
}

/*QUAKED func_rotating (0 .5 .8) ? start_on reverse x_axis y_axis touch_pain stop
 Solid entity that rotates continuously. Rotates on the Z axis by default and requires an origin brush. It will always start on in the game and is not targetable.

 -------- Keys --------
 speed : The speed at which the entity rotates (default 100).
 dmg : The damage to inflict to players who touch or block the entity (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : The entity will spawn rotating.
 reverse : Will cause the entity to rotate in the opposite direction.
 x_axis : Rotate on the X axis.
 y_axis : Rotate on the Y axis.
 touch_pain : If set, any interaction with the entity will inflict damage to the player.
 stop : If set and the entity is blocked, the entity will stop rotating.
 */
void G_func_rotating(g_entity_t *ent) {

  ent->solid = SOLID_BSP;

  if (ent->spawn_flags & ROTATE_TOUCH_STOP) {
    ent->move_type = MOVE_TYPE_STOP;
  } else {
    ent->move_type = MOVE_TYPE_PUSH;
  }

  // set the axis of rotation
  ent->move_dir = Vec3_Zero();
  if (ent->spawn_flags & ROTATE_AXIS_X) {
    ent->move_dir.z = 1.0;
  } else if (ent->spawn_flags & ROTATE_AXIS_Y) {
    ent->move_dir.x = 1.0;
  } else {
    ent->move_dir.y = 1.0;
  }

  // check for reverse rotation
  if (ent->spawn_flags & ROTATE_REVERSE) {
    ent->move_dir = Vec3_Negate(ent->move_dir);
  }

  if (!ent->speed) {
    ent->speed = 100.0;
  }

  if (!ent->damage) {
    ent->damage = 2;
  }

  ent->Use = G_func_rotating_Use;
  ent->Blocked = G_MoveType_Push_Blocked;

  if (ent->spawn_flags & ROTATE_START_ON) {
    ent->Use(ent, NULL, NULL);
  }

  gi.SetModel(ent, ent->model);
  gi.LinkEntity(ent);
}

/**
 * @brief Called when a button has fully returned to its resting (bottom) position.
 */
static void G_func_button_Done(g_entity_t *ent) {
  ent->move_info.state = MOVE_STATE_BOTTOM;
}

/**
 * @brief Moves a button back to its starting position and re-enables damage if applicable.
 */
static void G_func_button_Reset(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;

  move->state = MOVE_STATE_GOING_DOWN;

  G_MoveInfo_Linear_Init(ent, move->start_origin, G_func_button_Done);

  if (ent->health) {
    ent->take_damage = true;
  }
}

/**
 * @brief Called when a button reaches its pressed position, firing targets and scheduling the reset.
 */
static void G_func_button_Wait(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;

  move->state = MOVE_STATE_TOP;

  G_UseTargets(ent, ent->activator);

  if (move->wait >= 0) {
    ent->next_think = g_level.time + move->wait * 1000;
    ent->Think = G_func_button_Reset;
  }
}

/**
 * @brief Initiates button press movement from its resting position toward its destination.
 */
static void G_func_button_Activate(g_entity_t *ent) {
  g_move_info_t *move = &ent->move_info;

  if (move->state == MOVE_STATE_GOING_UP || move->state == MOVE_STATE_TOP) {
    return;
  }

  move->state = MOVE_STATE_GOING_UP;

  if (move->sound_start && !(ent->flags & FL_TEAM_SLAVE)) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = move->sound_start,
      .entity = ent,
      .atten = SOUND_ATTEN_SQUARE
    }, MULTICAST_PHS);
  }

  G_MoveInfo_Linear_Init(ent, move->end_origin, G_func_button_Wait);
}

/**
 * @brief Handles use activation of a button by storing the activator and pressing the button.
 */
static void G_func_button_Use(g_entity_t *ent, g_entity_t *other,
                              g_entity_t *activator) {

  ent->activator = activator;
  G_func_button_Activate(ent);
}

/**
 * @brief Handles touch events on a button, pressing it when a live player makes contact.
 */
static void G_func_button_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->client) {
    return;
  }

  if (other->health <= 0) {
    return;
  }

  ent->activator = other;
  G_func_button_Activate(ent);
}

/**
 * @brief Handles button death (destroyed by damage), triggering it as if it were pressed.
 */
static void G_func_button_Die(g_entity_t *ent, g_entity_t *attacker,
                              uint32_t mod) {

  ent->health = ent->max_health;
  ent->take_damage = false;

  ent->activator = attacker;
  G_func_button_Activate(ent);
}

/*QUAKED func_button (0 .5 .8) ?
 When a button is touched by a player, it moves in the direction set by the "angle" key, triggers all its targets, stays pressed by the amount of time set by the "wait" key, then returns to its original position where it can be operated again.

 -------- Keys --------
 angle : Determines the direction in which the button will move (up = -1, down = -2).
 target : All entities with a matching target name will be triggered.
 speed : Speed of the button's displacement (default 40).
 wait : Number of seconds the button stays pressed (default 1, -1 = indefinitely).
 lip : Lip remaining at end of move (default 4 units).
 sounds : The sound set for the button (0 default, -1 silent).
 health : If set, the button must be killed instead of touched to use.
 targetname : The target name of this entity if it is to be triggered.
 */
void G_func_button(g_entity_t *ent) {
  vec3_t abs_move_dir;
  float dist;

  G_SetMoveDir(ent);
  ent->move_type = MOVE_TYPE_PUSH;
  ent->solid = SOLID_BSP;
  gi.SetModel(ent, ent->model);

  gi.LinkEntity(ent);

  if (gi.EntityValue(ent->def, "sounds")->integer != -1) {
    ent->move_info.sound_start = gi.SoundIndex("func/switch");
  }

  if (!ent->speed) {
    ent->speed = 40.0;
  }

  if (!ent->wait) {
    ent->wait = 3.0;
  }

  float lip = gi.EntityValue(ent->def, "lip")->value;
  if (!lip) {
    lip = 4.0;
  }

  ent->pos1 = ent->s.origin;
  abs_move_dir.x = fabsf(ent->move_dir.x);
  abs_move_dir.y = fabsf(ent->move_dir.y);
  abs_move_dir.z = fabsf(ent->move_dir.z);
  dist = abs_move_dir.x * ent->size.x + abs_move_dir.y * ent->size.y + abs_move_dir.z * ent->size.z - lip;
  ent->pos2 = Vec3_Fmaf(ent->pos1, dist, ent->move_dir);

  ent->Use = G_func_button_Use;

  if (ent->health) {
    ent->max_health = ent->health;
    ent->Die = G_func_button_Die;
    ent->take_damage = true;
  } else if (!ent->target_name) {
    ent->Touch = G_func_button_Touch;
  }

  ent->move_info.state = MOVE_STATE_BOTTOM;

  ent->move_info.speed = ent->speed;
  ent->move_info.wait = ent->wait;
  ent->move_info.start_origin = ent->pos1;
  ent->move_info.start_angles = ent->s.angles;
  ent->move_info.end_origin = ent->pos2;
  ent->move_info.end_angles = ent->s.angles;
}

#define DOOR_START_OPEN        0x1
#define DOOR_TOGGLE            0x2
#define DOOR_ROTATING_REVERSE  0x4
#define DOOR_ROTATING_X_AXIS   0x8
#define DOOR_ROTATING_Y_AXIS   0x10

static void G_func_door_GoingDown(g_entity_t *ent);

/**
 * @brief Called when a door reaches its fully open position, scheduling automatic closure.
 */
static void G_func_door_Top(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_end) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_end,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = 0;
  }

  ent->move_info.state = MOVE_STATE_TOP;

  if (ent->spawn_flags & DOOR_TOGGLE) {
    return;
  }

  if (ent->move_info.wait >= 0) {
    ent->Think = G_func_door_GoingDown;
    ent->next_think = g_level.time + ent->move_info.wait * 1000;
  }
}

/**
 * @brief Called when a door reaches its fully closed position, marking movement complete.
 */
static void G_func_door_Bottom(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_end) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_end,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = 0;
  }

  ent->move_info.state = MOVE_STATE_BOTTOM;
}

/**
 * @brief Initiates closing movement of a door toward its start (closed) origin.
 */
static void G_func_door_GoingDown(g_entity_t *ent) {
  if (!(ent->flags & FL_TEAM_SLAVE)) {
    if (ent->move_info.sound_start) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }
    ent->s.sound = ent->move_info.sound_middle;
  }
  if (ent->max_health) {
    ent->take_damage = true;
    ent->health = ent->max_health;
  }

  ent->move_info.state = MOVE_STATE_GOING_DOWN;
  if (g_strcmp0(ent->classname, "func_door_rotating")) {
    G_MoveInfo_Linear_Init(ent, ent->move_info.start_origin, G_func_door_Bottom);
  } else { // rotating
    G_MoveInfo_Angular_Init(ent, G_func_door_Bottom);
  }
}

/**
 * @brief Initiates opening movement of a door toward its end (open) origin.
 */
static void G_func_door_GoingUp(g_entity_t *ent, g_entity_t *activator) {

  if (ent->move_info.state == MOVE_STATE_GOING_UP) {
    return; // already going up
  }

  if (ent->move_info.state == MOVE_STATE_TOP) { // reset top wait time
    if (ent->move_info.wait >= 0) {
      ent->next_think = g_level.time + ent->move_info.wait * 1000;
    }
    return;
  }

  if (!(ent->flags & FL_TEAM_SLAVE)) {
    if (ent->move_info.sound_start) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }
    ent->s.sound = ent->move_info.sound_middle;
  }
  ent->move_info.state = MOVE_STATE_GOING_UP;
  if (g_strcmp0(ent->classname, "func_door_rotating")) {
    G_MoveInfo_Linear_Init(ent, ent->move_info.end_origin, G_func_door_Top);
  } else { // rotating
    G_MoveInfo_Angular_Init(ent, G_func_door_Top);
  }

  G_UseTargets(ent, activator);
}

/**
 * @brief Handles use activation of a door, toggling it open or closed for all team members.
 */
static void G_func_door_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {
  g_entity_t *e;

  if (ent->flags & FL_TEAM_SLAVE) {
    return;
  }

  if (ent->spawn_flags & DOOR_TOGGLE) {
    if (ent->move_info.state == MOVE_STATE_GOING_UP
            || ent->move_info.state == MOVE_STATE_TOP) {
      // trigger all paired doors
      for (e = ent; e; e = e->team_next) {
        e->message = NULL;
        e->Touch = NULL;
        G_func_door_GoingDown(e);
      }
      return;
    }
  }

  // trigger all paired doors
  for (e = ent; e; e = e->team_next) {
    e->message = NULL;
    e->Touch = NULL;
    G_func_door_GoingUp(e, activator);
  }
}

/**
 * @brief Touch callback for a door's proximity trigger, opening the door when a player enters the volume.
 */
static void G_func_door_TouchTrigger(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (other->health <= 0) {
    return;
  }

  if (!other->client) {
    return;
  }

  if (g_level.time < ent->touch_time) {
    return;
  }

  ent->touch_time = g_level.time + 1000;

  G_func_door_Use(ent->owner, other, other);
}

/**
 * @brief Adjusts the speeds of all door team members so they all complete their movement simultaneously.
 */
static void G_func_door_CalculateMove(g_entity_t *ent) {
  g_entity_t *e;

  if (ent->flags & FL_TEAM_SLAVE) {
    return; // only the team master does this
  }

  // find the smallest distance any member of the team will be moving
  float min = fabsf(ent->move_info.distance);
  for (e = ent->team_next; e; e = e->team_next) {
    float dist = fabsf(e->move_info.distance);
    if (dist < min) {
      min = dist;
    }
  }

  const float time = min / ent->move_info.speed;

  // adjust speeds so they will all complete at the same time
  for (e = ent; e; e = e->team_next) {
    const float new_speed = fabsf(e->move_info.distance) / time;
    const float ratio = new_speed / e->move_info.speed;
    if (e->move_info.accel == e->move_info.speed) {
      e->move_info.accel = new_speed;
    } else {
      e->move_info.accel *= ratio;
    }
    if (e->move_info.decel == e->move_info.speed) {
      e->move_info.decel = new_speed;
    } else {
      e->move_info.decel *= ratio;
    }
    e->move_info.speed = new_speed;
  }
}

/**
 * @brief Creates the proximity trigger volume that automatically opens a door when entered.
 */
static void G_func_door_CreateTrigger(g_entity_t *ent) {
  g_entity_t *trigger;

  if (ent->flags & FL_TEAM_SLAVE) {
    return; // only the team leader spawns a trigger
  }

  box3_t bounds = ent->abs_bounds;

  for (trigger = ent->team_next; trigger; trigger = trigger->team_next) {
    bounds = Box3_Union(bounds, trigger->abs_bounds);
  }

  // expand
  bounds = Box3_Expand3(bounds, Vec3(60.f, 60.f, 0.f));

  trigger = G_AllocEntity(__func__);
  trigger->bounds = bounds;
  trigger->owner = ent;
  trigger->solid = SOLID_TRIGGER;
  trigger->move_type = MOVE_TYPE_NONE;
  trigger->Touch = G_func_door_TouchTrigger;
  gi.LinkEntity(trigger);

  G_func_door_CalculateMove(ent);
}

/**
 * @brief Handles a door blocked by an obstacle, reversing its travel direction.
 */
static void G_func_door_Blocked(g_entity_t *ent, g_entity_t *other) {

  G_MoveType_Push_Blocked(ent, other);

  // if a door has a negative wait, it would never come back if blocked,
  // so let it just squash the object to death real fast
  if (ent->move_info.wait >= 0) {
    g_entity_t *e;
    if (ent->move_info.state == MOVE_STATE_GOING_DOWN) {
      for (e = ent->team_master; e; e = e->team_next) {
        G_func_door_GoingUp(e, e->activator);
      }
    } else {
      for (e = ent->team_master; e; e = e->team_next) {
        G_func_door_GoingDown(e);
      }
    }
  }
}

/**
 * @brief Handles door death (destroyed by damage), forcing the door open for all team members.
 */
static void G_func_door_Die(g_entity_t *ent, g_entity_t *attacker, uint32_t mod) {

  g_entity_t *e;

  for (e = ent->team_master; e; e = e->team_next) {
    e->health = ent->max_health;
    e->take_damage = false;
  }

  G_func_door_Use(ent->team_master, attacker, attacker);
}

/**
 * @brief Displays the door's locked message when a player first touches it.
 */
static void G_func_door_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->client) {
    return;
  }

  if (g_level.time < ent->touch_time) {
    return;
  }

  ent->touch_time = g_level.time + 10000;

  if (ent->message && strlen(ent->message)) {
    gi.WriteByte(SV_CMD_CENTER_PRINT);
    gi.WriteString(ent->message);
    gi.Unicast(other->client, true);
  }

  G_UnicastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.chat,
  }, other->client, true);
}

/*QUAKED func_door (0 .5 .8) ? start_open x x x toggle
 A sliding door. By default, doors open when a player walks close to them.

 -------- Keys --------
 message : An optional string printed when the door is first touched.
 angle : Determines the opening direction of the door (up = -1, down = -2).
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 wait : wait before returning (3 default, -1 = never return).
 lip : The lip remaining at end of move (default 8 units).
 sounds : The sound set for the door (0 default, 1 stone, -1 silent).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_open : The door to moves to its destination when spawned, and operates in reverse.
 toggle : The door will wait in both the start and end states for a trigger event.
 */
void G_func_door(g_entity_t *ent) {
  vec3_t abs_move_dir;

  if (!(gi.EntityValue(ent->def, "angle")->parsed & ENTITY_INTEGER)) {
    ent->s.angles = Vec3(0.0, -1.0, 0.0); // default to sliding up
  }

  G_SetMoveDir(ent);
  ent->move_type = MOVE_TYPE_PUSH;
  ent->solid = SOLID_BSP;
  gi.SetModel(ent, ent->model);

  ent->Blocked = G_func_door_Blocked;
  ent->Use = G_func_door_Use;

  if (!ent->speed) {
    ent->speed = 200.0;
  }

  if (!ent->accel) {
    ent->accel = ent->speed * 2.0;
  }

  if (!ent->decel) {
    ent->decel = ent->accel;
  }

  if (!ent->wait) {
    ent->wait = 3.0;
  }

  float lip = gi.EntityValue(ent->def, "lip")->value;
  if (!lip) {
    lip = 8.0;
  }

  if (!ent->damage) {
    ent->damage = 2;
  }

  // calculate second position
  ent->pos1 = ent->s.origin;
  abs_move_dir = Vec3_Fabsf(ent->move_dir);
  ent->move_info.distance = abs_move_dir.x * ent->size.x +
                                   abs_move_dir.y * ent->size.y +
                                   abs_move_dir.z * ent->size.z - lip;

  ent->pos2 = Vec3_Fmaf(ent->pos1, ent->move_info.distance, ent->move_dir);

  // if it starts open, switch the positions
  if (ent->spawn_flags & DOOR_START_OPEN) {
    ent->s.origin = ent->pos2;
    ent->pos2 = ent->pos1;
    ent->pos1 = ent->s.origin;
  }

  gi.LinkEntity(ent);

  ent->move_info.state = MOVE_STATE_BOTTOM;

  if (ent->health) {
    ent->take_damage = true;
    ent->Die = G_func_door_Die;
    ent->max_health = ent->health;
  } else if (ent->target_name && ent->message) {
    ent->Touch = G_func_door_Touch;
  }

  ent->move_info.speed = ent->speed;
  ent->move_info.accel = ent->accel;
  ent->move_info.decel = ent->decel;
  ent->move_info.wait = ent->wait;

  ent->move_info.start_origin = ent->pos1;
  ent->move_info.start_angles = ent->s.angles;
  ent->move_info.end_origin = ent->pos2;
  ent->move_info.end_angles = ent->s.angles;

  const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
  if (s != -1) {
    ent->move_info.sound_start = gi.SoundIndex(va("func/door_start_%d", s + 1));
    ent->move_info.sound_middle = gi.SoundIndex(va("func/door_middle_%d", s + 1));
    ent->move_info.sound_end = gi.SoundIndex(va("func/door_end_%d", s + 1));
  }

  // to simplify logic elsewhere, make non-teamed doors into a team of one
  if (!ent->team) {
    ent->team_master = ent;
  }

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
  if (ent->health || ent->target_name) {
    ent->Think = G_func_door_CalculateMove;
  } else {
    ent->Think = G_func_door_CreateTrigger;
  }
}

/*QUAKED func_door_rotating (0 .5 .8) ? start_open reverse toggle x_axis y_axis
 A door which rotates about an origin on its Z axis. By default, doors open when a player walks close to them.

 -------- Keys --------
 message : An optional string printed when the door is first touched.
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 rotation : The rotation the door will open, in degrees (default 90).
 wait : wait before returning (3 default, -1 = never return).
 sounds : The sound set for the door (0 default, 1 stone, -1 silent).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_open : The door to moves to its destination when spawned, and operates in reverse.
 toggle : The door will wait in both the start and end states for a trigger event.
 reverse : The door will rotate in the opposite (negative) direction along its axis.
 x_axis : The door will rotate along its X axis.
 y_axis : The door will rotate along its Y axis.
 */
void G_func_door_rotating(g_entity_t *ent) {
  ent->s.angles = Vec3_Zero();

  // set the axis of rotation
  ent->move_dir = Vec3_Zero();
  if (ent->spawn_flags & DOOR_ROTATING_X_AXIS) {
    ent->move_dir.z = 1.0;
  } else if (ent->spawn_flags & DOOR_ROTATING_Y_AXIS) {
    ent->move_dir.x = 1.0;
  } else {
    ent->move_dir.y = 1.0;
  }

  // check for reverse rotation
  if (ent->spawn_flags & DOOR_ROTATING_REVERSE) {
    ent->move_dir = Vec3_Negate(ent->move_dir);
  }

  const float rotation = gi.EntityValue(ent->def, "rotation")->value ?: 90.f;

  ent->pos1 = ent->s.angles;
  ent->pos2 = Vec3_Fmaf(ent->s.angles, rotation, ent->move_dir);
  ent->move_info.distance = rotation;

  ent->move_type = MOVE_TYPE_PUSH;
  ent->solid = SOLID_BSP;
  gi.SetModel(ent, ent->model);

  ent->Blocked = G_func_door_Blocked;
  ent->Use = G_func_door_Use;

  if (!ent->speed) {
    ent->speed = 100.0;
  }
  if (!ent->accel) {
    ent->accel = ent->speed;
  }
  if (!ent->decel) {
    ent->decel = ent->speed;
  }

  if (!ent->wait) {
    ent->wait = 3.0;
  }
  if (!ent->damage) {
    ent->damage = 2;
  }

  // if it starts open, switch the positions
  if (ent->spawn_flags & DOOR_START_OPEN) {
    ent->s.angles = ent->pos2;
    ent->pos2 = ent->pos1;
    ent->pos1 = ent->s.angles;
    ent->move_dir = Vec3_Negate(ent->move_dir);
  }

  if (ent->health) {
    ent->take_damage = true;
    ent->Die = G_func_door_Die;
    ent->max_health = ent->health;
  }

  if (ent->target_name && ent->message) {
    ent->Touch = G_func_door_Touch;
  }

  ent->move_info.state = MOVE_STATE_BOTTOM;
  ent->move_info.speed = ent->speed;
  ent->move_info.accel = ent->accel;
  ent->move_info.decel = ent->decel;
  ent->move_info.wait = ent->wait;

  ent->move_info.start_origin = ent->s.origin;
  ent->move_info.start_angles = ent->pos1;
  ent->move_info.end_origin = ent->s.origin;
  ent->move_info.end_angles = ent->pos2;

  const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
  if (s != -1) {
    ent->move_info.sound_middle = gi.SoundIndex(va("func/door_middle_%d", s + 1));
  }

  // to simplify logic elsewhere, make non-teamed doors into a team of one
  if (!ent->team) {
    ent->team_master = ent;
  }

  gi.LinkEntity(ent);

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
  if (ent->health || ent->target_name) {
    ent->Think = G_func_door_CalculateMove;
  } else {
    ent->Think = G_func_door_CreateTrigger;
  }
}

#define SECRET_ALWAYS_SHOOT    1
#define SECRET_FIRST_LEFT    2
#define SECRET_FIRST_DOWN    4

static void G_func_door_secret_Move1(g_entity_t *ent);
static void G_func_door_secret_Move2(g_entity_t *ent);
static void G_func_door_secret_Move3(g_entity_t *ent);
static void G_func_door_secret_Move4(g_entity_t *ent);
static void G_func_door_secret_Move5(g_entity_t *ent);
static void G_func_door_secret_Move6(g_entity_t *ent);
static void G_func_door_secret_Done(g_entity_t *ent);

/**
 * @brief Handles use activation of a secret door, beginning its two-stage open sequence.
 */
static void G_func_door_secret_Use(g_entity_t *ent, g_entity_t *other,
                                   g_entity_t *activator) {

  // make sure we're not already moving
  if (!Vec3_Equal(ent->s.origin, Vec3_Zero())) {
    return;
  }

  G_MoveInfo_Linear_Init(ent, ent->pos1, G_func_door_secret_Move1);

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_start) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = ent->move_info.sound_middle;
  }
}

/**
 * @brief First stage pause of the secret door open sequence before beginning lateral movement.
 */
static void G_func_door_secret_Move1(g_entity_t *ent) {

  ent->next_think = g_level.time + 1000;
  ent->Think = G_func_door_secret_Move2;
}

/**
 * @brief Second stage of the secret door sequence, sliding the door laterally.
 */
static void G_func_door_secret_Move2(g_entity_t *ent) {

  G_MoveInfo_Linear_Init(ent, ent->pos2, G_func_door_secret_Move3);
}

/**
 * @brief Third stage of the secret door sequence, pausing at the open position before returning.
 */
static void G_func_door_secret_Move3(g_entity_t *ent) {

  if (ent->wait == -1.0) {
    return;
  }

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_end) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_end,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = 0;
  }

  ent->next_think = g_level.time + ent->wait * 1000;
  ent->Think = G_func_door_secret_Move4;
}

/**
 * @brief Fourth stage of the secret door sequence, starting the return slide.
 */
static void G_func_door_secret_Move4(g_entity_t *ent) {

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_start) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = ent->move_info.sound_middle;
  }

  G_MoveInfo_Linear_Init(ent, ent->pos1, G_func_door_secret_Move5);
}

/**
 * @brief Fifth stage pause of the secret door return sequence before the final slide back.
 */
static void G_func_door_secret_Move5(g_entity_t *ent) {

  ent->next_think = g_level.time + 1000;
  ent->Think = G_func_door_secret_Move6;
}

/**
 * @brief Sixth stage of the secret door sequence, sliding the door back to its origin.
 */
static void G_func_door_secret_Move6(g_entity_t *ent) {

  G_MoveInfo_Linear_Init(ent, Vec3_Zero(), G_func_door_secret_Done);
}

/**
 * @brief Completes the secret door movement sequence, restoring damage if applicable.
 */
static void G_func_door_secret_Done(g_entity_t *ent) {

  if (!(ent->target_name) || (ent->spawn_flags & SECRET_ALWAYS_SHOOT)) {
    ent->dead = true;
    ent->take_damage = true;
  }

  if (!(ent->flags & FL_TEAM_SLAVE)) {

    if (ent->move_info.sound_end) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_end,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }

    ent->s.sound = 0;
  }
}

/**
 * @brief Handles a secret door blocked by a player, dealing damage and delaying retraction.
 */
static void G_func_door_secret_Blocked(g_entity_t *ent, g_entity_t *other) {

  if (!other->client) {
    return;
  }

  if (g_level.time < ent->touch_time) {
    return;
  }

  ent->touch_time = g_level.time + 500;

  G_Damage(&(g_damage_t) {
    .target = other,
    .inflictor = ent,
    .attacker = ent,
    .dir = Vec3_Zero(),
    .point = other->s.origin,
    .normal = Vec3_Zero(),
    .damage = ent->damage,
    .knockback = 1,
    .flags = 0,
    .mod = MOD_CRUSH
  });

  ent->next_think = g_level.time + 1;
}

/**
 * @brief Handles secret door death (shot open), triggering it as if it were used.
 */
static void G_func_door_secret_Die(g_entity_t *ent, g_entity_t *attacker, uint32_t mod) {

  ent->take_damage = false;
  G_func_door_secret_Use(ent, attacker, attacker);
}

/*QUAKED func_door_secret (0 .5 .8) ? always_shoot 1st_left 1st_down
 A secret door which opens when shot, or when targeted. The door first slides
 back, and then to the side.

 -------- Keys --------
 angle : The angle at which the door opens.
 message : An optional string printed when the door is first touched.
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 wait : wait before returning (3 default, -1 = never return).
 lip : The lip remaining at end of move (default 8 units).
 sounds : The sound set for the door (0 default, 1 stone, -1 silent).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
always_shoot : The door will open when shot, even if it is targeted.
first_left : The door will first slide to the left.
first_down : The door will first slide down.
*/
void G_func_door_secret(g_entity_t *ent) {
  vec3_t forward, right, up;

  ent->move_type = MOVE_TYPE_PUSH;
  ent->solid = SOLID_BSP;
  gi.SetModel(ent, ent->model);

  ent->Blocked = G_func_door_secret_Blocked;
  ent->Use = G_func_door_secret_Use;

  if (!(ent->target_name) || (ent->spawn_flags & SECRET_ALWAYS_SHOOT)) {
    ent->dead = true;
    ent->take_damage = true;
    ent->Die = G_func_door_secret_Die;
  }

  float lip = gi.EntityValue(ent->def, "lip")->value;
  if (!lip) {
      lip = 8.0;
  }

  if (!ent->damage) {
    ent->damage = 2;
  }

  if (!ent->wait) {
    ent->wait = 5.0;
  }

  if (!ent->speed) {
    ent->speed = 50.0;
  }

  ent->move_info.speed = ent->speed;

  const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
  if (s != -1) {
    ent->move_info.sound_start = gi.SoundIndex(va("func/door_start_%d", s + 1));
    ent->move_info.sound_middle = gi.SoundIndex(va("func/door_middle_%d", s + 1));
    ent->move_info.sound_end = gi.SoundIndex(va("func/door_end_%d", s + 1));
  }

  // calculate positions
  Vec3_Vectors(ent->s.angles, &forward, &right, &up);
  ent->s.angles = Vec3_Zero();

  const float side = 1.0 - (ent->spawn_flags & SECRET_FIRST_LEFT);

  const float length = fabsf(Vec3_Dot(forward, ent->size));

  float width;
  if (ent->spawn_flags & SECRET_FIRST_DOWN) {
    width = fabsf(Vec3_Dot(up, ent->size));
  } else {
    width = fabsf(Vec3_Dot(right, ent->size));
  }

  if (ent->spawn_flags & SECRET_FIRST_DOWN) {
    ent->pos1 = Vec3_Fmaf(ent->s.origin, -1.0 * width, up);
  } else {
    ent->pos1 = Vec3_Fmaf(ent->s.origin, side * width, right);
  }

  ent->pos2 = Vec3_Fmaf(ent->pos1, length - lip, forward);

  if (ent->health) {
    ent->take_damage = true;
    ent->Die = G_func_door_Die;
    ent->max_health = ent->health;
  } else if (ent->target_name && ent->message) {
    gi.SoundIndex("misc/chat");
    ent->Touch = G_func_door_Touch;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Handles use activation of a func_wall, toggling its solidity on or off.
 */
static void G_func_wall_Use(g_entity_t *ent, g_entity_t *other,
                            g_entity_t *activator) {

  if (ent->solid == SOLID_NOT) {
    ent->solid = SOLID_BSP;
    ent->sv_flags &= ~SVF_NO_CLIENT;
    G_KillBox(ent);
  } else {
    ent->solid = SOLID_NOT;
    ent->sv_flags |= SVF_NO_CLIENT;
  }
  gi.LinkEntity(ent);

  if (!(ent->spawn_flags & 2)) {
    ent->Use = NULL;
  }
}

#define WALL_TRIGGER   0x1
#define WALL_TOGGLE    0x2
#define WALL_START_ON  0x4

#define WALL_SPAWN_FLAGS (WALL_TRIGGER | WALL_TOGGLE | WALL_START_ON)

/*QUAKED func_wall (0 .5 .8) ? triggered toggle start_on
 A solid that may spawn into existence via trigger.

 -------- Keys --------
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 triggered : The wall is inhibited until triggered, at which point it appears and kills everything in its way.
 toggle : The wall may be triggered off and on.
 start_on : The wall will initially be present, but can be toggled off.
 */
void G_func_wall(g_entity_t *ent) {
  ent->move_type = MOVE_TYPE_PUSH;
  gi.SetModel(ent, ent->model);

  if ((ent->spawn_flags & WALL_SPAWN_FLAGS) == 0) {
    ent->solid = SOLID_BSP;
    gi.LinkEntity(ent);
    return;
  }

  // it must be triggered to use start_on or toggle
  ent->spawn_flags |= WALL_TRIGGER;

  // and if it's start_on, it must be toggled
  if (ent->spawn_flags & WALL_START_ON) {
    ent->spawn_flags |= WALL_TOGGLE;
  }

  ent->Use = G_func_wall_Use;

  if (ent->spawn_flags & WALL_START_ON) {
    ent->solid = SOLID_BSP;
  } else {
    ent->solid = SOLID_NOT;
    ent->sv_flags |= SVF_NO_CLIENT;
  }

  gi.LinkEntity(ent);
}

/*QUAKED func_water(0 .5 .8) ? start_open
 A movable water brush, which must be targeted to operate.

 -------- Keys --------
 angle : Determines the opening direction (up = -1, down = -2)
 speed : The speed at which the water moves (default 25).
 wait : Delay in seconds before returning to the initial position (default -1).
 lip : Lip remaining at end of move (default 0 units).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags ---------
 start_open : If set, causes the water to move to its destination when spawned and operate in reverse.
 */
void G_func_water(g_entity_t *ent) {
  vec3_t abs_move_dir;

  G_SetMoveDir(ent);
  ent->move_type = MOVE_TYPE_PUSH;
  ent->solid = SOLID_BSP;
  gi.SetModel(ent, ent->model);

  // calculate second position
  ent->pos1 = ent->s.origin;
  abs_move_dir = Vec3_Fabsf(ent->move_dir);
  float lip = gi.EntityValue(ent->def, "lip")->value;
  ent->move_info.distance = abs_move_dir.x * ent->size.x +
                    abs_move_dir.y * ent->size.y +
                    abs_move_dir.z * ent->size.z - lip;

  ent->pos2 = Vec3_Fmaf(ent->pos1, ent->move_info.distance, ent->move_dir);

  // if it starts open, switch the positions
  if (ent->spawn_flags & DOOR_START_OPEN) {
    ent->s.origin = ent->pos2;
    ent->pos2 = ent->pos1;
    ent->pos1 = ent->s.origin;
  }

  ent->move_info.start_origin = ent->pos1;
  ent->move_info.start_angles = ent->s.angles;
  ent->move_info.end_origin = ent->pos2;
  ent->move_info.end_angles = ent->s.angles;

  ent->move_info.state = MOVE_STATE_BOTTOM;

  if (!ent->speed) {
    ent->speed = 25.0;
  }

  ent->move_info.speed = ent->speed;

  if (!ent->wait) {
    ent->wait = -1;
  }

  ent->move_info.wait = ent->wait;

  ent->Use = G_func_door_Use;

  if (ent->wait == -1) {
    ent->spawn_flags |= DOOR_TOGGLE;
  }

  gi.LinkEntity(ent);
}

#define TRAIN_START_ON    1
#define TRAIN_TOGGLE    2
#define TRAIN_BLOCK_STOPS  4

static void G_func_train_Next(g_entity_t *ent);

/**
 * @brief Called when a train arrives at a path_corner, firing pathtargets and scheduling the next segment.
 */
static void G_func_train_Wait(g_entity_t *ent) {

  const char *path_target = gi.EntityValue(ent->target_ent->def, "pathtarget")->nullable_string;
  if (path_target) {
    g_entity_t *target_ent = ent->target_ent;
    const char *target = target_ent->target;
    target_ent->target = path_target;
    G_UseTargets(target_ent, ent->activator);
    target_ent->target = target;

    // make sure we didn't get killed by a killtarget
    if (!ent->in_use) {
      return;
    }
  }

  if (ent->move_info.wait) {
    if (ent->move_info.wait > 0) {
      ent->next_think = g_level.time + (ent->move_info.wait * 1000);
      ent->Think = G_func_train_Next;
    } else if (ent->spawn_flags & TRAIN_TOGGLE) {
      G_func_train_Next(ent);
      ent->spawn_flags &= ~TRAIN_START_ON;
      ent->velocity = Vec3_Zero();
      ent->next_think = 0;
    }

    if (!(ent->flags & FL_TEAM_SLAVE)) {
      if (ent->move_info.sound_end) {
        G_MulticastSound(&(const g_play_sound_t) {
          .index = ent->move_info.sound_end,
          .entity = ent,
          .atten = SOUND_ATTEN_SQUARE
        }, MULTICAST_PHS);
      }
      ent->s.sound = 0;
    }
  } else {
    G_func_train_Next(ent);
  }
}

/**
 * @brief Advances the train to the next path_corner in its route.
 */
static void G_func_train_Next(g_entity_t *ent) {
  g_entity_t *target;
  vec3_t dest;
  bool first;

  first = true;
again:
  if (!ent->target) {
    return;
  }

  target = G_PickTarget(ent->target);
  if (!target) {
    G_Debug("%s has invalid target %s\n", etos(ent), ent->target);
    return;
  }

  ent->target = target->target;

  // check for a teleport path_corner
  if (target->spawn_flags & 1) {
    if (!first) {
      G_Debug("%s has teleport path_corner %s\n", etos(ent), etos(target));
      return;
    }
    first = false;
    ent->s.origin = Vec3_Subtract(target->s.origin, ent->bounds.mins);
    if (!(target->spawn_flags & 2)) {
      ent->s.event = EV_CLIENT_TELEPORT;
    }
    gi.LinkEntity(ent);
    goto again;
  }

  ent->move_info.wait = target->wait;
  ent->target_ent = target;

  if (!(ent->flags & FL_TEAM_SLAVE)) {
    if (ent->move_info.sound_start) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .entity = ent,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }
    ent->s.sound = ent->move_info.sound_middle;
  }

  dest = Vec3_Subtract(target->s.origin, ent->bounds.mins);
  ent->move_info.state = MOVE_STATE_TOP;
  ent->move_info.start_origin = ent->s.origin;
  ent->move_info.end_origin = dest;
  G_MoveInfo_Linear_Init(ent, dest, G_func_train_Wait);
  ent->spawn_flags |= TRAIN_START_ON;
}

/**
 * @brief Resumes train movement toward the current target path_corner after a stop.
 */
static void G_func_train_Resume(g_entity_t *ent) {
  g_entity_t *target;
  vec3_t dest;

  target = ent->target_ent;

  dest = Vec3_Subtract(target->s.origin, ent->bounds.mins);
  ent->move_info.state = MOVE_STATE_TOP;
  ent->move_info.start_origin = ent->s.origin;
  ent->move_info.end_origin = dest;
  G_MoveInfo_Linear_Init(ent, dest, G_func_train_Wait);
  ent->spawn_flags |= TRAIN_START_ON;
}

/**
 * @brief Locates the initial path_corner and positions the train at its starting origin.
 */
static void G_func_train_Find(g_entity_t *ent) {
  g_entity_t *target;

  if (!ent->target) {
    G_Debug("No target specified\n");
    return;
  }
  target = G_PickTarget(ent->target);
  if (!target) {
    G_Debug("Target \"%s\" not found\n", ent->target);
    return;
  }
  ent->target = target->target;

  ent->s.origin = Vec3_Subtract(target->s.origin, ent->bounds.mins);
  gi.LinkEntity(ent);

  // if not triggered, start immediately
  if (!ent->target_name) {
    ent->spawn_flags |= TRAIN_START_ON;
  }

  if (ent->spawn_flags & TRAIN_START_ON) {
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
    ent->Think = G_func_train_Next;
    ent->activator = ent;
  }
}

/**
 * @brief Handles use activation of a train, toggling movement on or off.
 */
static void G_func_train_Use(g_entity_t *ent, g_entity_t *other,
                             g_entity_t *activator) {
  ent->activator = activator;

  if (ent->spawn_flags & TRAIN_START_ON) {
    if (!(ent->spawn_flags & TRAIN_TOGGLE)) {
      return;
    }
    ent->spawn_flags &= ~TRAIN_START_ON;
    ent->velocity = Vec3_Zero();
    ent->next_think = 0;
  } else {
    if (ent->target_ent) {
      G_func_train_Resume(ent);
    } else {
      G_func_train_Next(ent);
    }
  }
}

/*QUAKED func_train (0 .5 .8) ? start_on toggle block_stops
 Trains are moving solids that players can ride along a series of path_corners. The origin of
 each corner specifies the lower bounding point of the train at that corner. If the train is
 the target of a button or trigger, it will not begin moving until activated.

 -------- Keys --------
 speed : The speed with which the train moves (default 100).
 dmg : The damage inflicted on players who block the train (default 2).
 sound : The looping sound to play while the train is in motion.
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : If set, the train will begin moving once spawned.
 toggle : If set, the train will start or stop each time it is activated.
 block_stops : When blocked, stop moving and inflict no damage.
 */
void G_func_train(g_entity_t *ent) {
  ent->move_type = MOVE_TYPE_PUSH;

  ent->s.angles = Vec3_Zero();

  if (ent->spawn_flags & TRAIN_BLOCK_STOPS) {
    ent->damage = 0;
  } else {
    if (!ent->damage) {
      ent->damage = 100;
    }
  }
  ent->solid = SOLID_BSP;
  gi.SetModel(ent, ent->model);

  const char *sound = gi.EntityValue(ent->def, "sound")->string;
  if (*sound) {
    ent->move_info.sound_middle = gi.SoundIndex(sound);
  }

  if (!ent->speed) {
    ent->speed = 100.0;
  }

  ent->move_info.speed = ent->speed;

  ent->Use = G_func_train_Use;
  ent->Blocked = G_MoveType_Push_Blocked;

  gi.LinkEntity(ent);

  if (ent->target) {
    // start trains on the second frame, to make sure their targets have had
    // a chance to spawn
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
    ent->Think = G_func_train_Find;
  } else {
    G_Debug("No target: %s\n", vtos(ent->s.origin));
  }
}

/**
 * @brief Fires all targets of a func_timer and schedules the next timer tick.
 */
static void G_func_timer_Think(g_entity_t *ent) {

  G_UseTargets(ent, ent->activator);

  const uint32_t wait = ent->wait * 1000;
  const uint32_t rand = ent->random * 1000 * RandomRangef(-1.f, 1.f);

  ent->next_think = g_level.time + wait + rand;
}

/**
 * @brief Handles use activation of a func_timer, toggling it on or off.
 */
static void G_func_timer_Use(g_entity_t *ent, g_entity_t *other,
                             g_entity_t *activator) {
  ent->activator = activator;

  // if on, turn it off
  if (ent->next_think) {
    ent->next_think = 0;
    return;
  }

  // turn it on
  if (ent->delay) {
    ent->next_think = g_level.time + ent->delay * 1000;
  } else {
    G_func_timer_Think(ent);
  }
}

/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) start_on
 Time delay trigger that will continuously fire its targets after a preset time delay. The time delay can also be randomized. When triggered, the timer will toggle on/off.

 -------- Keys --------
 wait : Delay in seconds between each triggering of all targets (default 1).
 random : Random time variance in seconds added to "wait" delay (default 0).
 delay :  Additional delay before the first firing when start_on (default 0).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : If set, the timer will begin firing once spawned.
 */
void G_func_timer(g_entity_t *ent) {

  if (!ent->wait) {
    ent->wait = 1.0;
  }

  ent->Use = G_func_timer_Use;
  ent->Think = G_func_timer_Think;

  if (ent->random >= ent->wait) {
    ent->random = ent->wait - QUETOO_TICK_SECONDS;
    G_Debug("random >= wait: %s\n", vtos(ent->s.origin));
  }

  if (ent->spawn_flags & 1) {

    const uint32_t delay = ent->delay * 1000;
    const uint32_t wait = ent->wait * 1000;
    const uint32_t rand = ent->random * 1000 * RandomRangef(-1.f, 1.f);

    ent->next_think = g_level.time + delay + wait + rand;
    ent->activator = ent;
  }

  ent->sv_flags = SVF_NO_CLIENT;
}

/**
 * @brief Handles use activation of a func_conveyor, toggling the belt speed on or off.
 */
static void G_func_conveyor_Use(g_entity_t *ent, g_entity_t *other,
                                g_entity_t *activator) {
  if (ent->spawn_flags & 1) {
    ent->speed = 0;
    ent->spawn_flags &= ~1;
  } else {
    ent->speed = ent->count;
    ent->spawn_flags |= 1;
  }

  if (!(ent->spawn_flags & 2)) {
    ent->count = 0;
  }
}

/*QUAKED func_conveyor (0 .5 .8) ? start_on toggle
 Conveyors are stationary brushes that move what's on them. The brush should be have a surface with at least one current content enabled.

 -------- Keys --------
 speed : The speed at which objects on the conveyor are moved (default 100).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : The conveyor will be active immediately.
 toggle : The conveyor is toggled each time it is used.
 */
void G_func_conveyor(g_entity_t *ent) {
  if (!ent->speed) {
    ent->speed = 100;
  }

  if (!(ent->spawn_flags & 1)) {
    ent->count = ent->speed;
    ent->speed = 0;
  }

  ent->Use = G_func_conveyor_Use;

  gi.SetModel(ent, ent->model);
  ent->solid = SOLID_BSP;
  gi.LinkEntity(ent);
}
