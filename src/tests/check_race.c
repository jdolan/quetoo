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

quetoo_t quetoo;
g_import_t gi;
g_level_t g_level;

static g_race_mode_t test_race_mode = RACE_MODE_RACE;

/**
 * @see g_race.h
 */
void G_Race_CenterPrint(const g_client_t *cl, const char *fmt, ...) {
}

/**
 * @see g_race.h
 */
g_race_mode_t G_Race_Mode(const g_client_t *cl) {
  return test_race_mode;
}

/**
 * @brief The ghost entity, and only it, needs these; nothing here spawns one.
 */
g_entity_t *G_AllocEntity(const char *classname) {
  return Mem_TagMalloc(sizeof(g_entity_t), MEM_TAG_GAME_LEVEL);
}

void G_FreeEntity(g_entity_t *ent) {
  Mem_Free(ent);
}

#define TEST_BSP_HASH "deadbeef"

static char test_cs_race_records[MAX_STRING_CHARS];
static char test_cs_race_ghost[MAX_STRING_CHARS];

static void Test_SetConfigString(const int32_t index, const char *string) {

  if (index == CS_RACE_RECORDS) {
    q_strlcpy(test_cs_race_records, string, sizeof(test_cs_race_records));
  } else if (index == CS_RACE_GHOST) {
    q_strlcpy(test_cs_race_ghost, string, sizeof(test_cs_race_ghost));
  }
}

static const char *Test_GetConfigString(const int32_t index) {

  if (index == CS_BSP_HASH) {
    return TEST_BSP_HASH;
  }

  if (index == CS_RACE_RECORDS) {
    return test_cs_race_records;
  }

  if (index == CS_RACE_GHOST) {
    return test_cs_race_ghost;
  }

  if (index >= CS_CLIENTS) {
    return "newbie\\qforcer/default";
  }

  return "";
}

static void Test_BroadcastPrint(const int32_t level, const char *fmt, ...) {
}

static void Test_ClientPrint(const g_client_t *cl, const int32_t level, const char *fmt, ...) {
}

static void Test_Debug(const debug_t debug, const char *func, const char *fmt, ...) {
}

static void Test_Warn(const char *func, const char *fmt, ...) {
}

/**
 * @brief A client with just enough set to submit and reload a race run.
 */
static g_entity_t test_entity;
static g_client_t test_client;

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

  memset(&g_level, 0, sizeof(g_level));
  q_strlcpy(g_level.name, "checkrace", sizeof(g_level.name));
  g_level.movement = PM_MOVEMENT_RACE;

  memset(test_cs_race_records, 0, sizeof(test_cs_race_records));
  memset(test_cs_race_ghost, 0, sizeof(test_cs_race_ghost));

  memset(&test_entity, 0, sizeof(test_entity));
  memset(&test_client, 0, sizeof(test_client));

  test_client.entity = &test_entity;
  test_race_mode = RACE_MODE_RACE;
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  gi.Free(g_level.race_records);
  gi.Free(g_level.race_line.samples);

  Fs_Shutdown();

  Mem_Shutdown();
}

/**
 * @brief Fills in a run as though the client just finished the course.
 */
static void Test_FinishRun(g_client_t *cl, const char *guid, const char *name, uint32_t elapsed) {

  g_race_run_t *run = &cl->race_run;
  memset(run, 0, sizeof(*run));

  q_strlcpy(cl->persistent.guid, guid, sizeof(cl->persistent.guid));
  q_strlcpy(cl->persistent.net_name, name, sizeof(cl->persistent.net_name));

  run->movement = PM_MOVEMENT_RACE;
  run->elapsed = elapsed;

  run->checkpoint_count = 2;
  run->checkpoint_times[0] = elapsed / 3;
  run->checkpoint_times[1] = 2 * elapsed / 3;

  run->split_count = 1;
  run->split_times[0] = elapsed / 2;

  run->stage = 1; // never left stage 1, so no stage times are recorded

  run->start_speed = 320.f;
  run->top_speed = 900.f;
  run->speed_sum = 640.f * 10.f;
  run->speed_samples = 10;
}

START_TEST(check_G_Race_Records_RoundTrip) {

  Test_FinishRun(&test_client, "guid-alice", "Alice", 83412);

  ck_assert(G_Race_SubmitRecord(&test_client));
  ck_assert_uint_eq(g_level.race_record_count, 1);

  // forget everything in memory and reload from what was just written
  gi.Free(g_level.race_records);
  memset(&g_level.race_records, 0, sizeof(g_level.race_records));
  g_level.race_record_count = g_level.race_record_capacity = 0;

  G_Race_LoadRecords();

  ck_assert_uint_eq(g_level.race_record_count, 1);

  const g_race_record_t *record = G_Race_Record("guid-alice", PM_MOVEMENT_RACE);
  ck_assert_ptr_nonnull(record);
  ck_assert_str_eq(record->name, "Alice");
  ck_assert_uint_eq(record->time, 83412);
  ck_assert_uint_eq(record->checkpoint_count, 2);
  ck_assert_uint_eq(record->checkpoint_times[0], 83412 / 3);
  ck_assert_uint_eq(record->checkpoint_times[1], 2 * 83412 / 3);
  ck_assert_uint_eq(record->split_count, 1);
  ck_assert_uint_eq(record->split_times[0], 83412 / 2);
  ck_assert_uint_eq(record->stage_count, 0);
  ck_assert_float_eq_tol(record->start_speed, 320.f, 1.f);
  ck_assert_float_eq_tol(record->top_speed, 900.f, 1.f);
  ck_assert_float_eq_tol(record->average_speed, 640.f, 1.f);

  ck_assert(q_strlen(test_cs_race_records) > 0);

} END_TEST

