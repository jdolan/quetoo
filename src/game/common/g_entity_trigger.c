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
#include "bg_portal.h"

#define TRIGGERED 0x1
#define SHOOTABLE 0x2

/**
 * @brief Initializes a trigger entity, setting its move direction, solid type, and model.
 */
static void G_Trigger_Init(g_entity_t *ent) {

  if (!Vec3_Equal(ent->s.angles, Vec3_Zero())) {
    G_SetMoveDir(ent);
  }

  ent->solid = SOLID_TRIGGER;
  ent->move_type = MOVE_TYPE_NONE;
  gi.SetModel(ent, ent->model);
  ent->sv_flags = SVF_NO_CLIENT;
}

/**
 * @brief The wait time has passed, so set back up for another activation
 */
static void G_trigger_multiple_Wait(g_entity_t *ent) {
  ent->next_think = 0;
}

/**
 * @brief Called after the wait period expires, re-enabling the trigger for another activation.
 */
static void G_trigger_multiple_Think(g_entity_t *ent) {

  if (ent->next_think) {
    return; // already been triggered
  }

  G_UseTargets(ent, ent->activator);

  if (ent->wait < 0) { // a trigger_once, which fires the once and is gone
    ent->Touch = NULL;
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
    ent->Think = G_FreeEntity;
  } else { // otherwise re-arm, at zero meaning as often as we are touched
    ent->Think = G_trigger_multiple_Wait;
    ent->next_think = g_level.time + (uint32_t) Maxi((int32_t) SECONDS_TO_MILLIS(ent->wait), QUETOO_TICK_MILLIS);
  }
}

/**
 * @brief Fires the trigger's targets after the optional delay, then resets or removes the trigger.
 */
static void G_trigger_multiple_Use(g_entity_t *ent, g_entity_t *other,
                                   g_entity_t *activator) {

  ent->activator = activator;

  G_trigger_multiple_Think(ent);
}

/**
 * @brief Handles use activation of a `trigger_multiple`, delegating to the think function.
 */
static void G_trigger_multiple_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->client) {
    const bool isProjectile = other->owner && other->owner->client;
    if (isProjectile && (ent->spawn_flags & SHOOTABLE)) {
      // we're a shootable trigger, and we've been shot
    } else {
      return;
    }
  }

  if (!Vec3_Equal(ent->move_dir, Vec3_Zero())) {
    vec3_t forward;

    Vec3_Vectors(other->s.angles, &forward, NULL, NULL);

    if (Vec3_Dot(forward, ent->move_dir) < 0.0) {
      return;
    }
  }

  ent->activator = other;
  G_trigger_multiple_Think(ent);
}

/**
 * @brief Handles touch events on a `trigger_multiple`, activating it when a qualifying entity enters.
 */
static void G_trigger_multiple_Enable(g_entity_t *ent, g_entity_t *other,
                                      g_entity_t *activator) {
  ent->solid = SOLID_TRIGGER;
  ent->Use = G_trigger_multiple_Use;
  gi.LinkEntity(ent);
}

/*QUAKED trigger_multiple (.5 .5 .5) ? triggered shootable
 Triggers multiple targets at fixed intervals.

 -------- Keys --------
 delay : Delay in seconds between activation and firing of targets (default 0).
 wait : Interval in seconds between activations (default 0, activating for as long as it is
 touched). Give it a value to debounce, e.g. "wait" "3".
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 triggered : If set, this trigger must be targeted before it will activate.
 shootable : If set, this trigger will fire when projectiles touch it.
 */
void G_trigger_multiple(g_entity_t *ent) {

  ent->sound = gi.SoundIndex("misc/chat");

  ent->Touch = G_trigger_multiple_Touch;
  ent->move_type = MOVE_TYPE_NONE;
  ent->sv_flags |= SVF_NO_CLIENT;

  if (ent->spawn_flags & TRIGGERED) {
    ent->solid = SOLID_NOT;
    ent->Use = G_trigger_multiple_Enable;
  } else {
    ent->solid = SOLID_TRIGGER;
    ent->Use = G_trigger_multiple_Use;
  }

  if (!Vec3_Equal(ent->s.angles, Vec3_Zero())) {
    G_SetMoveDir(ent);
  }

  gi.SetModel(ent, ent->model);
  gi.LinkEntity(ent);
}

