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
 * @brief The entities that describe a course: the triggers, which record
 * themselves with the course as they spawn and hand the client to g_race.c when
 * touched, and the barriers, brushes that are solid to a racer on a condition.
 *
 * The triggers are invisible, and fire their `target` and `message` like a
 * `trigger_multiple` does when the touch counted. `wait` debounces a client
 * standing in one, and defaults to half a second.
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

/**
 * @brief Arms, starts or restarts a run, as the start's mode asks.
 */
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

/**
 * @brief Counts the checkpoint when it is the next one in sequence.
 */
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

/**
 * @brief The trigger's number as its milestones are announced.
 */
static const char *G_trigger_race_Label(const g_entity_t *ent) {
  return gi.EntityValue(ent->def, "label")->nullable_string;
}

/**
 * @brief Records the split and tells the racer how it compares.
 */
static void G_trigger_race_split_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (G_trigger_race_Accepts(ent, other) && G_Race_Split(other->client, ent->count, G_trigger_race_Label(ent))) {
    G_UseTargets(ent, other);
  }
}

/*QUAKED trigger_race_split (.5 .5 .5) ?
 A timing point. A course's splits are numbered 1 through N with none missing
 and are passed in that order, but they only time the run: one missed does not
 spoil it.

 -------- Keys --------
 split : This split's number, from 1.
 label : An optional name, printed with the time (default "Split N").
 wait : Seconds before the same client is heard from again (default 0.5).
 message : An optional string to display when the split is reached.
 target : The name of the entity or team to use when the split is reached.
 */
static void G_trigger_race_split(g_entity_t *ent) {

  const cm_entity_t *split = gi.EntityValue(ent->def, "split");

  if (!G_Race_AddSplit(split->parsed & ENTITY_INTEGER ? split->integer : 0)) {
    G_Warn("%s needs split, an integer from 1 through %d\n", etos(ent), RACE_MAX_CHECKPOINTS);
    G_FreeEntity(ent);
    return;
  }

  ent->count = split->integer;

  G_trigger_race_Init(ent, G_trigger_race_split_Touch);
}

/**
 * @brief Advances the run to the stage and remembers where it restarts.
 */
static void G_trigger_race_stage_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (G_trigger_race_Accepts(ent, other) &&
      G_Race_Stage(other->client, ent->count, G_trigger_race_Label(ent), ent->target_ent)) {
    G_UseTargets(ent, other);
  }
}

/*QUAKED trigger_race_stage (.5 .5 .5) ?
 Where a stage of the course begins. A run begins in stage 1, so stages are
 numbered 2 through N with none missing and entered in that order, and each is
 timed as a split is. A practicing client who reaches one has its restart_target
 stored, as store would, so that kill returns to the stage rather than the spawn.

 -------- Keys --------
 stage : This stage's number, from 2.
 restart_target : The targetname of the info_notnull to return to, placed as a spawn point would be.
 label : An optional name, printed with the time (default "Stage N").
 wait : Seconds before the same client is heard from again (default 0.5).
 message : An optional string to display when the stage is reached.
 target : The name of the entity or team to use when the stage is reached.
 */
static void G_trigger_race_stage(g_entity_t *ent) {

  const cm_entity_t *stage = gi.EntityValue(ent->def, "stage");
  const char *restart = gi.EntityValue(ent->def, "restart_target")->nullable_string;

  const bool complete = (stage->parsed & ENTITY_INTEGER) && restart && *restart;

  if (!G_Race_AddStage(complete ? stage->integer : 0)) {
    G_Warn("%s needs stage, an integer from 2 through %d, and restart_target\n", etos(ent), RACE_MAX_CHECKPOINTS);
    G_FreeEntity(ent);
    return;
  }

  ent->count = stage->integer;

  G_trigger_race_Init(ent, G_trigger_race_stage_Touch);
}

/**
 * @see g_race.h
 */
