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
 * @brief Inspect all damage received this frame and play a pain sound if appropriate.
 */
static void G_ClientDamage(g_client_t *cl) {

  if (cl->damage_health || cl->damage_armor) {
    // play an appropriate pain sound
    if (g_level.time > cl->pain_time) {
      int32_t pain;

      cl->pain_time = g_level.time + 700;

      if (cl->entity->health < 25) {
        pain = 0;
      } else if (cl->entity->health < 50) {
        pain = 1;
      } else if (cl->entity->health < 75) {
        pain = 2;
      } else {
        pain = 3;
      }

      const vec3_t org = Vec3_Add(cl->ps.pm_state.origin, cl->ps.pm_state.view_offset);

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.pain[pain],
        .entity = cl->entity,
        .origin = &org,
      }, MULTICAST_PHS);
    }
  }

  // clear totals
  cl->damage_health = 0;
  cl->damage_armor = 0;
  cl->damage_inflicted = 0;
}

/**
 * @brief Handles water entry and exit
 */
static void G_ClientWaterInteraction(g_client_t *cl) {

  g_entity_t *ent = cl->entity;

  if (ent->move_type == MOVE_TYPE_NO_CLIP) {
    cl->drown_time = g_level.time + 12000; // don't need air
    return;
  }

  const pm_water_level_t water_level = ent->water_level;
  const pm_water_level_t old_water_level = cl->old_water_level;

  // if just entered a water volume, play a sound
  if (old_water_level <= WATER_NONE && water_level >= WATER_FEET) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = g_media.sounds.water_in,
      .entity = ent,
    }, MULTICAST_PHS);
  }

  // completely exited the water
  if (old_water_level >= WATER_FEET && water_level == WATER_NONE) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = g_media.sounds.water_out,
      .entity = ent,
    }, MULTICAST_PHS);
  }

  // same water level, head out of water
  if ((old_water_level == water_level) && (water_level >= WATER_FEET && water_level < WATER_UNDER)) {
    if (Vec3_Length(ent->velocity) > 10.f) {
      G_Ripple(ent, ent->abs_bounds.maxs, ent->abs_bounds.mins, 0.f, false);
    }
  }

  if (ent->dead == false) { // if we're alive, we can drown

    // head just coming out of water, play a gasp if we were down for a while
    if (old_water_level == WATER_UNDER && water_level != WATER_UNDER && (cl->drown_time - g_level.time) < 8000) {
      const vec3_t org = Vec3_Add(cl->ps.pm_state.origin, cl->ps.pm_state.view_offset);

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.gasp,
        .entity = ent,
        .origin = &org,
      }, MULTICAST_PHS);
    }

    // check for drowning
    if (water_level != WATER_UNDER) { // take some air, push out drown time
      cl->drown_time = g_level.time + 12000;
      ent->damage = 0;
    } else { // we're under water
      if (cl->drown_time < g_level.time && ent->health > 0) {
        cl->drown_time = g_level.time + 1000;

        // take more damage the longer under water
        ent->damage += 2;

        if (ent->damage > 12) {
          ent->damage = 12;
        }

        // play a gurp sound instead of a normal pain sound
        if (ent->health <= ent->damage) {
          ent->s.event = EV_CLIENT_DROWN;
        } else {
          ent->s.event = EV_CLIENT_GURP;
        }

        // suppress normal pain sound
        cl->pain_time = g_level.time;

        // and apply the damage
        G_Damage(&(g_damage_t) {
          .target = ent,
          .inflictor = NULL,
          .attacker = NULL,
          .dir = Vec3_Zero(),
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = ent->damage,
          .knockback = 0,
          .flags = DMG_NO_ARMOR,
          .mod = MOD_WATER
        });
      }
    }
  }

  // check for sizzle damage
  if (water_level && (ent->water_type & (CONTENTS_LAVA | CONTENTS_SLIME))) {
    if (cl->sizzle_time <= g_level.time) {
      cl->sizzle_time = g_level.time + 300;

      if (!ent->dead && (ent->water_type & CONTENTS_LAVA) && cl->pain_time <= g_level.time) {

        // play a sizzle sound instead of a normal pain sound
        ent->s.event = EV_CLIENT_SIZZLE;

        // suppress normal pain sound
        cl->pain_time = g_level.time + 800;
      }

      if (ent->water_type & CONTENTS_LAVA) {
        G_Damage(&(g_damage_t) {
          .target = ent,
          .inflictor = NULL,
          .attacker = NULL,
          .dir = Vec3_Zero(),
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = 3 * water_level,
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
          .damage = 1 * water_level,
          .knockback = 0,
          .flags = DMG_NO_ARMOR,
          .mod = MOD_SLIME
        });
      }
    }
  }

  cl->old_water_level = water_level;
}