/*QUAKED trigger_once (.5 .5 .5) ? triggered
 Triggers multiple targets once.

 -------- Keys --------
 delay : Delay in seconds between activation and firing of targets (default 0).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 triggered : If set, this trigger must be targeted before it will activate.
 */
void G_trigger_once(g_entity_t *ent) {
  ent->wait = -1;
  G_trigger_multiple(ent);
}

/**
 * @brief Enables a previously dormant triggered trigger, making it solid and ready to activate.
 */
static void G_trigger_relay_Use(g_entity_t *ent, g_entity_t *other,
                                g_entity_t *activator) {
  G_UseTargets(ent, activator);
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
 A trigger that can not be touched, but must be triggered by another entity.

 -------- Keys --------
 delay : The delay in seconds between activation and firing of targets (default 0).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity.
 */
void G_trigger_relay(g_entity_t *ent) {
  ent->Use = G_trigger_relay_Use;
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
 Triggers targets once at level spawn.

 -------- Keys --------
 delay : The delay in seconds between activation and firing of targets (default 0.2).
 message : An optional message to display when this trigger fires.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 */
void G_trigger_always(g_entity_t *ent) {

  // we must have some delay to make sure our use targets are present
  if (ent->delay < 0.2) {
    ent->delay = 0.2;
  }

  G_UseTargets(ent, ent);
}

#define PUSH_ONCE 1
#define PUSH_EFFECT 2
#define PUSH_START_OFF 4
#define PUSH_TOGGLE 8

/**
 * @brief Handles touch events on a `trigger_push`, applying velocity to the touching entity.
 */
static void G_trigger_push_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (other->move_type == MOVE_TYPE_WALK || other->move_type == MOVE_TYPE_BOUNCE) {

    other->velocity = Vec3_Scale(ent->move_dir, ent->speed * 10.0);

    if (other->client) {
      other->client->ps.pm_state.flags |= PMF_TIME_PUSHED;
      other->client->ps.pm_state.time = 240;
    }

    if (other->push_time < g_level.time) {
      other->push_time = g_level.time + 1500;
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .origin = &other->s.origin,
      }, MULTICAST_PHS);
    }
  }

  if (ent->spawn_flags & PUSH_ONCE) {
    G_FreeEntity(ent);
  }
}

/**
 * @brief Handles use activation of a `trigger_push`, toggling its solidity on or off.
 */
static void G_trigger_push_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->solid == SOLID_NOT) {
    ent->solid = SOLID_TRIGGER;
  } else {
    ent->solid = SOLID_NOT;
  }

  gi.LinkEntity(ent);

  G_Debug("%s is now %s\n", etos(ent), ent->solid == SOLID_NOT ? "off" : "on");

  if (!(ent->spawn_flags & PUSH_TOGGLE)) {
    ent->Use = NULL;
  }
}

/**
 * @brief Creates an effect trail for the specified entity.
 */
static void G_trigger_push_Effect(g_entity_t *ent) {

  g_entity_t *effect = G_AllocEntity(__func__);

  effect->s.origin = Box3_Center(ent->bounds);

  effect->move_type = MOVE_TYPE_NONE;
  effect->s.trail = TRAIL_TELEPORTER;

  gi.LinkEntity(effect);
}

/*QUAKED trigger_push (.5 .5 .5) ? push_once push_effects start_off toggle
 Pushes the player in any direction. These are commonly used to make jump pads to send the player upwards. Using the angles key, you can project the player in any direction using "pitch yaw roll."

 -------- Keys --------
 angles : The direction to push the player in "pitch yaw roll" notation (e.g. -80 270 0).
 sound : The sound effect to play when the player is pushed (default "trigger/push").
 speed : The speed with which to push the player (default 100).
 targetname : The target name of this entity, if it is to be triggered.

 -------- Spawn flags --------
 push_once : If set, the pusher is freed after it is used once.
 push_effects : If set, emit particle effects to indicate that a pusher is here.
 start_off : If set, this entity must be activated before it will push players.
 toggle : If set, this entity is toggled each time it is activated.
 */
