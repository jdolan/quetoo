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
 * none starts the run on the client's first movement. Times are `gLevel.time`,
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
  InitEntity InitEntity;
  ClipEntity ClipEntity;
  PrepareSpawn PrepareSpawn;
  TossInventory TossInventory;
  ModifyDamage ModifyDamage;
  AllowHook AllowHook;
  HandleClientCommand HandleClientCommand;
  ClientWillThink ClientWillThink;
  ClientDidMove ClientDidMove;
  FrameDidEnd FrameDidEnd;
  ClientWillDisconnect ClientWillDisconnect;
  WriteStats WriteStats;
  WriteScore WriteScore;
} previous;

static bool installed;

/**
 * @see g_race.h
 */
GameRaceMode G_Race_Mode(const GameClient *cl) {

  if (cl->persistent.spectator) {
    return RACE_MODE_SPECTATOR;
  }

  return cl->persistent.raceMode == RACE_MODE_PRACTICE ? RACE_MODE_PRACTICE : RACE_MODE_RACE;
}

/**
 * @brief What a mode is called when it is announced or listed.
 */
static const char *G_Race_ModeName(GameRaceMode mode) {

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
static bool G_Race_CanRun(const GameClient *cl) {

  return cl->entity && cl->entity->inUse && !cl->entity->dead && cl->entity->health > 0 &&
         G_Race_Mode(cl) != RACE_MODE_SPECTATOR;
}

/**
 * @brief Formats a run time for printing.
 */
static const char *G_Race_FormatTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

/**
 * @see g_race.h
 */
void G_Race_CenterPrint(const GameClient *cl, const char *fmt, ...) {
  char string[MAX_STRING_CHARS];

  va_list args;
  va_start(args, fmt);
  vsnprintf(string, sizeof(string), fmt, args);
  va_end(args);

  gi.WriteByte(SV_CMD_CENTER_PRINT);
  gi.WriteString(string);
  gi.Unicast(cl, true);
}

/**
 * @brief Abandons the run, wherever it stood.
 */
static void G_Race_Reset(GameClient *cl) {

  G_Race_DropLine(cl);
  G_Race_RemoveGhost(cl);

  memset(&cl->raceRun, 0, sizeof(cl->raceRun));
  cl->raceStart = NULL;
  cl->raceTrigger = NULL;
}

// ---------------------------------------------------------------- the course

/**
 * @see g_race.h
 */
void G_Race_AddStart(void) {
  gLevel.raceCourse.startCount++;
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

/**
 * @see g_race.h
 */
bool G_Race_AddCheckpoint(int32_t checkpoint) {
  return G_Race_AddToSequence(&gLevel.raceCourse.checkpoints, &gLevel.raceCourse.malformed, checkpoint, 1);
}

/**
 * @see g_race.h
 */
bool G_Race_AddSplit(int32_t split) {
  return G_Race_AddToSequence(&gLevel.raceCourse.splits, &gLevel.raceCourse.splitsMalformed, split, 1);
}

/**
 * @see g_race.h
 */
bool G_Race_AddStage(int32_t stage) {
  return G_Race_AddToSequence(&gLevel.raceCourse.stages, &gLevel.raceCourse.stagesMalformed, stage, 2);
}

/**
 * @see g_race.h
 */
void G_Race_AddFinish(void) {
  gLevel.raceCourse.finishCount++;
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
  GameRaceCourse *course = &gLevel.raceCourse;

  course->valid = G_Race_ValidateSequence(course->checkpoints, course->malformed, 1, &course->checkpointCount) &&
                  course->finishCount > 0;

  course->splitsValid = G_Race_ValidateSequence(course->splits, course->splitsMalformed, 1, &course->splitCount);
  course->stagesValid = G_Race_ValidateSequence(course->stages, course->stagesMalformed, 2, &course->stageCount);
}

// ---------------------------------------------------------------- the run

/**
 * @see g_race.h
 */
bool G_Race_Debounced(GameClient *cl, const GameEntity *ent, float wait) {

  if (cl->raceTrigger == ent && gLevel.time - cl->raceTriggerTime < wait * 1000.f) {
    return true;
  }

  cl->raceTrigger = ent;
  cl->raceTriggerTime = gLevel.time;
  return false;
}

/**
 * @see g_race.h
 */
bool G_Race_Start(GameClient *cl) {

  if (!G_Race_CanRun(cl)) {
    return false;
  }

  if (!gLevel.raceCourse.valid) {
    gi.ClientPrint(cl, PRINT_HIGH, "This level has no valid course: it needs a finish and checkpoints 1 through N\n");
    return false;
  }

  G_Race_Reset(cl);

  GameRaceRun *run = &cl->raceRun;
  const float speed = Vec3_Length(cl->entity->velocity);

  run->state = RACE_RUN_ACTIVE;
  run->mode = G_Race_Mode(cl);
  run->movement = cl->ps.pmState.params.movement;
  run->stage = 1;
  run->startTime = gLevel.time;

  cl->persistent.raceRuns++;
  run->startSpeed = run->topSpeed = speed;

  if (cl->entity->moveType == MOVE_TYPE_NO_CLIP) {
    run->invalid |= RACE_INVALID_NOCLIP;
  }

  G_Race_BeginLine(cl);
  G_Race_SpawnGhost(cl);

  G_Race_CenterPrint(cl, "^2Go!");
  return true;
}

/**
 * @see g_race.h
 */
void G_Race_ArmStart(GameClient *cl, const GameEntity *start) {

  if (cl->raceStart == start) {
    return;
  }

  // entering a start zone abandons whatever run was under way
  if (cl->raceRun.state != RACE_RUN_IDLE) {
    G_Race_Reset(cl);
  }

  cl->raceStart = start;
}

/**
 * @brief How `time` compares to `record` at this milestone, or no answer when
 * the record never got this far.
 */
static int32_t G_Race_Delta(const GameRaceRecord *record, GameRaceMilestone kind, uint16_t number, uint32_t time) {

  if (!record) {
    return RACE_MILESTONE_NO_DELTA;
  }

  uint32_t reference;

  switch (kind) {
    case RACE_MILESTONE_CHECKPOINT:
      if (record->checkpointCount < number) {
        return RACE_MILESTONE_NO_DELTA;
      }
      reference = record->checkpointTimes[number - 1];
      break;
    case RACE_MILESTONE_SPLIT:
      if (record->splitCount < number) {
        return RACE_MILESTONE_NO_DELTA;
      }
      reference = record->splitTimes[number - 1];
      break;
    default:
      if (record->stageCount < number - 1) {
        return RACE_MILESTONE_NO_DELTA;
      }
      reference = record->stageTimes[number - 2];
      break;
  }

  return (int32_t) time - (int32_t) reference;
}

/**
 * @brief Tells the racer what they just passed and how it compares, for the
 * HUD to show: against their own best, and against the course record.
 */
static void G_Race_Milestone(GameClient *cl, GameRaceMilestone kind, uint16_t number, const char *label, uint32_t time) {

  const PMovement movement = cl->raceRun.movement;

  gi.WriteByte(SV_CMD_RACE_MILESTONE);
  gi.WriteByte(kind);
  gi.WriteByte(number);
  gi.WriteString(label ?: "");
  gi.WriteLong(time);
  gi.WriteLong(G_Race_Delta(G_Race_Record(cl->persistent.guid, movement), kind, number, time));
  gi.WriteLong(G_Race_Delta(G_Race_BestRecord(movement), kind, number, time));
  gi.Unicast(cl, true);
}

/**
 * @see g_race.h
 */
bool G_Race_Checkpoint(GameClient *cl, uint16_t checkpoint) {
  GameRaceRun *run = &cl->raceRun;

  if (!G_Race_CanRun(cl) || run->state != RACE_RUN_ACTIVE) {
    return false;
  }

  const uint16_t expected = run->checkpointCount + 1;

  if (checkpoint < expected) { // already reached, so standing in it again means nothing
    return false;
  }

  if (checkpoint > expected) {
    G_Race_CenterPrint(cl, "Checkpoint %u skipped", expected);
    return false;
  }

  run->checkpointTimes[run->checkpointCount++] = gLevel.time - run->startTime;

  G_Race_Milestone(cl, RACE_MILESTONE_CHECKPOINT, checkpoint, NULL, run->checkpointTimes[checkpoint - 1]);

  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.teleport,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

/**
 * @brief A split times the run without judging it: one reached out of order
 * is simply not a split of this run.
 */
bool G_Race_Split(GameClient *cl, uint16_t split, const char *label) {
  GameRaceRun *run = &cl->raceRun;

  if (!G_Race_CanRun(cl) || run->state != RACE_RUN_ACTIVE || !gLevel.raceCourse.splitsValid) {
    return false;
  }

  if (split != run->splitCount + 1) {
    return false;
  }

  const uint32_t time = gLevel.time - run->startTime;

  run->splitTimes[run->splitCount++] = time;

  G_Race_Milestone(cl, RACE_MILESTONE_SPLIT, split, label, time);

  return true;
}

/**
 * @brief A stage is timed as a split is, and is also where a practicing client
 * returns to: reaching one stores its anchor, as `store` would.
 */
bool G_Race_Stage(GameClient *cl, uint16_t stage, const char *label, const GameEntity *anchor) {
  GameRaceRun *run = &cl->raceRun;

  if (!G_Race_CanRun(cl) || !gLevel.raceCourse.stagesValid) {
    return false;
  }

  const char *name = label && *label ? label : va("Stage %u", stage);
  bool counted = false;

  if (run->state == RACE_RUN_ACTIVE && stage == run->stage + 1) {
    run->stageTimes[stage - 2] = gLevel.time - run->startTime;
    run->stage = stage;

    G_Race_Milestone(cl, RACE_MILESTONE_STAGE, stage, label, run->stageTimes[stage - 2]);
    counted = true;
  }

  if (G_Race_Mode(cl) == RACE_MODE_PRACTICE) {
    GameRaceSpawn *spawn = &cl->persistent.raceSpawn;

    if (!spawn->set || !Vec3_Equal(spawn->origin, anchor->s.origin)) {
      *spawn = (GameRaceSpawn) {
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

/**
 * @see g_race.h
 */
bool G_Race_Finish(GameClient *cl) {
  GameRaceRun *run = &cl->raceRun;

  if (!G_Race_CanRun(cl) || run->state != RACE_RUN_ACTIVE) {
    return false;
  }

  const uint16_t remaining = gLevel.raceCourse.checkpointCount - run->checkpointCount;
  if (remaining) {
    gi.ClientPrint(cl, PRINT_HIGH, "%u checkpoint%s remaining\n", remaining, remaining == 1 ? "" : "s");
    return false;
  }

  if (cl->entity->moveType == MOVE_TYPE_NO_CLIP) {
    run->invalid |= RACE_INVALID_NOCLIP;
  }

  if (cl->ps.pmState.params.movement != run->movement) {
    run->invalid |= RACE_INVALID_MOVEMENT;
  }

  run->state = RACE_RUN_FINISHED;
  run->elapsed = gLevel.time - run->startTime;
  run->endSpeed = Vec3_Length(cl->entity->velocity);
  run->topSpeed = Maxf(run->topSpeed, run->endSpeed);

  const char *time = G_Race_FormatTime(run->elapsed);

  // it counts only if it was raced from start to finish with nothing to disqualify it
  if (run->mode == RACE_MODE_RACE && G_Race_Mode(cl) == RACE_MODE_RACE && !run->invalid) {
    gi.BroadcastPrint(PRINT_HIGH, "%s finished in %s\n", cl->persistent.netName, time);
    if (G_Race_SubmitRecord(cl)) {
      G_Race_KeepLine(cl);
    }
  } else if (run->mode == RACE_MODE_PRACTICE) {
    G_Race_CenterPrint(cl, "Finished in %s, practicing", time);
  } else {
    G_Race_CenterPrint(cl, "Finished in %s, but ^1it does not count^7", time);
  }

  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.teleport,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

// ---------------------------------------------------------------- the modes

/**
 * @brief Moves the client to `mode`, resetting whatever the old mode held.
 */
static void G_Race_SetMode(GameClient *cl, GameRaceMode mode) {

  if (G_Race_Mode(cl) == mode) {
    gi.ClientPrint(cl, PRINT_HIGH, "You are already %s\n", G_Race_ModeName(mode));
    return;
  }

  if (gLevel.time - cl->respawnTime < 1000) { // as spectate and join are
    return;
  }

  if (mode == RACE_MODE_SPECTATOR) {
    G_TossInventory(cl); // which resets the run
    cl->persistent.spectator = true;
    cl->persistent.raceSpawn.set = false;
  } else {
    G_Race_Reset(cl);
    cl->persistent.spectator = false;
    cl->persistent.raceMode = mode;
  }

  gi.BroadcastPrint(PRINT_HIGH, "%s is %s\n", cl->persistent.netName, G_Race_ModeName(mode));

  if (mode == RACE_MODE_PRACTICE) {
    gi.ClientPrint(cl, PRINT_HIGH, "Nothing counts while practicing. ^2store^7 remembers where you are; ^2kill^7 takes you back\n");
  }

  G_ClientRespawn(cl, false);
}

/**
 * @brief `mode <race|practice|spectator>`, or with no argument, says which.
 */
static void G_Race_Mode_f(GameClient *cl) {

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
static void G_Race_Store_f(GameClient *cl) {

  if (G_Race_Mode(cl) != RACE_MODE_PRACTICE) {
    gi.ClientPrint(cl, PRINT_HIGH, "Only while practicing\n");
    return;
  }

  if (!G_Race_CanRun(cl)) {
    return;
  }

  // in the spawn point's terms, so that respawning here puts the feet back
  // exactly where they were
  Vec3 origin = cl->entity->s.origin;
  origin.z -= PM_STEP_HEIGHT;

  cl->persistent.raceSpawn = (GameRaceSpawn) {
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
static void G_Race_Kill_f(GameClient *cl) {

  if (cl->persistent.spectator || !cl->entity || cl->entity->dead) {
    return;
  }

  if (gLevel.time - cl->respawnTime < RACE_KILL_INTERVAL) {
    return;
  }

  G_Race_Reset(cl);
  G_ClientRespawn(cl, false);
}

/**
 * @brief Practicing allows noclip regardless of cheats; racing refuses it under
 * the usual rule, and any run it touches does not count.
 */
static void G_Race_NoClip_f(GameClient *cl) {

  if (cl->persistent.spectator || !cl->entity) {
    return;
  }

  const bool practicing = G_Race_Mode(cl) == RACE_MODE_PRACTICE;
  const bool cheating = sv_maxClients->integer <= 1 || g_cheats->value;

  if (!practicing && !cheating) {
    gi.ClientPrint(cl, PRINT_HIGH, "Cheats are disabled\n");
    return;
  }

  if (cl->entity->moveType == MOVE_TYPE_NO_CLIP) {
    cl->entity->moveType = MOVE_TYPE_WALK;
    gi.ClientPrint(cl, PRINT_HIGH, "noClip disabled\n");
  } else {
    cl->entity->moveType = MOVE_TYPE_NO_CLIP;
    cl->raceRun.invalid |= RACE_INVALID_NOCLIP;
    gi.ClientPrint(cl, PRINT_HIGH, "noClip enabled\n");
  }
}

/**
 * @brief `race`: the course, the client's run and their best, in the console.
 */
static void G_Race_Status_f(GameClient *cl) {
  const GameRaceRun *run = &cl->raceRun;
  const GameRaceCourse *course = &gLevel.raceCourse;

  gi.ClientPrint(cl, PRINT_HIGH, "Course: %u checkpoint%s, %u start%s, %u finish%s%s\n",
                 course->checkpointCount, course->checkpointCount == 1 ? "" : "s",
                 course->startCount, course->startCount == 1 ? "" : "s",
                 course->finishCount, course->finishCount == 1 ? "" : "es",
                 course->valid ? "" : " ^1(invalid)^7");

  const GameRaceRecord *record = G_Race_Record(cl->persistent.guid, gLevel.movement);
  if (record) {
    size_t count;
    const size_t rank = G_Race_Rank(record, &count);
    gi.ClientPrint(cl, PRINT_HIGH, "Best: %s, #%zu of %zu\n", G_Race_FormatTime(record->time), rank, count);
  }

  switch (run->state) {
    case RACE_RUN_ACTIVE:
      gi.ClientPrint(cl, PRINT_HIGH, "Running: %s, checkpoint %u of %u%s\n",
                     G_Race_FormatTime(gLevel.time - run->startTime),
                     run->checkpointCount, course->checkpointCount,
                     course->stagesValid && course->stageCount ? va(", stage %u of %u", run->stage, course->stageCount) : "");
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
static void G_LevelWillSpawn_Race(void) {

  G_ForEachClient(cl, {
    G_Race_Reset(cl);
  });

  previous.LevelWillSpawn();
}

/**
 * @brief The course is validated and published, the records and the raceline loaded, and every client reset for the new level.
 */
static void G_ConfigureLevel_Race(void) {

  previous.ConfigureLevel();

  G_Race_ValidateCourse();
  G_Race_ResolveStages();
  G_Race_LoadRecords();
  G_Race_LoadLine();

  const GameRaceCourse *course = &gLevel.raceCourse;

  gi.SetConfigString(CS_RACE_COURSE, va("%u\\%u\\%u", course->checkpointCount,
                                        course->finishCount, course->valid));

  if (!course->valid) {
    G_Warn("%s has no valid course: it needs a finish and checkpoints 1 through N\n", gLevel.name);
  }

  // the course is new, and so is the ground a stored position stood on
  G_ForEachClient(cl, {
    G_Race_Reset(cl);
    cl->persistent.raceSpawn.set = false;
    cl->persistent.raceRuns = 0;
  });
}

/**
 * @brief The race classes first, then whatever common knows.
 */
static bool G_InitEntity_Race(GameEntity *ent) {

  if (G_Race_InitEntity(ent)) {
    return true;
  }

  return previous.InitEntity(ent);
}

/**
 * @brief The barriers first, then whatever previous says.
 */
static bool G_ClipEntity_Race(const GameEntity *mover, const GameEntity *ent) {

  if (!G_Race_ClipEntity(mover, ent)) {
    return false;
  }

  return previous.ClipEntity ? previous.ClipEntity(mover, ent) : true;
}

/**
 * @brief Players pass through each other and telefrag nobody, and a practicing
 * client who has stored a position spawns there.
 */
static void G_PrepareSpawn_Race(GameClient *cl, GameClientSpawn *spawn) {

  spawn->clipMask &= ~CONTENTS_MONSTER;
  spawn->killBox = false;

  if (G_Race_Mode(cl) == RACE_MODE_PRACTICE && cl->persistent.raceSpawn.set) {
    spawn->origin = cl->persistent.raceSpawn.origin;
    spawn->angles = cl->persistent.raceSpawn.angles;
  }

  previous.PrepareSpawn(cl, spawn);
}

/**
 * @brief A death ends the run; the corpse tosses as usual.
 */
static void G_TossInventory_Race(GameClient *cl) {

  G_Race_Reset(cl);

  previous.TossInventory(cl);
}

/**
 * @brief Nobody is hurt. An attack from another player does nothing at all;
 * anything else keeps its knockback and loses its damage, which is what a
 * rocket jump needs and a lava pit does not get.
 */
static bool G_ModifyDamage_Race(GameEntity *target, GameEntity *attacker, int32_t *damage, int32_t *knockback) {

  if (!target->client) {
    return previous.ModifyDamage(target, attacker, damage, knockback);
  }

  if (attacker->client && attacker != target) {
    return false;
  }

  if (!previous.ModifyDamage(target, attacker, damage, knockback)) {
    return false;
  }

  *damage = 0;
  return true;
}

/**
 * @brief The grapple is practice equipment: never under a run.
 */
static bool G_AllowHook_Race(const GameClient *cl) {

  if (G_Race_Mode(cl) != RACE_MODE_PRACTICE) {
    return false;
  }

  return previous.AllowHook(cl);
}

/**
 * @brief The race commands, deferring the rest.
 */
static bool G_HandleClientCommand_Race(GameClient *cl, const char *cmd) {

  if (gLevel.intermissionTime) {
    return previous.HandleClientCommand(cl, cmd);
  }

  if (!q_strcmp(cmd, "race")) {
    G_Race_Status_f(cl);
  } else if (!q_strcmp(cmd, "mode")) {
    G_Race_Mode_f(cl);
  } else if (!q_strcmp(cmd, "store")) {
    G_Race_Store_f(cl);
  } else if (!q_strcmp(cmd, "kill")) {
    G_Race_Kill_f(cl);
  } else if (!q_strcmp(cmd, "noClip")) {
    G_Race_NoClip_f(cl);
  } else if (!q_strcmp(cmd, "ghost")) {
    G_Race_Ghost_f(cl);
  } else {
    return previous.HandleClientCommand(cl, cmd);
  }

  return true;
}

/**
 * @brief Whether `cl` is standing in `ent`. By bounds, because that is what a
 * trigger's touch is: the server never traces against a SOLID_TRIGGER, so a
 * clip against one reports nothing however deep inside it the box is.
 */
static bool G_Race_Inside(const GameClient *cl, const GameEntity *ent) {

  return ent->inUse && Box3_Intersects(cl->entity->absBounds, ent->absBounds);
}

/**
 * @brief The starts that begin on input: a jump out of a jump-mode zone, or the
 * first movement when the course has no start zone at all. `cl->cmd` is still
 * the previous command here, which is what makes the jump an edge.
 */
static void G_ClientWillThink_Race(GameClient *cl, const PMoveCmd *cmd) {

  previous.ClientWillThink(cl, cmd);

  if (!G_Race_CanRun(cl)) {
    return;
  }

  if (cl->raceStart) {
    if (cl->raceStart->count == RACE_START_JUMP && cmd->up > 0 && cl->cmd.up <= 0) {
      const GameEntity *start = cl->raceStart;
      cl->raceStart = NULL;

      if (G_Race_Inside(cl, start) && G_Race_Start(cl)) {
        G_UseTargets((GameEntity *) start, cl->entity);
      }
    }
    return;
  }

  if (!gLevel.raceCourse.startCount && cl->raceRun.state == RACE_RUN_IDLE &&
      (cmd->forward || cmd->right || cmd->up)) {
    G_Race_Start(cl);
  }
}

/**
 * @brief Every client is told which barriers pass them, every frame.
 */
static void G_FrameDidEnd_Race(void) {

  previous.FrameDidEnd();

  G_ForEachClient(cl, {
    if (cl->entity) {
      G_Race_UpdateBarriers(cl);
    }
  });
}

/**
 * @brief Samples the speed for the finish report, and starts the run for a
 * client who has just left an exit-mode start zone.
 */
static void G_ClientDidMove_Race(GameClient *cl, const PMoveCmd *cmd) {

  previous.ClientDidMove(cl, cmd);

  if (!G_Race_CanRun(cl)) {
    return;
  }

  GameRaceRun *run = &cl->raceRun;

  if (run->state == RACE_RUN_ACTIVE) {
    const float speed = Vec3_Length(cl->entity->velocity);

    run->topSpeed = Maxf(run->topSpeed, speed);
    run->speedSum += speed;
    run->speedSamples++;

    G_Race_SampleLine(cl);
  }

  if (cl->raceStart && !G_Race_Inside(cl, cl->raceStart)) {
    const GameEntity *start = cl->raceStart;
    cl->raceStart = NULL;

    if (start->count == RACE_START_EXIT && G_Race_Start(cl)) {
      G_UseTargets((GameEntity *) start, cl->entity);
    }
  }
}

/**
 * @brief A leaving client's run ends with them.
 */
static void G_ClientWillDisconnect_Race(GameClient *cl) {

  G_Race_Reset(cl);

  previous.ClientWillDisconnect(cl);
}

/**
 * @brief The race stats: mode, run state, time, checkpoints, flags and runs.
 */
static void G_WriteStats_Race(GameClient *cl) {

  previous.WriteStats(cl);

  const GameRaceRun *run = &cl->raceRun;

  uint32_t time = 0;
  if (run->state == RACE_RUN_ACTIVE) {
    time = gLevel.time - run->startTime;
  } else if (run->state == RACE_RUN_FINISHED) {
    time = run->elapsed;
  }

  cl->ps.stats[STAT_RACE_MODE] = G_Race_Mode(cl);
  cl->ps.stats[STAT_RACE_RUN] = run->state;
  cl->ps.stats[STAT_RACE_TIME_LOW] = (int16_t) (uint16_t) time;
  cl->ps.stats[STAT_RACE_TIME_HIGH] = (int16_t) (uint16_t) (time >> 16);
  cl->ps.stats[STAT_RACE_CHECKPOINTS] = run->checkpointCount;
  cl->ps.stats[STAT_RACE_FLAGS] = run->invalid;
  cl->ps.stats[STAT_RACE_RUNS] = cl->persistent.raceRuns;
}

/**
 * @brief The racer's standing on the board: their best, their mode and their
 * runs, and a score that sorts them by rank, since the common board sorts by
 * score before the race board ever sees the rows.
 */
static void G_WriteScore_Race(const GameClient *cl, GameScore *s) {

  previous.WriteScore(cl, s);

  s->raceMode = G_Race_Mode(cl);
  s->raceRuns = cl->persistent.raceRuns;

  const GameRaceRecord *record = G_Race_Record(cl->persistent.guid, gLevel.movement);
  if (record) {
    size_t count;
    s->raceBest = record->time;
    s->score = -(int16_t) G_Race_Rank(record, &count);
  } else {
    s->raceBest = 0;
    s->score = -9998; // beneath any rank, above the -9999 the board sorts spectators to
  }
}

/**
 * @see g_race.h
 */
void G_Race_Init(void) {

  if (installed) {
    return;
  }

  installed = true;

  previous.LevelWillSpawn = G_LevelWillSpawn;
  G_LevelWillSpawn = G_LevelWillSpawn_Race;

  previous.ConfigureLevel = G_ConfigureLevel;
  G_ConfigureLevel = G_ConfigureLevel_Race;

  previous.InitEntity = G_InitEntity;
  G_InitEntity = G_InitEntity_Race;

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

  previous.FrameDidEnd = G_FrameDidEnd;
  G_FrameDidEnd = G_FrameDidEnd_Race;

  previous.ClientWillDisconnect = G_ClientWillDisconnect;
  G_ClientWillDisconnect = G_ClientWillDisconnect_Race;

  previous.WriteStats = G_WriteStats;
  G_WriteStats = G_WriteStats_Race;

  previous.WriteScore = G_WriteScore;
  G_WriteScore = G_WriteScore_Race;
}