/**
 * @brief Set the angles of the client's world model, after clamping them to sane
 * values.
 */
static void G_ClientWorldAngles(g_client_t *cl) {

  g_entity_t *ent = cl->entity;

  if (ent->dead) { // just lay there like a lump
    ent->s.angles.x = ent->s.angles.z = 0.0;
    return;
  }

  ent->s.angles.x = cl->angles.x / 1.5;
  ent->s.angles.y = cl->angles.y;

  // set roll based on lateral velocity and ground entity
  const float dot = Vec3_Dot(ent->velocity, cl->right);

  ent->s.angles.z = ent->ground.ent ? dot * 0.015 : dot * 0.005;

  // check for footsteps
  if (ent->ground.ent && ent->move_type == MOVE_TYPE_WALK && !ent->s.event) {

    if (cl->speed > 250.0 && cl->footstep_time < g_level.time) {
      cl->footstep_time = g_level.time + 275;
      ent->s.event = EV_CLIENT_FOOTSTEP;
    }
  }
}

/**
 * @brief Advances the death camera, which is armed by `G_ClientDie` for deaths
 * that aren't a frag by another player's weapon. The camera drifts up and away
 * from the point of death, carrying a fraction of the velocity the player died
 * with, and watches the corpse until they respawn.
 *
 * @remarks The camera is resolved entirely here, as an eye offset from the
 * corpse. Nothing about it is sent to the client beyond `PMF_DEATH_CAM`, so it
 * interpolates, replays in demos and propagates to chase cameras for free.
 */
static void G_ClientDeathCam(g_client_t *cl) {

  if (!(cl->ps.pm_state.flags & PMF_DEATH_CAM)) {
    return;
  }

  const g_entity_t *ent = cl->entity;

  // resolve the corpse's eyes directly, mirroring Pm_CheckDuck. the view offset
  // can't be used here: we overwrite it below, and pmove only reclaims its z
  // for the dead, so its x and y would feed the camera back into itself
  const vec3_t eyes = Vec3_Add(ent->s.origin,
                               Vec3(0.f, 0.f, (cl->ps.pm_state.flags & PMF_GIBLET) ? 0.f : -16.f));

  const float duration = Maxf(g_death_cam_time->value, QUETOO_TICK_MILLIS);

  const float elapsed = g_level.time - cl->death_cam_time;

  const float frac = Clampf01(elapsed / duration);
  const float prev = Clampf01((elapsed - QUETOO_TICK_MILLIS) / duration);

  // ease out towards the settled offset, applied as a delta so that it
  // composes with the inherited velocity rather than fighting it
  const float ease = (1.f - (1.f - frac) * (1.f - frac)) - (1.f - (1.f - prev) * (1.f - prev));

  cl->death_cam_origin = Vec3_Fmaf(cl->death_cam_origin, ease, cl->death_cam_offset);

  // and coast the inherited velocity to a stop over the same interval
  cl->death_cam_origin = Vec3_Fmaf(cl->death_cam_origin, QUETOO_TICK_SECONDS, cl->death_cam_velocity);
  cl->death_cam_velocity = Vec3_Scale(cl->death_cam_velocity, expf(-3.f * QUETOO_TICK_MILLIS / duration));

  // don't let the camera escape the room we died in. the trace runs from the
  // corpse's origin rather than its eyes, which sit close enough to the floor
  // to start solid, and the clamp is applied to the resolved position only, so
  // that grazing a wall doesn't permanently arrest the camera
  vec3_t origin = cl->death_cam_origin;

  const cm_trace_t tr = gi.Trace(ent->s.origin, origin, Box3f(16.f, 16.f, 16.f), ent,
                                 CONTENTS_MASK_CLIP_PLAYER);
  if (!tr.start_solid && !tr.all_solid) {
    origin = tr.end;
  }

  cl->ps.pm_state.view_offset = Vec3_Subtract(origin, ent->s.origin);

  // look at the corpse, in absolute terms
  const vec3_t dir = Vec3_Subtract(eyes, origin);

  if (Vec3_Length(dir) > 1.f) {
    const vec3_t angles = Vec3_Euler(Vec3_Normalize(dir));

    cl->death_cam_angles.x = angles.x;
    cl->death_cam_angles.z = 0.f;

    // yaw is meaningless when we're looking straight down at ourselves, and
    // solving for it anyway swings the camera through half a turn in a frame
    if (Vec3_Length(Vec3(dir.x, dir.y, 0.f)) > 16.f) {
      cl->death_cam_angles.y = angles.y;
    }
  }

  cl->ps.pm_state.view_angles = cl->death_cam_angles;
  cl->ps.pm_state.delta_angles = Vec3_Zero();
}