void G_trigger_push(g_entity_t *ent) {

  G_Trigger_Init(ent);

  ent->Touch = G_trigger_push_Touch;

  const cm_entity_t *sound = gi.EntityValue(ent->def, "sound");
  if (sound->parsed & ENTITY_STRING) {
    ent->move_info.sound_start = gi.SoundIndex(sound->string);
  } else {
    ent->move_info.sound_start = gi.SoundIndex("trigger/push");
  }

  if (!ent->speed) {
    ent->speed = 100;
  }

  if (ent->spawn_flags & (PUSH_START_OFF | PUSH_TOGGLE)) {
    if (ent->spawn_flags & PUSH_START_OFF) {
      ent->solid = SOLID_NOT;
    }
    ent->Use = G_trigger_push_Use;
  }

  gi.LinkEntity(ent);

  if (ent->spawn_flags & PUSH_EFFECT) {
    G_trigger_push_Effect(ent);
  }
}

/**
 * @brief The reference point for a `trigger_portal`'s position math: the centroid of the
 * common/portal face quemap baked in at compile time.
 */
static vec3_t G_trigger_portal_Origin(const g_entity_t *ent) {
  return gi.EntityValue(ent->def, "portal_origin")->vec3;
}

/**
 * @brief The facing of a `trigger_portal`: the direction of travel into its common/portal face,
 * as quemap baked it. Kept apart from the entity's own angles, which would rotate the brush.
 */
static vec3_t G_trigger_portal_Angles(const g_entity_t *ent) {
  return gi.EntityValue(ent->def, "portal_angles")->vec3;
}

static void G_trigger_portal_Transit(const g_entity_t *ent, const g_entity_t *dest, g_entity_t *other);

#define PORTAL_STRICT_DISPLACE 1

/**
 * @brief Handles touch events on a `trigger_portal`. Unlike `trigger_teleporter`, this fires on
 * every frame the toucher overlaps the volume (there is no single "moment" of transit), and
 * carries the toucher's full position, velocity and view through to the paired portal via
 * `G_trigger_portal_Carry`. Walking through one frame at a time this way, rather than snapping to
 * a fixed destination point, is what makes the crossing imperceptible: the mapper builds matching
 * geometry on both sides, and the player simply never stops moving.
 *
 * `PORTAL_STRICT_DISPLACE` skips all of that relative-basis math and simply slides the toucher by
 * the fixed offset between the two faces, leaving velocity, view and facing untouched - a literal
 * hole in space rather than a seam between two differently-oriented pieces of geometry. Reach for
 * it only when the map schema demands a straight positional displacement and the illusion of
 * continuous surfaces doesn't matter.
 */
static void G_trigger_portal_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

#if defined(G_HOOK)
  if (other->owner && other->owner->client &&
      other->owner->client->hook.entity == other) {
    G_HookDetach(other->owner->client);
    return;
  }
#endif

  if (!G_IsMeat(other) && other->solid != SOLID_PROJECTILE) {
    return;
  }

#if defined(G_HOOK)
  if (other->client && other->client->hook.entity) {
    G_HookDetach(other->client);
  }
#endif

  const g_entity_t *dest = G_Find(NULL, EOFS(target_name), ent->target);

  if (!dest) {
    G_Warn("Couldn't find destination\n");
    return;
  }

  vec3_t right_a, up_a, fwd_a;
  Bg_PortalBasis(G_trigger_portal_Angles(ent), false, &right_a, &up_a, &fwd_a);

  // transit the moment the toucher's center crosses the face plane, not when its bounds first
  // touch the volume: it then lands exactly as far past the paired face as it is past this one,
  // straddling it on the way out rather than stranded in the recess behind it. A fresh arrival
  // is in front of the paired face, so it can never be bounced straight back
  if (Vec3_Dot(Vec3_Subtract(other->s.origin, G_trigger_portal_Origin(ent)), fwd_a) < 0.f) {
    return;
  }

  G_trigger_portal_Transit(ent, dest, other);
}

