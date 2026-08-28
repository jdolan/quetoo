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
 * @brief The run and the rules. See g_race.h.
 *
 * A run is a sequence: start, checkpoints 1 through N in order, finish. The
 * course says what N is and whether it has a start zone at all; a course with
 * none starts the run on the client's first movement. Times are `g_level.time`,
 * in milliseconds, and the run keeps its own so that the HUD can show it and a
 * record can later be made of it.
 *
 * Two modes, chosen by the client. Racing is what counts: the grapple and
 * noclip are refused, and a valid finish is announced. Practicing is for
 * learning the course: everything is allowed, `store` remembers a position that
 * `kill` returns to, and a finish is the client's business alone. Nobody is
 * ever hurt: players pass through each other, another player's attack does
 * nothing at all, and everything else - your own rockets, the world - keeps its
 * knockback and loses its damage, because rocket jumps are half the point.
 */

// how quickly kill may be repeated, which racers do constantly
#define RACE_KILL_INTERVAL 300

static struct {
  LevelWillSpawn LevelWillSpawn;
  ConfigureLevel ConfigureLevel;
  SpawnEntity SpawnEntity;
  ClipEntity ClipEntity;
  PrepareSpawn PrepareSpawn;
  TossInventory TossInventory;
  ModifyDamage ModifyDamage;
  AllowHook AllowHook;
  HandleClientCommand HandleClientCommand;
  ClientWillThink ClientWillThink;
  ClientDidMove ClientDidMove;
  ClientWillDisconnect ClientWillDisconnect;
  WriteStats WriteStats;
} previous;

static bool installed;

g_race_mode_t G_Race_Mode(const g_client_t *cl) {

  if (cl->persistent.spectator) {
    return RACE_MODE_SPECTATOR;
  }

  return cl->persistent.race_mode == RACE_MODE_PRACTICE ? RACE_MODE_PRACTICE : RACE_MODE_RACE;
}

static const char *G_Race_ModeName(g_race_mode_t mode) {

  switch (mode) {
    case RACE_MODE_RACE:
      return "racing";
    case RACE_MODE_PRACTICE:
      return "practicing";
    default:
      return "spectating";
  }
}

/**
 * @brief Whether `cl` is alive and taking part, which is what every step of a
 * run requires.
 */
static bool G_Race_CanRun(const g_client_t *cl) {

  return cl->entity && cl->entity->in_use && !cl->entity->dead && cl->entity->health > 0 &&
         G_Race_Mode(cl) != RACE_MODE_SPECTATOR;
}

/**
 * @brief Formats a run time for printing.
 */
static const char *G_Race_FormatTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

void G_Race_CenterPrint(const g_client_t *cl, const char *fmt, ...) {
  char string[MAX_STRING_CHARS];

  va_list args;
  va_start(args, fmt);
  vsnprintf(string, sizeof(string), fmt, args);
  va_end(args);

  gi.WriteByte(SV_CMD_CENTER_PRINT);
  gi.WriteString(string);
  gi.Unicast(cl, true);
}

static void G_Race_Reset(g_client_t *cl) {

  G_Race_DropLine(cl);
  G_Race_RemoveGhost(cl);

  memset(&cl->race_run, 0, sizeof(cl->race_run));
  cl->race_start = NULL;
  cl->race_trigger = NULL;
}

// ---------------------------------------------------------------- the course

void G_Race_AddStart(void) {
  g_level.race_course.start_count++;
}

/**
 * @brief Records number `n` of a sequence that runs from `first`, or marks the
 * sequence malformed for an `n` that cannot be one of it.
 */
static bool G_Race_AddToSequence(uint64_t *sequence, bool *malformed, int32_t n, int32_t first) {

  if (n < first || n > RACE_MAX_CHECKPOINTS) {
    *malformed = true;
    return false;
  }

  *sequence |= UINT64_C(1) << (n - 1);
  return true;
}

bool G_Race_AddCheckpoint(int32_t checkpoint) {
  return G_Race_AddToSequence(&g_level.race_course.checkpoints, &g_level.race_course.malformed, checkpoint, 1);
}