/**
 * @brief Adds view kick in the specified direction to the specified client.
 */
void G_ClientDamageKick(g_client_t *cl, const vec3_t dir, const float kick) {
  vec3_t ndir;

  ndir = Vec3_Normalize(dir);

  const float pitch = Vec3_Dot(ndir, cl->forward) * kick;
  cl->kick_angles.x += pitch;

  const float roll = Vec3_Dot(ndir, cl->right) * kick;
  cl->kick_angles.z += roll;
}

/**
 * @brief Adds view angle kick based on entity events (falling, landing, etc).
 */
static void G_ClientFallKick(g_client_t *cl, const float kick) {
  cl->kick_angles.x += kick;
}

/**
 * @brief Sends the kick angles accumulated this frame to the client.
 */
static void G_ClientKickAngles(g_client_t *cl) {

  switch (cl->entity->s.event) {
    case EV_CLIENT_LAND:
      G_ClientFallKick(cl, 2.0);
      break;
    case EV_CLIENT_FALL:
      G_ClientFallKick(cl, 3.0);
      break;
    case EV_CLIENT_FALL_FAR:
      G_ClientFallKick(cl, 4.0);
      break;
    default:
      break;
  }

  if (!Vec3_Equal(cl->kick_angles, Vec3_Zero())) {
    gi.WriteByte(SV_CMD_VIEW_KICK);
    gi.WriteAngle(cl->kick_angles.x);
    gi.WriteAngle(cl->kick_angles.z);
    gi.Unicast(cl, false);
  }

  cl->kick_angles = Vec3_Zero();
}

/**
 * @brief Sets the animation sequences for the specified entity. This is called
 * towards the end of each frame, after our ground entity and water level have
 * been resolved.
 */
