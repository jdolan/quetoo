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

#include "tests.h"

#include "collision/cm_entity.h"
#include "game/race/g_race.h"

/**
 * @file
 * @brief The `.rec` and `.ghost` file formats, round tripped through the real
 * filesystem and entity parser: no server, no client, no module load. `gi`'s
 * file and entity fields point at the same functions `sv_game.c` binds them to
 * in production; everything else on it is a no-op stub, since nothing here
 * touches messaging or the wire. `G_Race_CenterPrint` and `G_Race_Mode`, the
 * two non-file functions `g_race_records.c` and `g_race_replay.c` reach outside
 * themselves for, are reimplemented here rather than pulling in `g_race.c` and
 * the rest of the game module behind it.
 */

Quetoo quetoo;
GameImport gi;
GameLevel gLevel;

static GameRaceMode testRaceMode = RACE_MODE_RACE;

/**
 * @see g_race.h
 */
void G_Race_CenterPrint(const GameClient *cl, const char *fmt, ...) {
}

/**
 * @see g_race.h
 */
GameRaceMode G_Race_Mode(const GameClient *cl) {
  return testRaceMode;
}

/**
 * @brief The ghost entity, and only it, needs these; nothing here spawns one.
 */
GameEntity *G_AllocEntity(const char *classname) {
  return Mem_TagMalloc(sizeof(GameEntity), MEM_TAG_GAME_LEVEL);
}

void G_FreeEntity(GameEntity *ent) {
  Mem_Free(ent);
}

#define TEST_BSP_HASH "deadbeef"

static char testCsRaceRecords[MAX_STRING_CHARS];
static char testCsRaceGhost[MAX_STRING_CHARS];

static void Test_SetConfigString(const int32_t index, const char *string) {

  if (index == CS_RACE_RECORDS) {
    q_strlcpy(testCsRaceRecords, string, sizeof(testCsRaceRecords));
  } else if (index == CS_RACE_GHOST) {
    q_strlcpy(testCsRaceGhost, string, sizeof(testCsRaceGhost));
  }
}

static const char *Test_GetConfigString(const int32_t index) {

  if (index == CS_BSP_HASH) {
    return TEST_BSP_HASH;
  }

  if (index == CS_RACE_RECORDS) {
    return testCsRaceRecords;
  }

  if (index == CS_RACE_GHOST) {
    return testCsRaceGhost;
  }

  if (index >= CS_CLIENTS) {
    return "newbie\\enforcer/default";
  }

  return "";
}

static void Test_BroadcastPrint(const int32_t level, const char *fmt, ...) {
}

static void Test_ClientPrint(const GameClient *cl, const int32_t level, const char *fmt, ...) {
}

static void Test_Debug(const DebugFlags debug, const char *func, const char *fmt, ...) {
}

static void Test_Warn(const char *func, const char *fmt, ...) {
}

/**
 * @brief A client with just enough set to submit and reload a race run.
 */
static GameEntity testEntity;
static GameClient testClient;

/**
 * @brief Setup fixture.
 */
void setup(void) {

  Mem_Init();

  Fs_Init(FS_NONE);

  ck_assert(Fs_SetGame(TEST_GAME, NULL));

  memset(&gi, 0, sizeof(gi));

  gi.Malloc = Mem_TagMalloc;
  gi.Free = Mem_Free;

  gi.OpenFileWrite = Fs_OpenWrite;
  gi.WriteFile = Fs_Write;
  gi.CloseFile = Fs_Close;
  gi.LoadFile = Fs_Load;
  gi.FreeFile = Fs_Free;

  gi.EntityValue = Cm_EntityValue;
  gi.LoadEntities = Cm_LoadEntities;
  gi.FreeEntity = Cm_FreeEntity;

  gi.SetConfigString = Test_SetConfigString;
  gi.GetConfigString = Test_GetConfigString;
  gi.BroadcastPrint = Test_BroadcastPrint;
  gi.ClientPrint = Test_ClientPrint;
  gi.Debug = Test_Debug;
  gi.Warn = Test_Warn;

  memset(&gLevel, 0, sizeof(gLevel));
  q_strlcpy(gLevel.name, "checkrace", sizeof(gLevel.name));
  gLevel.movement = PM_MOVEMENT_RACE;

  memset(testCsRaceRecords, 0, sizeof(testCsRaceRecords));
  memset(testCsRaceGhost, 0, sizeof(testCsRaceGhost));

  memset(&testEntity, 0, sizeof(testEntity));
  memset(&testClient, 0, sizeof(testClient));

  testClient.entity = &testEntity;
  testRaceMode = RACE_MODE_RACE;
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  gi.Free(gLevel.raceRecords);
  gi.Free(gLevel.raceLine.samples);

  Fs_Shutdown();

  Mem_Shutdown();
}