/**
 * @brief Carries `other` through the portal `ent` to its paired portal `dest`: its position
 * relative to the face it is at, its velocity, and its view or facing, all re-expressed relative
 * to the paired face.
 */
static void G_trigger_portal_Transit(const g_entity_t *ent, const g_entity_t *dest, g_entity_t *other) {

  vec3_t right_a, up_a, fwd_a;
  Bg_PortalBasis(G_trigger_portal_Angles(ent), false, &right_a, &up_a, &fwd_a);

  // on arrival, a toucher faces and moves along the destination face's outward normal, the
  // reverse of the travel-into direction its angles bake (see Bg_PortalBasis)
  vec3_t right_b, up_b, fwd_b;
  Bg_PortalBasis(G_trigger_portal_Angles(dest), true, &right_b, &up_b, &fwd_b);

  // nudge the arrival point a little past the destination face, along its outward normal, so the
  // toucher isn't left sitting exactly on the boundary of the destination's own touch field -
  // without this, a pair facing the same way (notably a strict_displace splice) immediately
  // re-triggers a transit back out, since the toucher is still technically touching on arrival
  const vec3_t exit_nudge = Vec3_Scale(fwd_b, 1.f);

  if (ent->spawn_flags & PORTAL_STRICT_DISPLACE) {
    other->s.origin = Vec3_Add(Vec3_Add(other->s.origin,
        Vec3_Subtract(G_trigger_portal_Origin(dest), G_trigger_portal_Origin(ent))), exit_nudge);

    gi.LinkEntity(other);
    return;
  }

  // carry the toucher's full position within this portal's volume through to the paired
  // portal, expressed relative to each portal's own face, instead of snapping to a fixed point
  const vec3_t offset = Vec3_Subtract(other->s.origin, G_trigger_portal_Origin(ent));
  const vec3_t carried_offset = Bg_PortalCarry(offset, right_a, up_a, fwd_a, right_b, up_b, fwd_b);

  other->s.origin = Vec3_Add(Vec3_Add(G_trigger_portal_Origin(dest), carried_offset), exit_nudge);
  other->velocity = Bg_PortalCarry(other->velocity, right_a, up_a, fwd_a, right_b, up_b, fwd_b);

  if (other->client) {
    vec3_t view_forward;
    Vec3_Vectors(other->client->ps.pm_state.view_angles, &view_forward, NULL, NULL);

    const vec3_t view_angles = Vec3_Euler(
        Bg_PortalCarry(view_forward, right_a, up_a, fwd_a, right_b, up_b, fwd_b));

    other->client->ps.pm_state.view_angles = view_angles;
    other->client->ps.pm_state.delta_angles = Vec3_Zero();
    other->client->angles = view_angles;

    Vec3_Vectors(other->client->angles, &other->client->forward, &other->client->right, &other->client->up);

    gi.WriteByte(SV_CMD_SNAP_ANGLES);
    gi.WriteAngles(view_angles);
    gi.Unicast(other->client, true);
  } else {
    vec3_t entity_forward;
    Vec3_Vectors(other->s.angles, &entity_forward, NULL, NULL);

    other->s.angles = Vec3_Euler(
        Bg_PortalCarry(entity_forward, right_a, up_a, fwd_a, right_b, up_b, fwd_b));
  }

  gi.LinkEntity(other);
}

#define MAX_PORTAL_HOPS 4

/**
 * @brief Traces from `start` toward `end`, carrying the trace through any `trigger_portal` face
 * it crosses inward on the way, exactly as a toucher would be carried. Hitscan weapons use this,
 * so that whatever can be seen through a portal can be shot through it too.
 * @param start The start of the trace; on return, the start of its final leg.
 * @param end The end of the trace; on return, the end of its final leg.
 * @param hops If given, receives the number of portals crossed.
 * @param segment If given, called for each leg that ends at a portal face, with that face's
 * outward normal, for tracers and the like. The final leg is the caller's own to draw.
 * @return The trace of the final leg.
 */
