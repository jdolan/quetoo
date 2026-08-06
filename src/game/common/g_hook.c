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
 * @brief The hook owns its own configuration, media and enabled state so that a
 * module adopting it needs only `g_client_hook_t`, a `g_hook_style_t` in its
 * persistent client state, and the MOD_HOOK, TE_HOOK_IMPACT and TRAIL_HOOK wire
 * values.
 */

/**
 * @brief `g_module.h` function pointers.
 */
static struct {
  CheckCvars CheckCvars;
  TossInventory TossInventory;
  InitMedia InitMedia;
  ConfigureLevel ConfigureLevel;
  PrepareMove PrepareMove;
} previous;

static bool installed;

cvar_t *g_hook;
cvar_t *g_hook_auto_refire;
cvar_t *g_hook_distance;
cvar_t *g_hook_pull_speed;
cvar_t *g_hook_refire;
cvar_t *g_hook_sky;
cvar_t *g_hook_speed;
cvar_t *g_hook_style;

static struct {
  uint16_t model;
  uint16_t fire;
  uint16_t fly;
  uint16_t hit;
  uint16_t pull;
  uint16_t detach;
  uint16_t gibhit;
} g_hook_media;

/**
 * @brief True when the hook is available this level.
 */
static bool g_hook_enabled;

/**
 * @return True if the hook is enabled for this level.
 */
static bool G_Hook_Enabled(void) {
  return g_hook_enabled;
}

/**
 * @brief Takes the client's movement over while they are pulling on the hook,
 * and otherwise defers to previous.
 */
static void G_PrepareMove_Hook(g_client_t *cl, pm_move_t *pm) {

  if (!cl->hook.pull) {
    previous.PrepareMove(cl, pm);
    return;
  }

  switch (cl->persistent.hook_style) {
    case HOOK_SWING_MANUAL:
      pm->s.type = PM_HOOK_SWING_MANUAL;
      break;
    case HOOK_SWING_AUTO:
      pm->s.type = PM_HOOK_SWING_AUTO;
      break;
    default:
      pm->s.type = PM_HOOK_PULL;
      break;
  }

  pm->hook_pull_speed = g_hook_pull_speed->value;
}

/**
 * @brief Indexes the hook's model and sounds for this level.
 */
static void G_InitMedia_Hook(void) {

  previous.InitMedia();

  g_hook_media.model = gi.ModelIndex("models/grapplehook/tris");

  g_hook_media.fire = gi.SoundIndex("grapplehook/fire");
  g_hook_media.fly = gi.SoundIndex("grapplehook/fly");
  g_hook_media.hit = gi.SoundIndex("grapplehook/hit");
  g_hook_media.pull = gi.SoundIndex("grapplehook/pull");
  g_hook_media.detach = gi.SoundIndex("grapplehook/detach");
  g_hook_media.gibhit = gi.SoundIndex("grapplehook/gibhit");
}

/**
 * @brief Resolves whether the hook is available this level, and publishes the
 * pull speed the client predicts with.
 */
static void G_ConfigureLevel_Hook(void) {

  G_Hook_CheckState();

  gi.SetConfigString(CS_HOOK_PULL_SPEED, g_hook_pull_speed->string);

  previous.ConfigureLevel();
}

/**
 * @brief Applies the hook's own cvars.
 */
static bool G_CheckCvars_Hook(void) {

  if (g_hook->modified) {
    g_hook->modified = false;

    G_Hook_CheckState();

    gi.BroadcastPrint(PRINT_HIGH, "Hook has been %s\n", G_Hook_Enabled() ? "enabled" : "disabled");
  }

  if (g_hook_speed->modified) {
    g_hook_speed->modified = false;

    gi.BroadcastPrint(PRINT_HIGH, "Hook speed has been changed to %g\n", g_hook_speed->value);
  }

  if (g_hook_pull_speed->modified) {
    g_hook_pull_speed->modified = false;

    gi.BroadcastPrint(PRINT_HIGH, "Hook pull speed has been changed to %g\n", g_hook_pull_speed->value);

    gi.SetConfigString(CS_HOOK_PULL_SPEED, g_hook_pull_speed->string);
  }

  if (g_hook_style->modified) {
    g_hook_style->modified = false;

    // reset all the hook styles on the players
    G_ForEachClient(cl, {
      G_SetClientHookStyle(cl);
    });

    gi.BroadcastPrint(PRINT_HIGH, "Hook style has been changed to %s\n", g_hook_style->string);
  }

  return previous.CheckCvars();
}