static void G_ClientAnimation(g_client_t *cl) {

  g_entity_t *ent = cl->entity;

  if (ent->s.model1 != MODEL_CLIENT) {
    return;
  }

  // corpses animate to their final resting place

  if (ent->solid == SOLID_DEAD) {

    if (g_level.time >= cl->respawn_time) {
      switch (ent->s.animation1 & ANIM_MASK_VALUE) {
        case ANIM_BOTH_DEATH1:
        case ANIM_BOTH_DEATH2:
        case ANIM_BOTH_DEATH3:
          G_SetAnimation(cl, (ent->s.animation1 & ANIM_MASK_VALUE) + 1, false);
          break;
        default:
          break;
      }
    }
    return;
  }

  // no-clippers do not animate

  if (ent->move_type == MOVE_TYPE_NO_CLIP) {
    G_SetAnimation(cl, ANIM_TORSO_STAND1, false);
    G_SetAnimation(cl, ANIM_LEGS_JUMP1, false);
    return;
  }

  // check for falling

  if (!ent->ground.ent) { // not on the ground

    if (g_level.time - cl->jump_time > 400) {
      if (ent->water_level == WATER_UNDER && cl->speed > 10.0) { // swimming
        G_SetAnimation(cl, ANIM_LEGS_SWIM, false);
        return;
      }
      if (cl->ps.pm_state.flags & PMF_DUCKED) { // ducking
        G_SetAnimation(cl, ANIM_LEGS_IDLECR, false);
        return;
      }
    }

    bool jumping = G_IsAnimation(cl, ANIM_LEGS_JUMP1);
    jumping |= G_IsAnimation(cl, ANIM_LEGS_JUMP2);

    if (!jumping) {
      G_SetAnimation(cl, ANIM_LEGS_JUMP1, false);
    }

    return;
  }

  // duck, walk or run after landing

  if (g_level.time - 400 > cl->land_time && g_level.time - 50 > cl->ground_time) {

    vec3_t forward;
    
    const vec3_t euler = Vec3(0.0, ent->s.angles.y, 0.0);
    Vec3_Vectors(euler, &forward, NULL, NULL);

    const bool backwards = Vec3_Dot(ent->velocity, forward) < -0.1;

    if (cl->ps.pm_state.flags & PMF_DUCKED) { // ducked
      if (cl->speed < 1.0) {
        G_SetAnimation(cl, ANIM_LEGS_IDLECR, false);
      } else if (backwards) {
        G_SetAnimation(cl, ANIM_LEGS_WALKCR | ANIM_REVERSE_BIT, false);
      } else {
        G_SetAnimation(cl, ANIM_LEGS_WALKCR, false);
      }

      return;
    }

    if (cl->speed < 1.0 && !cl->cmd.forward && !cl->cmd.right) {
      G_SetAnimation(cl, ANIM_LEGS_IDLE, false);
      return;
    }

    entity_animation_t anim = ANIM_LEGS_RUN;

    if (cl->speed < 290.0) {
      anim = ANIM_LEGS_WALK;

      if (backwards) {
        anim |= ANIM_REVERSE_BIT;
      }
    } else {
      if (backwards) {
        anim = ANIM_LEGS_BACK;
      }
    }

    G_SetAnimation(cl, anim, false);
    return;
  }
}

/**
 * @brief Called for each client at the end of the server frame.
 */
void G_ClientEndFrame(g_client_t *cl) {

  // If the origin or velocity have changed since G_ClientThink(),
  // update the pm_state_t values. This will happen when the client
  // is pushed by another entity or kicked by an explosion.
  //
  // If it wasn't updated here, the view position would lag a frame
  // behind the body position when pushed -- "sinking into plats"
  cl->ps.pm_state.origin = cl->entity->s.origin;
  cl->ps.pm_state.velocity = cl->entity->velocity;

  // If in intermission, just set stats and scores and return
  if (g_level.intermission_time) {
    G_ClientStats(cl);
    G_ClientScores(cl);
    return;
  }

  // check for water entry / exit, burn from lava, slime, etc
  G_ClientWaterInteraction(cl);

  // set the stats for this client
  if (cl->persistent.spectator) {
    G_ClientSpectatorStats(cl);
  } else {
    G_ClientStats(cl);
  }

  // apply all the damage taken this frame
  G_ClientDamage(cl);

  // send the kick angles
  G_ClientKickAngles(cl);

  // and the angles on the world model
  G_ClientWorldAngles(cl);

  // detach the view if we died an inglorious death
  G_ClientDeathCam(cl);

  // update the player's animations
  G_ClientAnimation(cl);

  // if the scoreboard is up, update it
  if (cl->show_scores) {
    G_ClientScores(cl);
  }
}

/**
 * @brief The tail of the `G_FrameDidEnd` chain: a notification, so it does nothing.
 */
static void G_FrameDidEnd_Common(void) {
}

FrameDidEnd G_FrameDidEnd = G_FrameDidEnd_Common;

/**
 * @brief Finalizes all client frames and applies chase camera state.
 */
void G_EndClientFrames(void) {

  // finalize the player_state_t for this frame
  G_ForEachClient(cl, {
    if (cl->entity) {
      G_ClientEndFrame(cl);
    }
  });

  // render the nodes to the clients
  G_Ai_Node_Render();

  // now loop through again, and for chase camera users, copy the final player state
  G_ForEachClient(cl, {
    if (cl->entity && cl->chase_target) {
      G_ClientChaseThink(cl);
      G_ClientSpectatorStats(cl);
    }
  });

  G_FrameDidEnd();
}