void G_Race_ResolveStages(void) {

  g_entity_t *stage = NULL;
  while ((stage = G_Find(stage, EOFS(classname), "trigger_race_stage"))) {

    const char *name = gi.EntityValue(stage->def, "restart_target")->nullable_string;

    g_entity_t *anchor = G_Find(NULL, EOFS(target_name), name);

    if (!anchor || q_strcmp(anchor->classname, "info_notnull") || G_Find(anchor, EOFS(target_name), name)) {
      G_Warn("%s needs restart_target to name one info_notnull, and \"%s\" does not\n", etos(stage), name);
      g_level.race_course.stages_valid = false;
      continue;
    }

    stage->target_ent = anchor;
  }
}

/**
 * @brief Ends the run, submits the record, and says how it went.
 */
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

/**
 * @brief The common half of every barrier's setup: an inline brush, solid,
 * listed with the course so that `G_Race_UpdateBarriers` can decide it.
 */
static void G_func_race_Init(g_entity_t *ent, g_race_barrier_t barrier) {

  if (!ent->model || ent->model[0] != '*') {
    G_Warn("%s needs a brush\n", etos(ent));
    G_FreeEntity(ent);
    return;
  }

  if (g_level.race_course.barrier_count == RACE_MAX_BARRIERS) {
    G_Warn("%s is one func_race_* too many; the level may have %d\n", etos(ent), RACE_MAX_BARRIERS);
    G_FreeEntity(ent);
    return;
  }

  ent->race_barrier = barrier;
  ent->race_barrier_slot = g_level.race_course.barrier_count;
  ent->solid = SOLID_BSP;
  ent->move_type = MOVE_TYPE_NONE;

  gi.SetModel(ent, ent->model);
  gi.LinkEntity(ent);

  g_level.race_course.barriers[g_level.race_course.barrier_count++] = ent;
}

/*QUAKED func_race_checkpoint_gate (0 .5 .8) ?
 A brush that is solid to a racer until the run satisfies its checkpoint
 condition, and solid to everything else always. A client with no run under way
 passes freely. Texture it with clip so that it is never seen. The gate opens
 the frame after its checkpoint is reached, so leave a stride between them.

 -------- Keys --------
 cp : The checkpoint the gate is about, from 1.
 mode : atleast (default) opens the gate once checkpoint cp has been reached; exact opens it only while cp is the last one reached.
 invert : 1 to close the gate under the condition instead of opening it.
 */
static void G_func_race_checkpoint_gate(g_entity_t *ent) {

  const cm_entity_t *cp = gi.EntityValue(ent->def, "cp");
  const char *mode = gi.EntityValue(ent->def, "mode")->nullable_string;

  if (!(cp->parsed & ENTITY_INTEGER) || cp->integer < 1 || cp->integer > RACE_MAX_CHECKPOINTS) {
    G_Warn("%s needs cp, an integer from 1 through %d\n", etos(ent), RACE_MAX_CHECKPOINTS);
    G_FreeEntity(ent);
    return;
  }

  g_race_gate_t gate = {
    .checkpoint = cp->integer,
    .invert = gi.EntityValue(ent->def, "invert")->integer != 0,
  };

  if (!mode || !*mode || !q_strcasecmp(mode, "atleast")) {
    gate.mode = RACE_GATE_AT_LEAST;
  } else if (!q_strcasecmp(mode, "exact")) {
    gate.mode = RACE_GATE_EXACT;
  } else {
    G_Warn("%s has mode \"%s\"; it must be atleast or exact\n", etos(ent), mode);
    G_FreeEntity(ent);
    return;
  }

  ent->race_gate = gate;

  G_func_race_Init(ent, RACE_BARRIER_GATE);
}

/*QUAKED func_race_oneway_wall (0 .5 .8) ?
 A brush that a racer passes through in one direction and not the other, and
 that is solid to everything else. Texture it with clip so that it is never seen.
 The wall is judged by which side of its centre a racer stands: it passes them
 from its entry side in any direction, and stops them from the far side. Make it
 one thin brush from floor to ceiling, so that there is no over, under or gap.

 -------- Keys --------
 angle : The direction of travel that passes, in the horizontal plane.
 */