cm_trace_t G_TracePortals(vec3_t *start, vec3_t *end, const box3_t bounds, const g_entity_t *skip,
                          int32_t contents, int32_t *hops, G_TraceSegmentFunc segment, void *data);

/**
 * @return The nearest `trigger_portal` whose face the segment from `start` along `dir` crosses
 * inward within `*length`, or NULL. `*length` and `normal` receive the distance to the crossing
 * and the face's outward normal.
 */
static const g_entity_t *G_trigger_portal_Crossing(const vec3_t start, const vec3_t dir, float *length, vec3_t *normal) {

  const g_entity_t *portal = NULL;

  for (g_entity_t *p = G_Find(NULL, EOFS(classname), "trigger_portal"); p; p = G_Find(p, EOFS(classname), "trigger_portal")) {

    if (!p->in_use || !p->target) {
      continue;
    }

    vec3_t right, up, forward;
    Bg_PortalBasis(G_trigger_portal_Angles(p), false, &right, &up, &forward);

    const float into = Vec3_Dot(dir, forward);
    if (into <= 0.f) {
      continue;
    }

    const float t = Vec3_Dot(Vec3_Subtract(G_trigger_portal_Origin(p), start), forward) / into;
    if (t <= 0.f || t >= *length) {
      continue;
    }

    if (!Box3_ContainsPoint(Box3_Expand(p->abs_bounds, 1.f), Vec3_Fmaf(start, t, dir))) {
      continue;
    }

    portal = p;
    *normal = Vec3_Negate(forward);
    *length = t;
  }

  return portal;
}

/**
 * @brief Carries `ent` through the first `trigger_portal` face its center crosses inward while
 * moving from `start` to `end`, if any, so that something crossing a portal within a single tick
 * transits at the face rather than flying over the volume behind it into the wall.
 * @param fraction The fraction of the move that was clear of anything solid.
 * @return The fraction of the move at which the crossing occurred, or -1 for none.
 */
float G_TransitPortals(g_entity_t *ent, const vec3_t start, const vec3_t end, float fraction) {

  if (!G_IsMeat(ent) && ent->solid != SOLID_PROJECTILE) {
    return -1.f;
  }

#if defined(G_HOOK)
  if (ent->owner && ent->owner->client && ent->owner->client->hook.entity == ent) {
    return -1.f;
  }
#endif

  const vec3_t delta = Vec3_Subtract(end, start);
  const float length = Vec3_Length(delta);
  if (length == 0.f) {
    return -1.f;
  }

  const vec3_t dir = Vec3_Scale(delta, 1.f / length);

  vec3_t normal;
  float distance = fraction * length;

  const g_entity_t *portal = G_trigger_portal_Crossing(start, dir, &distance, &normal);
  if (!portal) {
    return -1.f;
  }

  const g_entity_t *dest = G_Find(NULL, EOFS(target_name), portal->target);
  if (!dest) {
    return -1.f;
  }

  ent->s.origin = Vec3_Fmaf(start, distance, dir);

  G_trigger_portal_Transit(portal, dest, ent);

  return distance / length;
}