/**
 * @brief Tosses the grapple a client leaving play is holding.
 */
static void G_TossInventory_Hook(g_client_t *cl) {

  G_HookDetach(cl);

  previous.TossInventory(cl);
}

/**
 * @brief Registers the hook's cvars and installs its hooks.
 */
void G_Hook_Init(void) {

  // G_Init runs on every server initialization, and the module is not always
  // unloaded in between, so installing twice would point previous at ourselves.
  if (!installed) {
    installed = true;

    previous.CheckCvars = G_CheckCvars;
    G_CheckCvars = G_CheckCvars_Hook;
    previous.TossInventory = G_TossInventory;
    G_TossInventory = G_TossInventory_Hook;

    previous.InitMedia = G_InitMedia;
    G_InitMedia = G_InitMedia_Hook;

    previous.ConfigureLevel = G_ConfigureLevel;
    G_ConfigureLevel = G_ConfigureLevel_Hook;

    previous.PrepareMove = G_PrepareMove;
    G_PrepareMove = G_PrepareMove_Hook;
  }

  g_hook = gi.AddCvar("g_hook", "default", CVAR_SERVER_INFO, "Whether to allow the hook to be used or not. \"default\" only allows hook in CTF; 1 is always allow, 0 is never allow.");
  g_hook_style = gi.AddCvar("g_hook_style", "default", 0, "Whether to allow only \"pull\", \"swing_manual\", \"swing_auto\" or any (\"default\") hook swing style.");
  g_hook_auto_refire = gi.AddCvar("g_hook_auto_refire", "0", 0, "If the hook automatically refires when it hits a non-solid surface, like players or weapon clips. (Currently non-functional)");
  g_hook_distance = gi.AddCvar("g_hook_distance", va("%.1f", PM_HOOK_DEF_DIST), 0, "The maximum distance the hook will travel.");
  g_hook_pull_speed = gi.AddCvar("g_hook_pull_speed", "800", 0, "The speed that you get pulled towards the hook.");
  g_hook_refire = gi.AddCvar("g_hook_refire", "0.25", 0, "The refire delay on the grapple hook in seconds.");
  g_hook_sky = gi.AddCvar("g_hook_sky", "0", CVAR_SERVER_INFO, "If enabled, the grapple hook attaches to sky surfaces rather than detaching.");
  g_hook_speed = gi.AddCvar("g_hook_speed", "1200", 0, "The speed that the hook will fly at.");

  g_hook_pull_speed->modified =
      g_hook_speed->modified =
      g_hook_style->modified =
      g_hook->modified = false;
}

/**
 * @brief Touch callback for the hook projectile; attaches to structural surfaces or deals damage and detaches on hitting enemies.
 */
static void G_HookProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  ent->s.sound = 0;

  const bool sky = G_IsSky(trace);

  if (!sky || g_hook_sky->integer) {

    if (G_IsStructural(trace) || (sky && g_hook_sky->integer) || (G_IsMeat(other) && G_OnSameTeam(other->client, ent->owner->client))) {

      ent->velocity = Vec3_Zero();
      ent->avelocity = Vec3_Zero();

      ent->owner->client->hook.pull = true;

      ent->move_type = MOVE_TYPE_THINK;
      ent->solid = SOLID_NOT;
      ent->bounds = Box3_Zero();
      ent->enemy = other;

      gi.LinkEntity(ent);

      ent->owner->client->ps.pm_state.hook_position = ent->s.origin;

      if (ent->owner->client->persistent.hook_style != HOOK_PULL) {
        const float distance = Vec3_Distance(ent->owner->s.origin, ent->s.origin);

        ent->owner->client->ps.pm_state.hook_length = Clampf(distance, PM_HOOK_MIN_DIST, g_hook_distance->value);
      }

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_HOOK_IMPACT);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    } else {

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_hook_media.gibhit,
        .entity = ent,
        .pitch = RandomRangei(-4, 5)
      }, MULTICAST_PHS);

      /*
      if (g_hook_auto_refire->integer) {
        G_HookThink(ent->owner, true);
      } else {*/
        ent->velocity = Vec3_Normalize(ent->velocity);

        G_Damage(&(g_damage_t) {
          .target = other,
          .inflictor = ent,
          .attacker = ent->owner,
          .dir = ent->velocity,
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = 5,
          .knockback = 0,
          .flags = 0,
          .mod = MOD_HOOK
        });

        G_HookDetach(ent->owner->client);