bool G_Race_AddSplit(int32_t split) {
  return G_Race_AddToSequence(&g_level.race_course.splits, &g_level.race_course.splits_malformed, split, 1);
}

bool G_Race_AddStage(int32_t stage) {
  return G_Race_AddToSequence(&g_level.race_course.stages, &g_level.race_course.stages_malformed, stage, 2);
}

void G_Race_AddFinish(void) {
  g_level.race_course.finish_count++;
}

/**
 * @brief The lowest `n` bits.
 */
static uint64_t G_Race_Bits(int32_t n) {
  return n >= 64 ? UINT64_MAX : (UINT64_C(1) << n) - 1;
}

/**
 * @brief Whether `sequence` is exactly `first` through N with none missing,
 * for an N it reports. An empty sequence is a valid one with N of zero.
 */
static bool G_Race_ValidateSequence(uint64_t sequence, bool malformed, int32_t first, uint16_t *count) {

  *count = 0;
  for (int32_t i = RACE_MAX_CHECKPOINTS; i > 0; i--) {
    if (sequence & (UINT64_C(1) << (i - 1))) {
      *count = i;
      break;
    }
  }

  const uint64_t expected = *count ? G_Race_Bits(*count) & ~G_Race_Bits(first - 1) : 0;

  return !malformed && sequence == expected;
}

/**
 * @brief A course is valid when its checkpoints are exactly 1 through N with
 * none missing, and it has somewhere to finish. Splits and stages are valid
 * on their own terms and spoil only themselves.
 */
static void G_Race_ValidateCourse(void) {
  g_race_course_t *course = &g_level.race_course;

  course->valid = G_Race_ValidateSequence(course->checkpoints, course->malformed, 1, &course->checkpoint_count) &&
                  course->finish_count > 0;

  course->splits_valid = G_Race_ValidateSequence(course->splits, course->splits_malformed, 1, &course->split_count);
  course->stages_valid = G_Race_ValidateSequence(course->stages, course->stages_malformed, 2, &course->stage_count);
}

// ---------------------------------------------------------------- the run

bool G_Race_Debounced(g_client_t *cl, const g_entity_t *ent, float wait) {

  if (cl->race_trigger == ent && g_level.time - cl->race_trigger_time < wait * 1000.f) {
    return true;
  }

  cl->race_trigger = ent;
  cl->race_trigger_time = g_level.time;
  return false;
}

bool G_Race_Start(g_client_t *cl) {

  if (!G_Race_CanRun(cl)) {
    return false;
  }

  if (!g_level.race_course.valid) {
    gi.ClientPrint(cl, PRINT_HIGH, "This level has no valid course: it needs a finish and checkpoints 1 through N\n");
    return false;
  }

  G_Race_Reset(cl);

  g_race_run_t *run = &cl->race_run;
  const float speed = Vec3_Length(cl->entity->velocity);

  run->state = RACE_RUN_ACTIVE;
  run->mode = G_Race_Mode(cl);
  run->movement = cl->ps.pm_state.params.movement;
  run->stage = 1;
  run->start_time = g_level.time;
  run->start_speed = run->top_speed = speed;

  if (cl->entity->move_type == MOVE_TYPE_NO_CLIP) {
    run->invalid |= RACE_INVALID_NOCLIP;
  }

  G_Race_BeginLine(cl);
  G_Race_SpawnGhost(cl);

  G_Race_CenterPrint(cl, "^2Go!");
  return true;
}

void G_Race_ArmStart(g_client_t *cl, const g_entity_t *start) {

  if (cl->race_start == start) {
    return;
  }

  // entering a start zone abandons whatever run was under way
  if (cl->race_run.state != RACE_RUN_IDLE) {
    G_Race_Reset(cl);
  }

  cl->race_start = start;
}