/**
 * @brief Fills in a run as though the client just finished the course.
 */
static void Test_FinishRun(GameClient *cl, const char *guid, const char *name, uint32_t elapsed) {

  GameRaceRun *run = &cl->raceRun;
  memset(run, 0, sizeof(*run));

  q_strlcpy(cl->persistent.guid, guid, sizeof(cl->persistent.guid));
  q_strlcpy(cl->persistent.netName, name, sizeof(cl->persistent.netName));

  // a real run has real params behind it, not the zeroed struct a fresh fixture starts with
  cl->ps.pmState.params = *Pm_Movement(PM_MOVEMENT_RACE)->params;

  run->movement = PM_MOVEMENT_RACE;
  run->elapsed = elapsed;

  run->checkpointCount = 2;
  run->checkpointTimes[0] = elapsed / 3;
  run->checkpointTimes[1] = 2 * elapsed / 3;

  run->splitCount = 1;
  run->splitTimes[0] = elapsed / 2;

  run->stage = 1; // never left stage 1, so no stage times are recorded

  run->startSpeed = 320.f;
  run->topSpeed = 900.f;
  run->speedSum = 640.f * 10.f;
  run->speedSamples = 10;
}

START_TEST(check_G_Race_Records_RoundTrip) {

  Test_FinishRun(&testClient, "guid-alice", "Alice", 83412);

  ck_assert(G_Race_SubmitRecord(&testClient));
  ck_assert_uint_eq(gLevel.raceRecordCount, 1);

  const uint32_t params = G_Race_Record("guid-alice", PM_MOVEMENT_RACE)->params;
  ck_assert_uint_ne(params, 0); // the fixture's params are not the zeroed struct's hash

  // forget everything in memory, and what was published, and reload from what was just written
  gi.Free(gLevel.raceRecords);
  memset(&gLevel.raceRecords, 0, sizeof(gLevel.raceRecords));
  gLevel.raceRecordCount = gLevel.raceRecordCapacity = 0;
  memset(testCsRaceRecords, 0, sizeof(testCsRaceRecords));

  G_Race_LoadRecords();

  ck_assert_uint_eq(gLevel.raceRecordCount, 1);

  const GameRaceRecord *record = G_Race_Record("guid-alice", PM_MOVEMENT_RACE);
  ck_assert_ptr_nonnull(record);
  ck_assert_str_eq(record->name, "Alice");
  ck_assert_uint_eq(record->time, 83412);
  ck_assert_uint_eq(record->params, params);
  ck_assert_uint_eq(record->checkpointCount, 2);
  ck_assert_uint_eq(record->checkpointTimes[0], 83412 / 3);
  ck_assert_uint_eq(record->checkpointTimes[1], 2 * 83412 / 3);
  ck_assert_uint_eq(record->splitCount, 1);
  ck_assert_uint_eq(record->splitTimes[0], 83412 / 2);
  ck_assert_uint_eq(record->stageCount, 0);
  ck_assert_float_eq_tol(record->startSpeed, 320.f, 1.f);
  ck_assert_float_eq_tol(record->topSpeed, 900.f, 1.f);
  ck_assert_float_eq_tol(record->averageSpeed, 640.f, 1.f);

  ck_assert(q_strlen(testCsRaceRecords) > 0);

} END_TEST

START_TEST(check_G_Race_Records_KeepsBestOnly) {

  Test_FinishRun(&testClient, "guid-bob", "Bob", 90000);
  ck_assert(G_Race_SubmitRecord(&testClient)); // first ever, so it's a course record

  Test_FinishRun(&testClient, "guid-bob", "Bob", 95000);
  ck_assert(!G_Race_SubmitRecord(&testClient)); // slower than the standing best

  const GameRaceRecord *record = G_Race_Record("guid-bob", PM_MOVEMENT_RACE);
  ck_assert_ptr_nonnull(record);
  ck_assert_uint_eq(record->time, 90000);

  Test_FinishRun(&testClient, "guid-bob", "Bob", 80000);
  ck_assert(G_Race_SubmitRecord(&testClient)); // faster, and still the only racer

  record = G_Race_Record("guid-bob", PM_MOVEMENT_RACE);
  ck_assert_ptr_nonnull(record);
  ck_assert_uint_eq(record->time, 80000);
  ck_assert_uint_eq(gLevel.raceRecordCount, 1); // one record per client, not one per run

} END_TEST