static void G_func_race_oneway_wall(g_entity_t *ent) {

  G_SetMoveDir(ent);
  ent->move_dir.z = 0.f;

  if (Vec3_Equal(ent->move_dir, Vec3_Zero())) {
    G_Warn("%s needs angle, a direction of travel in the horizontal plane\n", etos(ent));
    G_FreeEntity(ent);
    return;
  }

  G_func_race_Init(ent, RACE_BARRIER_WALL);
}

/**
 * @brief Whether `ent`, a barrier, lets `cl` pass as they stand right now.
 *
 * A gate opens on the run's checkpoints. A wall passes a client on its entry
 * side or already inside it, and stops one on the far side, which is the one
 * direction it allows without anyone measuring a direction: a client who is
 * inside came in the way it permits, and one who has crossed is behind it.
 */
static bool G_Race_Passes(const g_client_t *cl, const g_entity_t *ent) {

  if (ent->race_barrier == RACE_BARRIER_GATE) {
    const g_race_run_t *run = &cl->race_run;

    if (run->state != RACE_RUN_ACTIVE) {
      return true;
    }

    const g_race_gate_t *gate = &ent->race_gate;
    const bool open = gate->mode == RACE_GATE_EXACT
                      ? run->checkpoint_count == gate->checkpoint
                      : run->checkpoint_count >= gate->checkpoint;

    return open != gate->invert;
  }

  const g_entity_t *self = cl->entity;

  if (gi.Clip(self->s.origin, self->s.origin, self->bounds, ent, CONTENTS_MASK_CLIP_PLAYER).start_solid) {
    return true;
  }

  const vec3_t offset = Vec3_Subtract(self->s.origin, Box3_Center(ent->abs_bounds));

  return offset.x * ent->move_dir.x + offset.y * ent->move_dir.y < 0.f;
}

/**
 * @see g_race.h
 */
void G_Race_UpdateBarriers(g_client_t *cl) {
  const g_race_course_t *course = &g_level.race_course;

  if (!course->barrier_count) {
    return;
  }

  cl->race_passable = 0;
  int32_t count = 0;

  for (uint16_t i = 0; i < course->barrier_count; i++) {
    if (G_Race_Passes(cl, course->barriers[i])) {
      cl->race_passable |= 1u << i;
      count++;
    }
  }

  gi.WriteByte(SV_CMD_RACE_BARRIERS);
  gi.WriteByte(count);

  for (uint16_t i = 0; i < course->barrier_count; i++) {
    if (cl->race_passable & (1u << i)) {
      gi.WriteShort(course->barriers[i]->s.number);
    }
  }

  gi.Unicast(cl, false);
}

/**
 * @brief Called for every entity a trace considers, so the ones that are not
 * barriers, and everything that is not a client, are turned away first. The
 * answer is the one `G_Race_UpdateBarriers` settled and sent at the end of the
 * last frame, so that the client predicts against the same set.
 */
bool G_Race_ClipEntity(const g_entity_t *mover, const g_entity_t *ent) {

  if (ent->race_barrier == RACE_BARRIER_NONE || !mover || !mover->client) {
    return true;
  }

  return !(mover->client->race_passable & (1u << ent->race_barrier_slot));
}

static const struct {
  const char *classname;
  void (*Init)(g_entity_t *ent);
} g_race_entity_classes[] = {
  { "trigger_race_start", G_trigger_race_start },
  { "trigger_race_checkpoint", G_trigger_race_checkpoint },
  { "trigger_race_split", G_trigger_race_split },
  { "trigger_race_stage", G_trigger_race_stage },
  { "trigger_race_finish", G_trigger_race_finish },
  { "func_race_checkpoint_gate", G_func_race_checkpoint_gate },
  { "func_race_oneway_wall", G_func_race_oneway_wall },
};

/**
 * @see g_race.h
 */
bool G_Race_InitEntity(g_entity_t *ent) {

  for (size_t i = 0; i < lengthof(g_race_entity_classes); i++) {
    if (!q_strcmp(g_race_entity_classes[i].classname, ent->classname)) {
      g_race_entity_classes[i].Init(ent);
      return true;
    }
  }

  return false;
}
