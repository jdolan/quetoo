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

#include "g_race.h"

/**
 * @file
 * @brief The triggers that describe a course. Each records itself with the
 * course as it spawns, and hands the client to g_race.c when touched.
 *
 * All of them are brush triggers, invisible, and fire their `target` and
 * `message` like a `trigger_multiple` does when the touch counted. `wait`
 * debounces a client standing in one, and defaults to half a second.
 */

// what a trigger stood in waits before counting the same client again
#define RACE_TRIGGER_WAIT .5f

/**
 * @brief The common half of every race trigger's setup.
 */
static void G_trigger_race_Init(g_entity_t *ent, void (*Touch)(g_entity_t *, g_entity_t *, const cm_trace_t *)) {

  if (ent->wait == 0.f) {
    ent->wait = RACE_TRIGGER_WAIT;
  }

  ent->solid = SOLID_TRIGGER;
  ent->move_type = MOVE_TYPE_NONE;
  ent->sv_flags |= SVF_NO_CLIENT;
  ent->Touch = Touch;

  gi.SetModel(ent, ent->model);
  gi.LinkEntity(ent);
}

/**
 * @brief Whether `other` is a client this trigger should hear from right now.
 */
static bool G_trigger_race_Accepts(g_entity_t *ent, g_entity_t *other) {

  if (!other->client) {
    return false;
  }

  return !G_Race_Debounced(other->client, ent, ent->wait);
}

static void G_trigger_race_start_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!G_trigger_race_Accepts(ent, other)) {
    return;
  }

  if (ent->count == RACE_START_TOUCH) {
    if (G_Race_Start(other->client)) {
      G_UseTargets(ent, other);
    }
  } else {
    G_Race_ArmStart(other->client, ent); // and g_race.c fires it on the way out
  }
}

/*QUAKED trigger_race_start (.5 .5 .5) ?
 Where a run begins. A course may have several, or none: with none, the run
 begins on the client's first movement.

 -------- Keys --------
 start_mode : When the run begins: touch (default) on entering, exit on leaving,
 or jump on the first jump from inside.
 wait : Seconds before the same client is heard from again (default 0.5).
 message : An optional string to display when the run begins.
 target : The name of the entity or team to use when the run begins.
 */
static void G_trigger_race_start(g_entity_t *ent) {

  const char *mode = gi.EntityValue(ent->def, "start_mode")->nullable_string;

  if (!mode || !*mode || !q_strcasecmp(mode, "touch")) {
    ent->count = RACE_START_TOUCH;
  } else if (!q_strcasecmp(mode, "exit")) {
    ent->count = RACE_START_EXIT;
  } else if (!q_strcasecmp(mode, "jump")) {
    ent->count = RACE_START_JUMP;
  } else {
    G_Warn("%s has start_mode \"%s\"; it must be touch, exit or jump\n", etos(ent), mode);
    G_FreeEntity(ent);
    return;
  }

  G_trigger_race_Init(ent, G_trigger_race_start_Touch);
  G_Race_AddStart();
}

static void G_trigger_race_checkpoint_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (G_trigger_race_Accepts(ent, other) && G_Race_Checkpoint(other->client, ent->count)) {
    G_UseTargets(ent, other);
  }
}

/*QUAKED trigger_race_checkpoint (.5 .5 .5) ?
 A checkpoint. A course's checkpoints must be numbered 1 through N with none
 missing, and are passed in that order.

 -------- Keys --------
 cp : This checkpoint's number, from 1.
 wait : Seconds before the same client is heard from again (default 0.5).
 message : An optional string to display when the checkpoint is reached.
 target : The name of the entity or team to use when the checkpoint is reached.
 */
static void G_trigger_race_checkpoint(g_entity_t *ent) {

  const cm_entity_t *cp = gi.EntityValue(ent->def, "cp");

  // an unreadable number is recorded as an impossible one, so that the course
  // is spoiled rather than validated around the trigger this frees
  if (!G_Race_AddCheckpoint(cp->parsed & ENTITY_INTEGER ? cp->integer : 0)) {
    G_Warn("%s needs cp, an integer from 1 through %d\n", etos(ent), RACE_MAX_CHECKPOINTS);
    G_FreeEntity(ent);
    return;
  }

  ent->count = cp->integer;

  G_trigger_race_Init(ent, G_trigger_race_checkpoint_Touch);
}

static void G_trigger_race_finish_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (G_trigger_race_Accepts(ent, other) && G_Race_Finish(other->client)) {
    G_UseTargets(ent, other);
  }
}

/*QUAKED trigger_race_finish (.5 .5 .5) ?
 Where a run ends, once every checkpoint has been passed. A course needs at
 least one.

 -------- Keys --------
 wait : Seconds before the same client is heard from again (default 0.5).
 message : An optional string to display when the run ends.
 target : The name of the entity or team to use when the run ends.
 */
static void G_trigger_race_finish(g_entity_t *ent) {

  G_trigger_race_Init(ent, G_trigger_race_finish_Touch);
  G_Race_AddFinish();
}

static const struct {
  const char *classname;
  void (*Init)(g_entity_t *ent);
} g_race_entity_classes[] = {
  { "trigger_race_start", G_trigger_race_start },
  { "trigger_race_checkpoint", G_trigger_race_checkpoint },
  { "trigger_race_finish", G_trigger_race_finish },
};

bool G_Race_SpawnEntity(g_entity_t *ent) {

  for (size_t i = 0; i < lengthof(g_race_entity_classes); i++) {
    if (!q_strcmp(g_race_entity_classes[i].classname, ent->classname)) {
      g_race_entity_classes[i].Init(ent);
      return true;
    }
  }

  return false;
}