bool G_Race_Checkpoint(g_client_t *cl, uint16_t checkpoint) {
  g_race_run_t *run = &cl->race_run;

  if (!G_Race_CanRun(cl) || run->state != RACE_RUN_ACTIVE) {
    return false;
  }

  const uint16_t expected = run->checkpoint_count + 1;

  if (checkpoint < expected) { // already reached, so standing in it again means nothing
    return false;
  }

  if (checkpoint > expected) {
    G_Race_CenterPrint(cl, "Checkpoint %u skipped", expected);
    return false;
  }

  run->checkpoint_times[run->checkpoint_count++] = g_level.time - run->start_time;

  G_Race_CenterPrint(cl, "Checkpoint %u  %s", checkpoint, G_Race_FormatTime(run->checkpoint_times[checkpoint - 1]));

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.teleport,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

/**
 * @brief A split times the run without judging it: one reached out of order
 * is simply not a split of this run.
 */
bool G_Race_Split(g_client_t *cl, uint16_t split, const char *label) {
  g_race_run_t *run = &cl->race_run;

  if (!G_Race_CanRun(cl) || run->state != RACE_RUN_ACTIVE || !g_level.race_course.splits_valid) {
    return false;
  }

  if (split != run->split_count + 1) {
    return false;
  }

  const uint32_t time = g_level.time - run->start_time;
  const uint32_t previous = run->split_count ? run->split_times[run->split_count - 1] : 0;

  run->split_times[run->split_count++] = time;

  G_Race_CenterPrint(cl, "%s  %s  (+%s)", label && *label ? label : va("Split %u", split),
                     G_Race_FormatTime(time), G_Race_FormatTime(time - previous));
  return true;
}

/**
 * @brief A stage is timed as a split is, and is also where a practicing client
 * returns to: reaching one stores its anchor, as `store` would.
 */
bool G_Race_Stage(g_client_t *cl, uint16_t stage, const char *label, const g_entity_t *anchor) {
  g_race_run_t *run = &cl->race_run;

  if (!G_Race_CanRun(cl) || !g_level.race_course.stages_valid) {
    return false;
  }

  const char *name = label && *label ? label : va("Stage %u", stage);
  bool counted = false;

  if (run->state == RACE_RUN_ACTIVE && stage == run->stage + 1) {
    run->stage_times[stage - 2] = g_level.time - run->start_time;
    run->stage = stage;

    G_Race_CenterPrint(cl, "%s  %s", name, G_Race_FormatTime(run->stage_times[stage - 2]));
    counted = true;
  }

  if (G_Race_Mode(cl) == RACE_MODE_PRACTICE) {
    g_race_spawn_t *spawn = &cl->persistent.race_spawn;

    if (!spawn->set || !Vec3_Equal(spawn->origin, anchor->s.origin)) {
      *spawn = (g_race_spawn_t) {
        .origin = anchor->s.origin,
        .angles = anchor->s.angles,
        .set = true,
      };

      if (!counted) {
        G_Race_CenterPrint(cl, "%s. ^2kill^7 returns here", name);
      }
      counted = true;
    }
  }

  return counted;
}

bool G_Race_Finish(g_client_t *cl) {
  g_race_run_t *run = &cl->race_run;

  if (!G_Race_CanRun(cl) || run->state != RACE_RUN_ACTIVE) {
    return false;
  }

  const uint16_t remaining = g_level.race_course.checkpoint_count - run->checkpoint_count;
  if (remaining) {
    gi.ClientPrint(cl, PRINT_HIGH, "%u checkpoint%s remaining\n", remaining, remaining == 1 ? "" : "s");
    return false;
  }

  if (cl->entity->move_type == MOVE_TYPE_NO_CLIP) {
    run->invalid |= RACE_INVALID_NOCLIP;
  }

  if (cl->ps.pm_state.params.movement != run->movement) {
    run->invalid |= RACE_INVALID_MOVEMENT;
  }

  run->state = RACE_RUN_FINISHED;
  run->elapsed = g_level.time - run->start_time;
  run->end_speed = Vec3_Length(cl->entity->velocity);
  run->top_speed = Maxf(run->top_speed, run->end_speed);

  const char *time = G_Race_FormatTime(run->elapsed);

  // it counts only if it was raced from start to finish with nothing to disqualify it
  if (run->mode == RACE_MODE_RACE && G_Race_Mode(cl) == RACE_MODE_RACE && !run->invalid) {
    gi.BroadcastPrint(PRINT_HIGH, "%s finished in %s\n", cl->persistent.net_name, time);
    if (G_Race_SubmitRecord(cl)) {
      G_Race_KeepLine(cl);
    }
  } else if (run->mode == RACE_MODE_PRACTICE) {
    G_Race_CenterPrint(cl, "Finished in %s, practicing", time);
  } else {
    G_Race_CenterPrint(cl, "Finished in %s, but ^1it does not count^7", time);
  }

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.teleport,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

// ---------------------------------------------------------------- the modes

static void G_Race_SetMode(g_client_t *cl, g_race_mode_t mode) {

  if (G_Race_Mode(cl) == mode) {
    gi.ClientPrint(cl, PRINT_HIGH, "You are already %s\n", G_Race_ModeName(mode));
    return;
  }

  if (g_level.time - cl->respawn_time < 1000) { // as spectate and join are
    return;
  }

  if (mode == RACE_MODE_SPECTATOR) {
    G_TossInventory(cl); // which resets the run
    cl->persistent.spectator = true;
    cl->persistent.race_spawn.set = false;
  } else {
    G_Race_Reset(cl);
    cl->persistent.spectator = false;
    cl->persistent.race_mode = mode;
  }

  gi.BroadcastPrint(PRINT_HIGH, "%s is %s\n", cl->persistent.net_name, G_Race_ModeName(mode));

  if (mode == RACE_MODE_PRACTICE) {
    gi.ClientPrint(cl, PRINT_HIGH, "Nothing counts while practicing. ^2store^7 remembers where you are; ^2kill^7 takes you back\n");
  }

  G_ClientRespawn(cl, false);
}

static void G_Race_Mode_f(g_client_t *cl) {

  if (gi.Argc() < 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "You are %s. Use mode race|practice|spectator\n",
                   G_Race_ModeName(G_Race_Mode(cl)));
    return;
  }

  const char *name = gi.Argv(1);

  if (!q_strcasecmp(name, "race")) {
    G_Race_SetMode(cl, RACE_MODE_RACE);
  } else if (!q_strcasecmp(name, "practice")) {
    G_Race_SetMode(cl, RACE_MODE_PRACTICE);
  } else if (!q_strcasecmp(name, "spectator") || !q_strcasecmp(name, "spectate")) {
    G_Race_SetMode(cl, RACE_MODE_SPECTATOR);
  } else {
    gi.ClientPrint(cl, PRINT_HIGH, "Unknown mode \"%s\". Use race, practice or spectator\n", name);
  }
}

/**
 * @brief Remembers where a practicing client is standing, for `kill`.
 */
static void G_Race_Store_f(g_client_t *cl) {

  if (G_Race_Mode(cl) != RACE_MODE_PRACTICE) {
    gi.ClientPrint(cl, PRINT_HIGH, "Only while practicing\n");
    return;
  }

  if (!G_Race_CanRun(cl)) {
    return;
  }

  // in the spawn point's terms, so that respawning here puts the feet back
  // exactly where they were
  vec3_t origin = cl->entity->s.origin;
  origin.z -= PM_STEP_HEIGHT;

  cl->persistent.race_spawn = (g_race_spawn_t) {
    .origin = origin,
    .angles = cl->angles,
    .set = true,
  };

  gi.ClientPrint(cl, PRINT_HIGH, "Stored. ^2kill^7 returns here\n");
}

/**
 * @brief Respawns at once, with no corpse and no death: for racing, `kill`
 * means "again", and it is pressed constantly.
 */
static void G_Race_Kill_f(g_client_t *cl) {

  if (cl->persistent.spectator || !cl->entity || cl->entity->dead) {
    return;
  }

  if (g_level.time - cl->respawn_time < RACE_KILL_INTERVAL) {
    return;
  }

  G_Race_Reset(cl);
  G_ClientRespawn(cl, false);
}

/**
 * @brief Practicing allows noclip regardless of cheats; racing refuses it under
 * the usual rule, and any run it touches does not count.
 */
static void G_Race_NoClip_f(g_client_t *cl) {

  if (cl->persistent.spectator || !cl->entity) {
    return;
  }

  const bool practicing = G_Race_Mode(cl) == RACE_MODE_PRACTICE;
  const bool cheating = sv_max_clients->integer <= 1 || g_cheats->value;

  if (!practicing && !cheating) {
    gi.ClientPrint(cl, PRINT_HIGH, "Cheats are disabled\n");
    return;
  }

  if (cl->entity->move_type == MOVE_TYPE_NO_CLIP) {
    cl->entity->move_type = MOVE_TYPE_WALK;
    gi.ClientPrint(cl, PRINT_HIGH, "no_clip disabled\n");
  } else {
    cl->entity->move_type = MOVE_TYPE_NO_CLIP;
    cl->race_run.invalid |= RACE_INVALID_NOCLIP;
    gi.ClientPrint(cl, PRINT_HIGH, "no_clip enabled\n");
  }
}

static void G_Race_Status_f(g_client_t *cl) {
  const g_race_run_t *run = &cl->race_run;
  const g_race_course_t *course = &g_level.race_course;

  gi.ClientPrint(cl, PRINT_HIGH, "Course: %u checkpoint%s, %u start%s, %u finish%s%s\n",
                 course->checkpoint_count, course->checkpoint_count == 1 ? "" : "s",
                 course->start_count, course->start_count == 1 ? "" : "s",
                 course->finish_count, course->finish_count == 1 ? "" : "es",
                 course->valid ? "" : " ^1(invalid)^7");

  const g_race_record_t *record = G_Race_Record(cl->persistent.guid, g_level.movement);
  if (record) {
    size_t count;
    const size_t rank = G_Race_Rank(record, &count);
    gi.ClientPrint(cl, PRINT_HIGH, "Best: %s, #%zu of %zu\n", G_Race_FormatTime(record->time), rank, count);
  }

  switch (run->state) {
    case RACE_RUN_ACTIVE:
      gi.ClientPrint(cl, PRINT_HIGH, "Running: %s, checkpoint %u of %u%s\n",
                     G_Race_FormatTime(g_level.time - run->start_time),
                     run->checkpoint_count, course->checkpoint_count,
                     course->stages_valid && course->stage_count ? va(", stage %u of %u", run->stage, course->stage_count) : "");
      break;
    case RACE_RUN_FINISHED:
      gi.ClientPrint(cl, PRINT_HIGH, "Finished: %s\n", G_Race_FormatTime(run->elapsed));
      break;
    default:
      gi.ClientPrint(cl, PRINT_HIGH, "%s, not running\n", G_Race_ModeName(G_Race_Mode(cl)));
      break;
  }
}

// ---------------------------------------------------------------- the hooks

/**
 * @brief The level's entities are about to go, the ghosts among them, and with
 * them every run.
 */
static void G_LevelWillSpawn_Race(const char *name) {

  G_ForEachClient(cl, {
    G_Race_Reset(cl);
  });

  previous.LevelWillSpawn(name);
}

static void G_ConfigureLevel_Race(void) {

  previous.ConfigureLevel();

  G_Race_ValidateCourse();
  G_Race_ResolveStages();
  G_Race_LoadRecords();
  G_Race_LoadLine();

  const g_race_course_t *course = &g_level.race_course;

  gi.SetConfigString(CS_RACE_COURSE, va("%u\\%u\\%u", course->checkpoint_count,
                                        course->finish_count, course->valid));

  if (!course->valid) {
    G_Warn("%s has no valid course: it needs a finish and checkpoints 1 through N\n", g_level.name);
  }

  // the course is new, and so is the ground a stored position stood on
  G_ForEachClient(cl, {
    G_Race_Reset(cl);
    cl->persistent.race_spawn.set = false;
  });
}

static bool G_SpawnEntity_Race(g_entity_t *ent) {

  if (G_Race_SpawnEntity(ent)) {
    return true;
  }

  return previous.SpawnEntity(ent);
}

static bool G_ClipEntity_Race(const g_entity_t *mover, const g_entity_t *ent, const vec3_t start, const vec3_t end, const box3_t bounds) {

  if (!G_Race_ClipEntity(mover, ent, start, end, bounds)) {
    return false;
  }

  return previous.ClipEntity(mover, ent, start, end, bounds);
}

/**
 * @brief Players pass through each other and telefrag nobody, and a practicing
 * client who has stored a position spawns there.
 */
static void G_PrepareSpawn_Race(g_client_t *cl, g_client_spawn_t *spawn) {

  spawn->clip_mask &= ~CONTENTS_MONSTER;
  spawn->kill_box = false;

  if (G_Race_Mode(cl) == RACE_MODE_PRACTICE && cl->persistent.race_spawn.set) {
    spawn->origin = cl->persistent.race_spawn.origin;
    spawn->angles = cl->persistent.race_spawn.angles;
  }

  previous.PrepareSpawn(cl, spawn);
}

static void G_TossInventory_Race(g_client_t *cl) {

  G_Race_Reset(cl);

  previous.TossInventory(cl);
}

/**
 * @brief Nobody is hurt. An attack from another player does nothing at all;
 * anything else keeps its knockback and loses its damage, which is what a
 * rocket jump needs and a lava pit does not get.
 */
static bool G_ModifyDamage_Race(const g_damage_t *dmg, int32_t *damage, int32_t *knockback) {

  if (!dmg->target->client) {
    return previous.ModifyDamage(dmg, damage, knockback);
  }

  if (dmg->attacker->client && dmg->attacker != dmg->target) {
    return false;
  }

  if (!previous.ModifyDamage(dmg, damage, knockback)) {
    return false;
  }

  *damage = 0;
  return true;
}

static bool G_AllowHook_Race(const g_client_t *cl) {

  if (G_Race_Mode(cl) != RACE_MODE_PRACTICE) {
    return false;
  }

  return previous.AllowHook(cl);
}

static bool G_HandleClientCommand_Race(g_client_t *cl, const char *cmd, bool intermission) {

  if (intermission) {
    return previous.HandleClientCommand(cl, cmd, intermission);
  }

  if (!q_strcmp(cmd, "race")) {
    G_Race_Status_f(cl);
  } else if (!q_strcmp(cmd, "mode")) {
    G_Race_Mode_f(cl);
  } else if (!q_strcmp(cmd, "store")) {
    G_Race_Store_f(cl);
  } else if (!q_strcmp(cmd, "kill")) {
    G_Race_Kill_f(cl);
  } else if (!q_strcmp(cmd, "no_clip")) {
    G_Race_NoClip_f(cl);
  } else if (!q_strcmp(cmd, "ghost")) {
    G_Race_Ghost_f(cl);
  } else {
    return previous.HandleClientCommand(cl, cmd, intermission);
  }

  return true;
}

/**
 * @brief Whether `cl` is standing in `ent`. By bounds, because that is what a
 * trigger's touch is: the server never traces against a SOLID_TRIGGER, so a
 * clip against one reports nothing however deep inside it the box is.
 */
static bool G_Race_Inside(const g_client_t *cl, const g_entity_t *ent) {

  return ent->in_use && Box3_Intersects(cl->entity->abs_bounds, ent->abs_bounds);
}

/**
 * @brief The starts that begin on input: a jump out of a jump-mode zone, or the
 * first movement when the course has no start zone at all. `cl->cmd` is still
 * the previous command here, which is what makes the jump an edge.
 */
static void G_ClientWillThink_Race(g_client_t *cl, const pm_cmd_t *cmd) {

  previous.ClientWillThink(cl, cmd);

  if (!G_Race_CanRun(cl)) {
    return;
  }

  if (cl->race_start) {
    if (cl->race_start->count == RACE_START_JUMP && cmd->up > 0 && cl->cmd.up <= 0) {
      const g_entity_t *start = cl->race_start;
      cl->race_start = NULL;

      if (G_Race_Inside(cl, start) && G_Race_Start(cl)) {
        G_UseTargets((g_entity_t *) start, cl->entity);
      }
    }
    return;
  }

  if (!g_level.race_course.start_count && cl->race_run.state == RACE_RUN_IDLE &&
      (cmd->forward || cmd->right || cmd->up)) {
    G_Race_Start(cl);
  }
}

/**
 * @brief Samples the speed for the finish report, and starts the run for a
 * client who has just left an exit-mode start zone.
 */
static void G_ClientDidMove_Race(g_client_t *cl, const pm_cmd_t *cmd) {

  previous.ClientDidMove(cl, cmd);

  if (!G_Race_CanRun(cl)) {
    return;
  }

  g_race_run_t *run = &cl->race_run;

  if (run->state == RACE_RUN_ACTIVE) {
    const float speed = Vec3_Length(cl->entity->velocity);

    run->top_speed = Maxf(run->top_speed, speed);
    run->speed_sum += speed;
    run->speed_samples++;

    G_Race_SampleLine(cl);
  }

  if (cl->race_start && !G_Race_Inside(cl, cl->race_start)) {
    const g_entity_t *start = cl->race_start;
    cl->race_start = NULL;

    if (start->count == RACE_START_EXIT && G_Race_Start(cl)) {
      G_UseTargets((g_entity_t *) start, cl->entity);
    }
  }
}

static void G_ClientWillDisconnect_Race(g_client_t *cl) {

  G_Race_Reset(cl);

  previous.ClientWillDisconnect(cl);
}

static void G_WriteStats_Race(g_client_t *cl) {

  previous.WriteStats(cl);

  const g_race_run_t *run = &cl->race_run;

  uint32_t time = 0;
  if (run->state == RACE_RUN_ACTIVE) {
    time = g_level.time - run->start_time;
  } else if (run->state == RACE_RUN_FINISHED) {
    time = run->elapsed;
  }

  cl->ps.stats[STAT_RACE_MODE] = G_Race_Mode(cl);
  cl->ps.stats[STAT_RACE_RUN] = run->state;
  cl->ps.stats[STAT_RACE_TIME_LOW] = (int16_t) (uint16_t) time;
  cl->ps.stats[STAT_RACE_TIME_HIGH] = (int16_t) (uint16_t) (time >> 16);
  cl->ps.stats[STAT_RACE_CHECKPOINTS] = run->checkpoint_count;
  cl->ps.stats[STAT_RACE_FLAGS] = run->invalid;
}

void G_Race_Init(void) {

  if (installed) {
    return;
  }

  installed = true;

  previous.LevelWillSpawn = G_LevelWillSpawn;
  G_LevelWillSpawn = G_LevelWillSpawn_Race;

  previous.ConfigureLevel = G_ConfigureLevel;
  G_ConfigureLevel = G_ConfigureLevel_Race;

  previous.SpawnEntity = G_SpawnEntity;
  G_SpawnEntity = G_SpawnEntity_Race;

  previous.ClipEntity = G_ClipEntity;
  G_ClipEntity = G_ClipEntity_Race;

  previous.PrepareSpawn = G_PrepareSpawn;
  G_PrepareSpawn = G_PrepareSpawn_Race;

  previous.TossInventory = G_TossInventory;
  G_TossInventory = G_TossInventory_Race;

  previous.ModifyDamage = G_ModifyDamage;
  G_ModifyDamage = G_ModifyDamage_Race;

  previous.AllowHook = G_AllowHook;
  G_AllowHook = G_AllowHook_Race;

  previous.HandleClientCommand = G_HandleClientCommand;
  G_HandleClientCommand = G_HandleClientCommand_Race;

  previous.ClientWillThink = G_ClientWillThink;
  G_ClientWillThink = G_ClientWillThink_Race;

  previous.ClientDidMove = G_ClientDidMove;
  G_ClientDidMove = G_ClientDidMove_Race;

  previous.ClientWillDisconnect = G_ClientWillDisconnect;
  G_ClientWillDisconnect = G_ClientWillDisconnect_Race;

  previous.WriteStats = G_WriteStats;
  G_WriteStats = G_WriteStats_Race;
}
