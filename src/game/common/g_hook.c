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
 * module adopting it needs only `GameClientHook`, a `GameHookStyle` in its
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

Cvar *g_hook;
Cvar *g_hookAutoRefire;
Cvar *g_hookDistance;
Cvar *g_hookPullSpeed;
Cvar *g_hookRefire;
Cvar *g_hookSky;
Cvar *g_hookSpeed;
Cvar *g_hookStyle;

static struct {
  uint16_t model;
  uint16_t fire;
  uint16_t fly;
  uint16_t hit;
  uint16_t pull;
  uint16_t detach;
  uint16_t gibhit;
} module;

/**
 * @brief True when the hook is available this level.
 */
static bool gHookEnabled;

/**
 * @return True if the hook is enabled for this level.
 */
static bool G_Hook_Enabled(void) {
  return gHookEnabled;
}

/**
 * @brief Takes the client's movement over while they are pulling on the hook,
 * and otherwise defers to previous.
 */
static void G_PrepareMove_Hook(GameClient *cl, PMove *pm) {

  if (!cl->hook.pull) {
    previous.PrepareMove(cl, pm);
    return;
  }

  switch (cl->persistent.hookStyle) {
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

  pm->hookPullSpeed = g_hookPullSpeed->value;
}

/**
 * @brief Indexes the hook's model and sounds for this level.
 */
static void G_InitMedia_Hook(void) {

  previous.InitMedia();

  module.model = gi.ModelIndex("models/grapplehook/tris");

  module.fire = gi.SoundIndex("grapplehook/fire");
  module.fly = gi.SoundIndex("grapplehook/fly");
  module.hit = gi.SoundIndex("grapplehook/hit");
  module.pull = gi.SoundIndex("grapplehook/pull");
  module.detach = gi.SoundIndex("grapplehook/detach");
  module.gibhit = gi.SoundIndex("grapplehook/gibhit");
}

/**
 * @brief Publishes the pull speed the client predicts with. The cvar's value,
 * not its string, is what the game moves players by, so that is what goes on
 * the wire; a string that does not parse as a positive speed is reset first.
 */
static void G_Hook_PublishPullSpeed(void) {

  if (!isfinite(g_hookPullSpeed->value) || g_hookPullSpeed->value <= 0.f) {
    G_Warn("Invalid g_hookPullSpeed \"%s\", resetting to %g\n", g_hookPullSpeed->string, PM_SPEED_HOOK_PULL);
    gi.SetCvarValue("g_hookPullSpeed", PM_SPEED_HOOK_PULL);
    g_hookPullSpeed->modified = false;
  }

  gi.SetConfigString(CS_HOOK_PULL_SPEED, va("%.9g", g_hookPullSpeed->value));
}

/**
 * @brief Resolves whether the hook is available this level, and publishes the
 * pull speed the client predicts with.
 */
static void G_ConfigureLevel_Hook(void) {

  G_Hook_CheckState();

  G_Hook_PublishPullSpeed();

  previous.ConfigureLevel();
}

/**
 * @brief Applies the hook's own cvars.
 */
static bool G_CheckCvars_Hook(void) {
  bool restart = false;

  if (g_hook->modified) {
    g_hook->modified = false;

    G_Hook_CheckState();

    gi.BroadcastPrint(PRINT_HIGH, "Hook has been %s\n", G_Hook_Enabled() ? "enabled" : "disabled");

    restart = true;
  }

  if (g_hookSpeed->modified) {
    g_hookSpeed->modified = false;

    gi.BroadcastPrint(PRINT_HIGH, "Hook speed has been changed to %g\n", g_hookSpeed->value);
  }

  if (g_hookPullSpeed->modified) {
    g_hookPullSpeed->modified = false;

    G_Hook_PublishPullSpeed();

    gi.BroadcastPrint(PRINT_HIGH, "Hook pull speed has been changed to %g\n", g_hookPullSpeed->value);
  }

  if (g_hookStyle->modified) {
    g_hookStyle->modified = false;

    // reset all the hook styles on the players
    G_ForEachClient(cl, {
      G_SetClientHookStyle(cl);
    });

    gi.BroadcastPrint(PRINT_HIGH, "Hook style has been changed to %s\n", g_hookStyle->string);
  }

  return previous.CheckCvars() || restart;
}

/**
 * @brief Tosses the grapple a client leaving play is holding.
 */
static void G_TossInventory_Hook(GameClient *cl) {

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
  g_hookStyle = gi.AddCvar("g_hookStyle", "default", 0, "Whether to allow only \"pull\", \"swing_manual\", \"swing_auto\" or any (\"default\") hook swing style.");
  g_hookAutoRefire = gi.AddCvar("g_hookAutoRefire", "0", 0, "If the hook automatically refires when it hits a non-solid surface, like players or weapon clips. (Currently non-functional)");
  g_hookDistance = gi.AddCvar("g_hookDistance", va("%.1f", PM_HOOK_DEF_DIST), 0, "The maximum distance the hook will travel.");
  g_hookPullSpeed = gi.AddCvar("g_hookPullSpeed", va("%g", PM_SPEED_HOOK_PULL), 0, "The speed that you get pulled towards the hook.");
  g_hookRefire = gi.AddCvar("g_hookRefire", "0.25", 0, "The refire delay on the grapple hook in seconds.");
  g_hookSky = gi.AddCvar("g_hookSky", "0", CVAR_SERVER_INFO, "If enabled, the grapple hook attaches to sky surfaces rather than detaching.");
  g_hookSpeed = gi.AddCvar("g_hookSpeed", "1200", 0, "The speed that the hook will fly at.");

  g_hookPullSpeed->modified =
      g_hookSpeed->modified =
      g_hookStyle->modified =
      g_hook->modified = false;
}

/**
 * @brief Touch callback for the hook projectile; attaches to structural surfaces or deals damage and detaches on hitting enemies.
 */
static void G_HookProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

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

  if (!sky || g_hookSky->integer) {

    if (G_IsStructural(trace) || (sky && g_hookSky->integer) || (G_IsMeat(other) && G_OnSameTeam(other->client, ent->owner->client))) {

      ent->velocity = Vec3_Zero();
      ent->avelocity = Vec3_Zero();

      ent->owner->client->hook.pull = true;

      ent->moveType = MOVE_TYPE_THINK;
      ent->solid = SOLID_NOT;
      ent->bounds = Box3_Zero();
      ent->enemy = other;

      gi.LinkEntity(ent);

      ent->owner->client->ps.pmState.hookPosition = ent->s.origin;

      if (ent->owner->client->persistent.hookStyle != HOOK_PULL) {
        const float distance = Vec3_Distance(ent->owner->s.origin, ent->s.origin);

        ent->owner->client->ps.pmState.hookLength = Clampf(distance, PM_HOOK_MIN_DIST, g_hookDistance->value);
      }

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_HOOK_IMPACT);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    } else {

      G_MulticastSound(&(const GamePlaySound) {
        .index = module.gibhit,
        .entity = ent,
        .pitch = RandomRangei(-4, 5)
      }, MULTICAST_PHS);

      /*
      if (g_hookAutoRefire->integer) {
        G_HookThink(ent->owner, true);
      } else {*/
        ent->velocity = Vec3_Normalize(ent->velocity);

        G_Damage(&(GameDamage) {
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
    if (g_hookAutoRefire->integer) {
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
static void G_HookTrail_Think(GameEntity *ent) {

  const GameEntity *hook = ent->targetEnt;
  GameClient *cl = ent->owner->client;

  Vec3 forward, right, up, org;

  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.0);

  ent->s.origin = org;
  ent->s.termination = hook->s.origin;

  Vec3 distance;
  distance = Vec3_Subtract(org, hook->s.origin);

  if (Vec3_Length(distance) > g_hookDistance->value) {

    G_HookDetach(cl);
    return;
  }

  ent->nextThink = gLevel.time + 1;
  gi.LinkEntity(ent);
}

/**
 * @brief Think callback for the hook projectile; tracks attached movers and updates the hook position each tick.
 */
static void G_HookProjectile_Think(GameEntity *ent) {

  // if we're attached to something, copy velocities
  if (ent->enemy) {
    GameEntity *mover = ent->enemy;
    Vec3 move, amove, inverseAmove, forward, right, up, rotate, translate, delta;

    move = Vec3_Scale(mover->velocity, QUETOO_TICK_SECONDS);
    amove = Vec3_Scale(mover->avelocity, QUETOO_TICK_SECONDS);

    if (!Vec3_Equal(move, Vec3_Zero()) || !Vec3_Equal(amove, Vec3_Zero())) {
      inverseAmove = Vec3_Negate(amove);
      Vec3_Vectors(inverseAmove, &forward, &right, &up);

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
      ent->targetEnt->s.angles.y += amove.y;

      gi.LinkEntity(ent);

      ent->owner->client->ps.pmState.hookPosition = ent->s.origin;
    }

    if ((ent->owner->client->persistent.hookStyle == HOOK_PULL && Vec3_LengthSquared(ent->owner->velocity) > 128.0) ||
      ent->knockback != ent->owner->client->ps.pmState.hookLength) {
      ent->s.sound = module.pull;
      ent->knockback = ent->owner->client->ps.pmState.hookLength;
    } else {
      ent->s.sound = 0;
    }
  }

  ent->nextThink = gLevel.time + 1;
}

/**
 * @brief Fires a grappling hook projectile from the specified entity in the given direction.
 */
GameEntity *G_HookProjectile(GameEntity *ent, const Vec3 start, const Vec3 dir) {
  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, g_hookSpeed->value);
  projectile->avelocity = MakeVec3(0, 0, 500);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->Touch = G_HookProjectile_Touch;
  projectile->s.model1 = module.model;
  projectile->Think = G_HookProjectile_Think;
  projectile->nextThink = gLevel.time + 1;
  projectile->s.sound = module.fly;

  gi.LinkEntity(projectile);

  GameEntity *trail = G_AllocEntity(__func__);

  projectile->targetEnt = trail;
  trail->targetEnt = projectile;

  trail->owner = ent;
  trail->solid = SOLID_NOT;
  trail->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  trail->moveType = MOVE_TYPE_THINK;
  trail->s.client = ent->s.client;
  trail->s.effects = EF_BEAM;
  trail->s.trail = TRAIL_HOOK;
  trail->Think = G_HookTrail_Think;
  trail->nextThink = gLevel.time + 1;

  G_HookTrail_Think(trail);

  // angle is used for rendering on client side
  trail->s.angles = projectile->s.angles;

  gi.LinkEntity(trail);
  return projectile;
}

/**
 * @brief Detach the player's hook if it's still attached.
 */
void G_HookDetach(GameClient *cl) {

  if (!gHookEnabled) {
    return;
  }

  if (!cl->hook.entity) {
    return;
  }

  // free entity
  if (cl->hook.entity->targetEnt) {
    G_FreeEntity(cl->hook.entity->targetEnt);
  }
  G_FreeEntity(cl->hook.entity);

  cl->hook.entity = NULL;

  // prevent hook spam
  if (!cl->hook.pull) {
    cl->hook.fireTime = gLevel.time + SECONDS_TO_MILLIS(g_hookRefire->value);
  } else {
    // don't get hurt from sweet-ass hooking
    cl->landTime = gLevel.time;
  }

  cl->hook.pull = false;

  G_MulticastSound(&(const GamePlaySound) {
    .index = module.detach,
    .entity = cl->entity,
    .pitch = RandomRangei(-4, 5)
  }, MULTICAST_PHS);

  // see if we can backflip for style points
  if (cl->entity->inUse && cl->entity->health > 0) {

    const Vec3 velocity = MakeVec3(cl->entity->velocity.x, cl->entity->velocity.y, 0.0);
    const float fwdSpeed = Vec3_Length(velocity) / 1.75;

    if (cl->entity->velocity.z > 50 && cl->entity->velocity.z > fwdSpeed) {
      G_SetAnimation(cl, ANIM_LEGS_JUMP2, true);
    }
  }
}

/**
 * @brief Handles the firing of the hook.
 */
static void G_HookCheckFire(GameClient *cl, const bool refire) {

  // hook can fire, see if we should
  if (!refire && !(cl->latchedButtons & BUTTON_HOOK)) {
    return;
  }

  if (!refire) {

    // use small epsilon for low server frame rates
    if (cl->hook.fireTime > gLevel.time + 1) {
      return;
    }

    cl->latchedButtons &= ~BUTTON_HOOK;
  } else {

    G_HookDetach(cl);
  }

  // fire away!
  Vec3 forward, right, up, org;
  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.0);

  cl->hook.pull = false;
  cl->hook.entity = G_HookProjectile(cl->entity, org, forward);

  G_MulticastSound(&(const GamePlaySound) {
    .index = module.fire,
    .entity = cl->entity,
    .pitch = RandomRangei(-4, 5)
  }, MULTICAST_PHS);

  cl->hook.thinkTime = gLevel.time;
}

/**
 * @brief The tail of the `G_AllowHook` chain: every client may hook, unless the
 * movement they are running has no hook to swing on. Declining here is how the
 * feature stays out of a ruleset that cannot carry it, rather than the ruleset
 * having to know the feature exists.
 */
static bool G_AllowHook_Common(const GameClient *cl) {
  return Pm_Movement(gLevel.movement)->hook;
}

AllowHook G_AllowHook = G_AllowHook_Common;

/**
 * @brief Handles management of the hook for a given player.
 */
void G_HookThink(GameClient *cl, const bool refire) {

  // sanity checks
  if (!gHookEnabled) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  if (G_Ai_InDeveloperMode() || !G_AllowHook(cl)) {
    if (cl->hook.entity) {
      G_HookDetach(cl);
    }
    cl->latchedButtons &= ~BUTTON_HOOK;
    return;
  }

  // send off to the proper sub-function

  if (refire) {
    G_HookCheckFire(cl, true);
    return;
  }

  if (cl->hook.entity) {

    const bool isManualHookSwing = cl->persistent.hookStyle == HOOK_SWING_MANUAL;
    const bool isHoldingHook = (cl->buttons & BUTTON_HOOK);
    const bool isPressingHook = (cl->latchedButtons & BUTTON_HOOK);

    if ((!isManualHookSwing && !isHoldingHook) || (isManualHookSwing && isPressingHook)) {

      G_HookDetach(cl);

      cl->latchedButtons &= ~BUTTON_HOOK;
      cl->hook.thinkTime = gLevel.time;
    }
  } else {
    G_HookCheckFire(cl, false);
  }
}

/**
 * @brief Set the hook style of the player, respecting server properties.
 */
void G_SetClientHookStyle(GameClient *cl) {

  if (!cl->inUse) {
    return;
  }

  GameHookStyle hookStyle;

  // respect userInfo on default
  if (!q_strcmp(g_hookStyle->string, "default")) {
    char style[MAX_INFO_STRING_VALUE];
    InfoString_Get(cl->persistent.userInfo, "hookStyle", style, sizeof(style));

    hookStyle = Hook_StyleByName(style);
  } else {
    hookStyle = Hook_StyleByName(g_hookStyle->string);
  }

  cl->persistent.hookStyle = hookStyle;
}

/**
 * @brief Checks and sets up the hook state.
 * @details "default" means enabled: a module that compiled the hook in is a
 * module that wants it, and one that does not can set the cvar.
 */
void G_Hook_CheckState(void) {

  if (q_strcmp(g_hook->string, "default")) { // the cvar, else compiled in means on
    gHookEnabled = !!g_hook->integer;
  } else {
    gHookEnabled = true;
  }

  if (g_hookDistance->modified) {
    g_hookDistance->value = Clampf(g_hookDistance->value, PM_HOOK_MIN_DIST, PM_HOOK_MAX_DIST);
    g_hookDistance->modified = false;
  }
}