cm_trace_t G_TracePortals(vec3_t *start, vec3_t *end, const box3_t bounds, const g_entity_t *skip,
                          int32_t contents, int32_t *hops, G_TraceSegmentFunc segment, void *data) {

  cm_trace_t tr = gi.Trace(*start, *end, bounds, skip, contents);

  if (hops) {
    *hops = 0;
  }

  for (int32_t hop = 0; hop < MAX_PORTAL_HOPS; hop++) {

    const vec3_t delta = Vec3_Subtract(*end, *start);
    const float length = Vec3_Length(delta);
    if (length == 0.f) {
      break;
    }

    const vec3_t d = Vec3_Scale(delta, 1.f / length);

    // the nearest portal face crossed inward before the trace ended, if any
    vec3_t normal;
    float best = tr.fraction * length;

    const g_entity_t *portal = G_trigger_portal_Crossing(*start, d, &best, &normal);
    if (!portal) {
      break;
    }

    const g_entity_t *dest = G_Find(NULL, EOFS(target_name), portal->target);
    if (!dest) {
      break;
    }

    const vec3_t point = Vec3_Fmaf(*start, best, d);

    if (segment) {
      segment(*start, point, normal, data);
    }

    vec3_t right_a, up_a, fwd_a, right_b, up_b, fwd_b;
    Bg_PortalBasis(G_trigger_portal_Angles(portal), false, &right_a, &up_a, &fwd_a);
    Bg_PortalBasis(G_trigger_portal_Angles(dest), true, &right_b, &up_b, &fwd_b);

    vec3_t carried = d;

    if (portal->spawn_flags & PORTAL_STRICT_DISPLACE) {
      *start = Vec3_Add(point, Vec3_Subtract(G_trigger_portal_Origin(dest), G_trigger_portal_Origin(portal)));
    } else {
      const vec3_t offset = Vec3_Subtract(point, G_trigger_portal_Origin(portal));
      *start = Vec3_Add(G_trigger_portal_Origin(dest), Bg_PortalCarry(offset, right_a, up_a, fwd_a, right_b, up_b, fwd_b));
      carried = Bg_PortalCarry(d, right_a, up_a, fwd_a, right_b, up_b, fwd_b);
    }

    // the same nudge past the far face a toucher gets, so the next leg starts in front of it
    *start = Vec3_Fmaf(*start, 1.f, fwd_b);
    *end = Vec3_Fmaf(*start, length - best, carried);

    if (hops) {
      (*hops)++;
    }

    tr = gi.Trace(*start, *end, bounds, skip, contents);
  }

  return tr;
}

/*QUAKED trigger_portal (.5 .5 .5) ? strict_displace
Continuously carries anything that walks through this volume to the paired trigger_portal, with
no teleport sound, effects, or angle/velocity snap of any kind. Requires one face of the brush
textured common/portal: the compiler bakes that face's own centroid and outward normal in as
this entity's position and facing (there is no angle key to set by hand - the two must never be
allowed to drift out of sync, so the face is authoritative). That face's outward normal is the
single source of truth for this specific entity, used two ways at once: crossing its plane inward
is what transits you (your center, not your bounds, so you always land in front of the far face),
and if something else targets this entity, it's the direction you'll be facing on arrival. Tag each
portal in a pair according to its own true orientation in the world - do not simply mirror
however the other one in the pair happens to be tagged, since the two may not (and often should
not) face the same way. A trigger_portal with no target of its own is inert (it generates no
touch field at all) but can still be pointed at by another's target, making a one-way portal:
give both entities a targetname and only the outgoing side a target to prevent transit back.

-------- Keys --------
target : The paired trigger_portal's targetname. If unset, this entity is a one-way destination
         only: it never transits anything itself.
targetname : This portal's own name, for the paired portal to target.

-------- Spawnflags --------
strict_displace : Skip the relative-facing math entirely and just slide the toucher by the fixed
                  offset between the two faces - velocity, view and facing pass through unchanged.
                  Use this only for a literal hole in space (e.g. two identically-oriented copies
                  of the same geometry spliced together); for a normal portal where the two faces
                  may point different ways, leave this off.
*/
void G_trigger_portal(g_entity_t *ent) {

  if (!ent->model) {
    G_Debug("trigger_portal requires brushwork\n");
    G_FreeEntity(ent);
    return;
  }

  if (!(gi.EntityValue(ent->def, "portal_origin")->parsed & ENTITY_VEC3) ||
      !(gi.EntityValue(ent->def, "portal_angles")->parsed & ENTITY_VEC3)) {
    G_Debug("trigger_portal requires a common/portal face\n");
    G_FreeEntity(ent);
    return;
  }

  ent->solid = SOLID_TRIGGER;
  ent->move_type = MOVE_TYPE_NONE;

  gi.SetModel(ent, ent->model);

  // no target means this is a one-way destination only; leave it untouchable
  if (ent->target) {
    ent->Touch = G_trigger_portal_Touch;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Handles use activation of a `trigger_hurt`, toggling its solidity on or off.
 */
static void G_trigger_hurt_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->solid == SOLID_NOT) {
    ent->solid = SOLID_TRIGGER;
  } else {
    ent->solid = SOLID_NOT;
  }

  gi.LinkEntity(ent);

  if (!(ent->spawn_flags & 2)) {
    ent->Use = NULL;
  }
}