START_TEST(check_G_Race_Records_MalformedSkipped) {

  File *file = gi.OpenFileWrite("records/checkrace.rec");
  ck_assert_ptr_nonnull(file);

  static const char *malformed =
    "{\n"
    "  \"guid\" \"guid-nobody\"\n" // no "time", so this block cannot be a record
    "  \"name\" \"Nobody\"\n"
    "  \"movement\" \"race\"\n"
    "}\n";

  gi.WriteFile(file, malformed, 1, q_strlen(malformed));
  gi.CloseFile(file);

  G_Race_LoadRecords();

  ck_assert_uint_eq(gLevel.raceRecordCount, 0);
  ck_assert_ptr_null(G_Race_Record("guid-nobody", PM_MOVEMENT_RACE));

} END_TEST

START_TEST(check_G_Race_Line_RoundTrip) {

  G_Race_BeginLine(&testClient);

  for (uint32_t time = 0; time <= 200; time += 100) {
    gLevel.time = time;
    testEntity.s.origin = MakeVec3((float) time, (float) time * 2.f, 0.f);
    testEntity.s.angles = MakeVec3(0.f, (float) time, 0.f);
    testEntity.s.animation1 = 1;
    testEntity.s.animation2 = 2;
    G_Race_SampleLine(&testClient);
  }

  Test_FinishRun(&testClient, "guid-carol", "Carol", 200);

  G_Race_KeepLine(&testClient);

  // G_Race_KeepLine reloaded gLevel.raceLine from disk, since the movement matches
  ck_assert_uint_eq(gLevel.raceLine.count, 3);
  ck_assert_str_eq(gLevel.raceLineHolder, "Carol");
  ck_assert_uint_eq(gLevel.raceLineTime, 200);

  for (size_t i = 0; i < gLevel.raceLine.count; i++) {
    const GameRaceSample *sample = &gLevel.raceLine.samples[i];
    const uint32_t expectedTime = (uint32_t) i * 100;

    ck_assert_uint_eq(sample->time, expectedTime);
    ck_assert_float_eq_tol(sample->origin.x, (float) expectedTime, .01f);
    ck_assert_float_eq_tol(sample->origin.y, (float) expectedTime * 2.f, .01f);
    ck_assert_uint_eq(sample->animation1, 1);
    ck_assert_uint_eq(sample->animation2, 2);
  }

  ck_assert(q_strlen(testCsRaceGhost) > 0);

} END_TEST

START_TEST(check_G_Race_Line_BspMismatchRejected) {

  G_Race_BeginLine(&testClient);

  gLevel.time = 0;
  testEntity.s.origin = Vec3_Zero();
  G_Race_SampleLine(&testClient);

  gLevel.time = 100;
  testEntity.s.origin = MakeVec3(10.f, 0.f, 0.f);
  G_Race_SampleLine(&testClient);

  Test_FinishRun(&testClient, "guid-dan", "Dan", 100);
  G_Race_KeepLine(&testClient);

  ck_assert_uint_gt(gLevel.raceLine.count, 0);

  // a rebuild of the map changes the hash the ghost was tied to
  File *file = gi.OpenFileWrite("records/checkrace-race.ghost");
  ck_assert_ptr_nonnull(file);

  static const char *rebuilt =
    "holder Dan\n"
    "guid guid-dan\n"
    "client newbie\\enforcer/default\n"
    "time 100\n"
    "bsp not-the-bsp-this-was-set-on\n"
    "samples 2\n"
    "0 0.00 0.00 0.00 0.0 0.0 0.0 0 0\n"
    "100 10.00 0.00 0.00 0.0 0.0 0.0 0 0\n";

  gi.WriteFile(file, rebuilt, 1, q_strlen(rebuilt));
  gi.CloseFile(file);

  G_Race_LoadLine();

  ck_assert_uint_eq(gLevel.raceLine.count, 0);
  ck_assert_uint_eq(gLevel.raceLineTime, 0);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_race");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_G_Race_Records_RoundTrip);
  tcase_add_test(tcase, check_G_Race_Records_KeepsBestOnly);
  tcase_add_test(tcase, check_G_Race_Records_MalformedSkipped);
  tcase_add_test(tcase, check_G_Race_Line_RoundTrip);
  tcase_add_test(tcase, check_G_Race_Line_BspMismatchRejected);

  Suite *suite = suite_create("check_race");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