START_TEST(check_G_Race_Records_KeepsBestOnly) {

  Test_FinishRun(&test_client, "guid-bob", "Bob", 90000);
  ck_assert(G_Race_SubmitRecord(&test_client)); // first ever, so it's a course record

  Test_FinishRun(&test_client, "guid-bob", "Bob", 95000);
  ck_assert(!G_Race_SubmitRecord(&test_client)); // slower than the standing best

  const g_race_record_t *record = G_Race_Record("guid-bob", PM_MOVEMENT_RACE);
  ck_assert_ptr_nonnull(record);
  ck_assert_uint_eq(record->time, 90000);

  Test_FinishRun(&test_client, "guid-bob", "Bob", 80000);
  ck_assert(G_Race_SubmitRecord(&test_client)); // faster, and still the only racer

  record = G_Race_Record("guid-bob", PM_MOVEMENT_RACE);
  ck_assert_ptr_nonnull(record);
  ck_assert_uint_eq(record->time, 80000);
  ck_assert_uint_eq(g_level.race_record_count, 1); // one record per client, not one per run

} END_TEST

START_TEST(check_G_Race_Records_MalformedSkipped) {

  file_t *file = gi.OpenFileWrite("records/checkrace.rec");
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

  ck_assert_uint_eq(g_level.race_record_count, 0);
  ck_assert_ptr_null(G_Race_Record("guid-nobody", PM_MOVEMENT_RACE));

} END_TEST

START_TEST(check_G_Race_Line_RoundTrip) {

  G_Race_BeginLine(&test_client);

  for (uint32_t time = 0; time <= 200; time += 100) {
    g_level.time = time;
    test_entity.s.origin = Vec3((float) time, (float) time * 2.f, 0.f);
    test_entity.s.angles = Vec3(0.f, (float) time, 0.f);
    test_entity.s.animation1 = 1;
    test_entity.s.animation2 = 2;
    G_Race_SampleLine(&test_client);
  }

  Test_FinishRun(&test_client, "guid-carol", "Carol", 200);

  G_Race_KeepLine(&test_client);

  // G_Race_KeepLine reloaded g_level.race_line from disk, since the movement matches
  ck_assert_uint_eq(g_level.race_line.count, 3);
  ck_assert_str_eq(g_level.race_line_holder, "Carol");
  ck_assert_uint_eq(g_level.race_line_time, 200);

  for (size_t i = 0; i < g_level.race_line.count; i++) {
    const g_race_sample_t *sample = &g_level.race_line.samples[i];
    const uint32_t expected_time = (uint32_t) i * 100;

    ck_assert_uint_eq(sample->time, expected_time);
    ck_assert_float_eq_tol(sample->origin.x, (float) expected_time, .01f);
    ck_assert_float_eq_tol(sample->origin.y, (float) expected_time * 2.f, .01f);
    ck_assert_uint_eq(sample->animation1, 1);
    ck_assert_uint_eq(sample->animation2, 2);
  }

  ck_assert(q_strlen(test_cs_race_ghost) > 0);

} END_TEST

START_TEST(check_G_Race_Line_BspMismatchRejected) {

  G_Race_BeginLine(&test_client);

  g_level.time = 0;
  test_entity.s.origin = Vec3_Zero();
  G_Race_SampleLine(&test_client);

  g_level.time = 100;
  test_entity.s.origin = Vec3(10.f, 0.f, 0.f);
  G_Race_SampleLine(&test_client);

  Test_FinishRun(&test_client, "guid-dan", "Dan", 100);
  G_Race_KeepLine(&test_client);

  ck_assert_uint_gt(g_level.race_line.count, 0);

  // a rebuild of the map changes the hash the ghost was tied to
  file_t *file = gi.OpenFileWrite("records/checkrace-race.ghost");
  ck_assert_ptr_nonnull(file);

  static const char *rebuilt =
    "holder Dan\n"
    "guid guid-dan\n"
    "client newbie\\qforcer/default\n"
    "time 100\n"
    "bsp not-the-bsp-this-was-set-on\n"
    "samples 2\n"
    "0 0.00 0.00 0.00 0.0 0.0 0.0 0 0\n"
    "100 10.00 0.00 0.00 0.0 0.0 0.0 0 0\n";

  gi.WriteFile(file, rebuilt, 1, q_strlen(rebuilt));
  gi.CloseFile(file);

  G_Race_LoadLine();

  ck_assert_uint_eq(g_level.race_line.count, 0);
  ck_assert_uint_eq(g_level.race_line_time, 0);

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