//      }
    }
  } else {
    /* Currently disabled due to bugs
    if (g_hook_auto_refire->integer) {
      G_HookThink(ent->owner, true);
    } else {
    */
      G_HookDetach(ent->owner->client);
//    }
  }
}

/**
 * @brief Think callback for the hook cable trail; updates beam endpoints and detaches if the hook exceeds maximum range.
 */
static void G_HookTrail_Think(g_entity_t *ent) {

  const g_entity_t *hook = ent->target_ent;
  g_client_t *cl = ent->owner->client;

  vec3_t forward, right, up, org;

  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.0);

  ent->s.origin = org;
  ent->s.termination = hook->s.origin;

  vec3_t distance;
  distance = Vec3_Subtract(org, hook->s.origin);

  if (Vec3_Length(distance) > g_hook_distance->value) {

    G_HookDetach(cl);
    return;
  }

  ent->next_think = g_level.time + 1;
  gi.LinkEntity(ent);
}

/**
 * @brief Think callback for the hook projectile; tracks attached movers and updates the hook position each tick.
 */
static void G_HookProjectile_Think(g_entity_t *ent) {

  // if we're attached to something, copy velocities
  if (ent->enemy) {
    g_entity_t *mover = ent->enemy;
    vec3_t move, amove, inverse_amove, forward, right, up, rotate, translate, delta;

    move = Vec3_Scale(mover->velocity, QUETOO_TICK_SECONDS);
    amove = Vec3_Scale(mover->avelocity, QUETOO_TICK_SECONDS);

    if (!Vec3_Equal(move, Vec3_Zero()) || !Vec3_Equal(amove, Vec3_Zero())) {
      inverse_amove = Vec3_Negate(amove);
      Vec3_Vectors(inverse_amove, &forward, &right, &up);

      // translate the pushed entity
      ent->s.origin = Vec3_Add(ent->s.origin, move);

      // then rotate the movement to comply with the pusher's rotation
      translate = Vec3_Subtract(ent->s.origin, mover->s.origin);

      rotate.x = Vec3_Dot(translate, forward);
      rotate.y = -Vec3_Dot(translate, right);
      rotate.z = Vec3_Dot(translate, up);

      delta = Vec3_Subtract(rotate, translate);

      ent->s.origin = Vec3_Add(ent->s.origin, delta);

      // FIXME: any way we can have the hook move on all axis?
      ent->s.angles.y += amove.y;
      ent->target_ent->s.angles.y += amove.y;

      gi.LinkEntity(ent);

      ent->owner->client->ps.pm_state.hook_position = ent->s.origin;
    }

    if ((ent->owner->client->persistent.hook_style == HOOK_PULL && Vec3_LengthSquared(ent->owner->velocity) > 128.0) ||
      ent->knockback != ent->owner->client->ps.pm_state.hook_length) {
      ent->s.sound = g_hook_media.pull;
      ent->knockback = ent->owner->client->ps.pm_state.hook_length;
    } else {
      ent->s.sound = 0;
    }
  }

  ent->next_think = g_level.time + 1;
}

/**
 * @brief Fires a grappling hook projectile from the specified entity in the given direction.
 */
g_entity_t *G_HookProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir) {
  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, g_hook_speed->value);
  projectile->avelocity = Vec3(0, 0, 500);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->move_type = MOVE_TYPE_FLY;
  projectile->Touch = G_HookProjectile_Touch;
  projectile->s.model1 = g_hook_media.model;
  projectile->Think = G_HookProjectile_Think;
  projectile->next_think = g_level.time + 1;
  projectile->s.sound = g_hook_media.fly;

  gi.LinkEntity(projectile);

  g_entity_t *trail = G_AllocEntity(__func__);

  projectile->target_ent = trail;
  trail->target_ent = projectile;

  trail->owner = ent;
  trail->solid = SOLID_NOT;
  trail->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  trail->move_type = MOVE_TYPE_THINK;
  trail->s.client = ent->s.client;
  trail->s.effects = EF_BEAM;
  trail->s.trail = TRAIL_HOOK;
  trail->Think = G_HookTrail_Think;
  trail->next_think = g_level.time + 1;

  G_HookTrail_Think(trail);

  // angle is used for rendering on client side
  trail->s.angles = projectile->s.angles;

  gi.LinkEntity(trail);
  return projectile;
}