/**
 * @brief Handles touch events on a `trigger_hurt`, dealing damage to entities that enter it.
 */
static void G_trigger_hurt_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->take_damage) { // deal with items that land on us

    if (other->item) {
      G_ResetDroppedItem(other);
    }

    G_Debug("%s\n", etos(other));
    return;
  }

  if (other->dead) {
    return;
  }

  if (ent->timestamp > g_level.time) {
    return;
  }

  if (ent->spawn_flags & 16) {
    ent->timestamp = g_level.time + 1000;
  } else {
    ent->timestamp = g_level.time + 100;
  }

  const int16_t d = ent->damage;

  int32_t dflags = DMG_NO_ARMOR;

  if (ent->spawn_flags & 8) {
    dflags = DMG_NO_GOD;
  }

  G_Damage(&(g_damage_t) {
    .target = other,
    .inflictor = ent,
    .attacker = NULL,
    .dir = Vec3_Zero(),
    .point = other->s.origin,
    .normal = Vec3_Zero(),
    .damage = d,
    .knockback = d >> 2,
    .flags = dflags,
    .mod = MOD_TRIGGER_HURT
  });
}

/*QUAKED trigger_hurt (.5 .5 .5) ? start_off toggle ? no_protection slow
 Any player that touches this will be hurt by "dmg" points of damage every 100ms (very fast).

 -------- Keys --------
 dmg : The damage done every 100ms to any player who touches this entity (default 2).
 targetname : The target name of this entity, if it is to be triggered.

 -------- Spawn flags --------
 start_off : If set, this entity must be activated before it will hurt players.
 toggle : If set, this entity is toggled each time it is activated.
 ?
 no_protection : If set, armor will not be used to absorb damage inflicted by this entity.
 slow : Decreases the damage rate to once per second.
 */
void G_trigger_hurt(g_entity_t *ent) {

  G_Trigger_Init(ent);

  ent->Touch = G_trigger_hurt_Touch;

  if (!ent->damage) {
    ent->damage = 2;
  }

  if (ent->spawn_flags & 1) {
    ent->solid = SOLID_NOT;
  } else {
    ent->solid = SOLID_TRIGGER;
  }

  if (ent->spawn_flags & 2) {
    ent->Use = G_trigger_hurt_Use;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Handles touch events on a `trigger_exec`, executing a console command or script.
 */
static void G_trigger_exec_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (ent->timestamp > g_level.time) {
    return;
  }

  ent->timestamp = g_level.time + ent->delay * 1000;

  const char *command = gi.EntityValue(ent->def, "command")->nullable_string;
  if (command) {
    gi.Cbuf(va("%s\n", command));
  }

  else {
    const char *script = gi.EntityValue(ent->def, "script")->nullable_string;
    if (script) {
      gi.Cbuf(va("exec %s\n", script));
    }
  }
}

/*QUAKED trigger_exec (0 1 0) ?
 Executes a console command or script file when activated.

 -------- Keys --------
 command : The console command(s) to execute.
 script : The script file (.cfg) to execute.
 delay : The delay in seconds between activation and execution of the commands.
 */
void G_trigger_exec(g_entity_t *ent) {

  const char *command = gi.EntityValue(ent->def, "command")->nullable_string;
  const char *script = gi.EntityValue(ent->def, "script")->nullable_string;
  if (!command && !script) {
    G_Debug("No command or script at %s", vtos(ent->s.origin));
    G_FreeEntity(ent);
    return;
  }

  G_Trigger_Init(ent);

  ent->Touch = G_trigger_exec_Touch;

  gi.LinkEntity(ent);
}