/**
 * @brief Detach the player's hook if it's still attached.
 */
void G_HookDetach(g_client_t *cl) {

  if (!g_hook_enabled) {
    return;
  }

  if (!cl->hook.entity) {
    return;
  }

  // free entity
  if (cl->hook.entity->target_ent) {
    G_FreeEntity(cl->hook.entity->target_ent);
  }
  G_FreeEntity(cl->hook.entity);

  cl->hook.entity = NULL;

  // prevent hook spam
  if (!cl->hook.pull) {
    cl->hook.fire_time = g_level.time + SECONDS_TO_MILLIS(g_hook_refire->value);
  } else {
    // don't get hurt from sweet-ass hooking
    cl->land_time = g_level.time;
  }

  cl->hook.pull = false;

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_hook_media.detach,
    .entity = cl->entity,
    .pitch = RandomRangei(-4, 5)
  }, MULTICAST_PHS);

  // see if we can backflip for style points
  if (cl->entity->in_use && cl->entity->health > 0) {

    const vec3_t velocity = Vec3(cl->entity->velocity.x, cl->entity->velocity.y, 0.0);
    const float fwd_speed = Vec3_Length(velocity) / 1.75;

    if (cl->entity->velocity.z > 50 && cl->entity->velocity.z > fwd_speed) {
      G_SetAnimation(cl, ANIM_LEGS_JUMP2, true);
    }
  }
}

/**
 * @brief Handles the firing of the hook.
 */
static void G_HookCheckFire(g_client_t *cl, const bool refire) {

  // hook can fire, see if we should
  if (!refire && !(cl->latched_buttons & BUTTON_HOOK)) {
    return;
  }

  if (!refire) {

    // use small epsilon for low server frame rates
    if (cl->hook.fire_time > g_level.time + 1) {
      return;
    }

    cl->latched_buttons &= ~BUTTON_HOOK;
  } else {

    G_HookDetach(cl);
  }

  // fire away!
  vec3_t forward, right, up, org;
  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.0);

  cl->hook.pull = false;
  cl->hook.entity = G_HookProjectile(cl->entity, org, forward);

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_hook_media.fire,
    .entity = cl->entity,
    .pitch = RandomRangei(-4, 5)
  }, MULTICAST_PHS);

  cl->hook.think_time = g_level.time;
}

/**
 * @brief Handles management of the hook for a given player.
 */
void G_HookThink(g_client_t *cl, const bool refire) {

  // sanity checks
  if (!g_hook_enabled) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  if (G_Ai_InDeveloperMode()) {
    if (cl->hook.entity) {
      G_HookDetach(cl);
    }
    cl->latched_buttons &= ~BUTTON_HOOK;
    return;
  }

  // send off to the proper sub-function

  if (refire) {
    G_HookCheckFire(cl, true);
    return;
  }

  if (cl->hook.entity) {

    const bool is_manual_hook_swing = cl->persistent.hook_style == HOOK_SWING_MANUAL;
    const bool is_holding_hook = (cl->buttons & BUTTON_HOOK);
    const bool is_pressing_hook = (cl->latched_buttons & BUTTON_HOOK);

    if ((!is_manual_hook_swing && !is_holding_hook) || (is_manual_hook_swing && is_pressing_hook)) {

      G_HookDetach(cl);

      cl->latched_buttons &= ~BUTTON_HOOK;
      cl->hook.think_time = g_level.time;
    }
  } else {
    G_HookCheckFire(cl, false);
  }
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
  if (!q_strcmp(g_hook_style->string, "default")) {
    hook_style = Hook_StyleByName(InfoString_Get(cl->persistent.user_info, "hook_style"));
  } else {
    hook_style = Hook_StyleByName(g_hook_style->string);
  }

  cl->persistent.hook_style = hook_style;
}

/**
 * @brief Checks and sets up the hook state.
 * @details "default" means enabled: a module that compiled the hook in is a
 * module that wants it, and one that does not can set the cvar.
 */
void G_Hook_CheckState(void) {

  if (q_strcmp(g_hook->string, "default")) { // the cvar, else compiled in means on
    g_hook_enabled = !!g_hook->integer;
  } else {
    g_hook_enabled = true;
  }

  if (g_hook_distance->modified) {
    g_hook_distance->value = Clampf(g_hook_distance->value, PM_HOOK_MIN_DIST, PM_HOOK_MAX_DIST);
    g_hook_distance->modified = false;
  }
}
