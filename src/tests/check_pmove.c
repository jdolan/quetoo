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

#include "game/common/bg_pmove.h"

quetoo_t quetoo;

/**
 * @file
 * @brief What each movement is, asserted against a flat floor.
 *
 * These are the properties that distinguish the movements from one another, so
 * that a port cannot quietly become a different game. The QuakeWorld cases are
 * the interesting ones: its speed cap, its jump, its refusal to duck, and the
 * speed gain that strafing while airborne produces, which is bunny hopping and
 * the reason anyone wants this movement at all.
 */

#define TEST_FLOOR 0.f

// a full-strength movement intent, above any movement's own cap
#define TEST_INTENT 400

// the upward speed above which Quake stops considering a player grounded
#define PM_QUAKE_UP_SPEED_FOR_TEST 180.f

static void *test_ground_ent = (void *) (intptr_t) 1;

/**
 * @brief A world that is nothing but a floor at z = 0.
 */
static cm_trace_t Test_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {

  cm_trace_t trace = { .fraction = 1.f, .end = end };

  const float from = start.z + bounds.mins.z;
  const float to = end.z + bounds.mins.z;

  if (from < TEST_FLOOR) { // started inside the floor
    trace.start_solid = true;
    trace.all_solid = to < TEST_FLOOR;
    trace.fraction = 0.f;
    trace.end = start;
    trace.plane.normal = Vec3_Up();
    trace.ent = test_ground_ent;
    return trace;
  }

  if (to < TEST_FLOOR) { // struck it
    trace.fraction = (from - TEST_FLOOR) / (from - to);
    trace.end = Vec3_Mix(start, end, trace.fraction);
    trace.plane.normal = Vec3_Up();
    trace.ent = test_ground_ent;
  }

  return trace;
}

static int32_t Test_PointContents(const vec3_t point) {
  return 0;
}

static int32_t Test_BoxContents(const box3_t box) {
  return 0;
}

static debug_t Test_DebugMask(void) {
  return 0;
}

static void Test_Debug(const debug_t debug, const char *func, const char *fmt, ...) {
}

/**
 * @brief A player standing on the floor, moving by `movement`.
 */
static pm_move_t Test_Move(pm_movement_t movement) {

  const pm_movement_info_t *info = Pm_Movement(movement);
  ck_assert_msg(info, "no such movement");

  pm_move_t pm = {
    .PointContents = Test_PointContents,
    .BoxContents = Test_BoxContents,
    .Trace = Test_Trace,
    .Debug = Test_Debug,
    .DebugMask = Test_DebugMask,
  };

  if (info->params) {
    pm.s.params = *info->params;
  } else { // Quetoo's follows the server's cvars, which a test has none of
    pm.s.params = (pm_params_t) {
      .gravity = 800,
      .gravity_water = PM_GRAVITY_WATER,
      .accel_ground = PM_ACCEL_GROUND,
      .accel_air = PM_ACCEL_AIR,
      .friction_ground = PM_FRICT_GROUND,
      .friction_air = PM_FRICT_AIR,
      .speed_ground = PM_SPEED_RUN,
      .speed_air = PM_SPEED_AIR,
      .speed_water = PM_SPEED_WATER,
      .accel_water = PM_ACCEL_WATER,
      .friction_water = PM_FRICT_WATER,
      .speed_stop = PM_SPEED_STOP,
      .speed_jump = PM_SPEED_JUMP,
      .speed_ducked = PM_SPEED_DUCKED,
      .speed_duck_stand = PM_SPEED_DUCK_STAND,
      .height = 36.f,
      .height_ducked = 6.f,
    };
  }

  pm.s.params.movement = movement;
  pm.s.origin = Vec3(0.f, 0.f, 24.f); // mins.z is -24, so this rests on the floor

  return pm;
}

/**
 * @brief Runs one 100ms command.
 */
static void Test_Command(pm_move_t *pm, int16_t forward, int16_t right, int16_t up) {

  pm->cmd = (pm_cmd_t) {
    .msec = 100,
    .forward = forward,
    .right = right,
    .up = up,
  };

  Pm_Move(pm);
}

/**
 * @brief QuakeWorld runs at its own speed, not the server's.
 */
START_TEST(check_Quake_GroundSpeed) {

  pm_move_t pm = Test_Move(PM_MOVEMENT_QUAKE);

  for (int32_t i = 0; i < 60; i++) {
    Test_Command(&pm, TEST_INTENT, 0, 0);
  }

  const float speed = Vec2_Length(Vec3_XY(pm.s.velocity));

  ck_assert_msg(pm.s.flags & PMF_ON_GROUND, "did not stay on the ground");
  ck_assert_msg(fabsf(speed - 320.f) < 1.f,
                "QuakeWorld ran at %g, expected its own 320", speed);
} END_TEST

/**
 * @brief Quake's jump adds to whatever vertical speed the player had, which is
 * what a jump off a lift keeps, and it does not repeat while held.
 */
START_TEST(check_Quake_Jump) {

  pm_move_t pm = Test_Move(PM_MOVEMENT_QUAKE);

  Test_Command(&pm, 0, 0, 0);
  ck_assert_msg(pm.s.flags & PMF_ON_GROUND, "did not start on the ground");

  // already rising, but not fast enough to have left the ground: a jump that
  // assigned 270 rather than adding it would be indistinguishable from a correct
  // one at rest, so the two are only told apart from here
  const float rising = 100.f;
  ck_assert_msg(rising < PM_QUAKE_UP_SPEED_FOR_TEST, "the setup left the ground");
  pm.s.velocity.z = rising;

  Test_Command(&pm, 0, 0, 1);

  ck_assert_msg(!(pm.s.flags & PMF_ON_GROUND), "stayed on the ground after jumping");

  // one command of gravity has already been taken out of it
  const float expected = rising + 270.f - 800.f * .1f;
  ck_assert_msg(fabsf(pm.s.velocity.z - expected) < 1.f,
                "jumped to %g, expected %g: Quake adds to the vertical speed it "
                "finds rather than replacing it", pm.s.velocity.z, expected);
} END_TEST

/**
 * @brief Quake has no crouch, so this movement never ducks however hard it is
 * asked to.
 */
START_TEST(check_Quake_DoesNotDuck) {

  pm_move_t pm = Test_Move(PM_MOVEMENT_QUAKE);

  for (int32_t i = 0; i < 10; i++) {
    Test_Command(&pm, 0, 0, -1);

    ck_assert_msg(!(pm.s.flags & PMF_DUCKED), "ducked, which Quake cannot do");
    ck_assert_msg(pm.bounds.maxs.z == 32.f,
                  "the box shrank to %g", pm.bounds.maxs.z);
  }
} END_TEST

/**
 * @brief Airborne, Quake bleeds no speed at all.
 */
START_TEST(check_Quake_NoAirFriction) {

  pm_move_t pm = Test_Move(PM_MOVEMENT_QUAKE);

  pm.s.origin.z = 256.f;
  pm.s.velocity = Vec3(300.f, 0.f, 0.f);

  Test_Command(&pm, 0, 0, 0);
  const float before = Vec2_Length(Vec3_XY(pm.s.velocity));

  Test_Command(&pm, 0, 0, 0);
  const float after = Vec2_Length(Vec3_XY(pm.s.velocity));

  ck_assert_msg(!(pm.s.flags & PMF_ON_GROUND), "was on the ground");
  ck_assert_msg(fabsf(after - before) < .01f,
                "lost speed in the air: %g then %g", before, after);
} END_TEST

/**
 * @brief The bunny hop. Airborne and already at the cap, a player who keeps the
 * direction they are asking for perpendicular to the direction they are already
 * travelling gains speed, because the wish speed is capped at 30 while the
 * acceleration it earns is scaled by the uncapped one.
 * @details Turning the view to hold that perpendicular is the technique, and it
 * is why this is a skill rather than a button. The dot product of the velocity
 * with the wish direction has to stay under the 30-unit cap for there to be any
 * headroom at all; aim 45 degrees off instead and it is 226, so nothing is
 * gained. That is the whole mechanism.
 */
START_TEST(check_Quake_BunnyHop) {

  pm_move_t pm = Test_Move(PM_MOVEMENT_QUAKE);

  pm.s.origin.z = 8192.f; // far enough to stay airborne for the whole run
  pm.s.velocity = Vec3(320.f, 0.f, 0.f);

  const float before = Vec2_Length(Vec3_XY(pm.s.velocity));

  for (int32_t i = 0; i < 20; i++) {

    // look along the way we are already going, and hold strafe: the strafe axis
    // is then square to the velocity, whichever way it points
    const float yaw = Degrees(atan2f(pm.s.velocity.y, pm.s.velocity.x));

    pm.cmd = (pm_cmd_t) {
      .msec = 100,
      .angles = Vec3(0.f, yaw, 0.f),
      .right = TEST_INTENT,
    };

    Pm_Move(&pm);
  }

  const float after = Vec2_Length(Vec3_XY(pm.s.velocity));

  ck_assert_msg(!(pm.s.flags & PMF_ON_GROUND), "landed, so this proves nothing");
  ck_assert_msg(after > before + 10.f,
                "no speed was gained by strafing in the air: %g then %g", before, after);
} END_TEST

/**
 * @brief Quetoo's movement is unaffected by any of the above.
 */
START_TEST(check_Quetoo_GroundSpeed) {

  pm_move_t pm = Test_Move(PM_MOVEMENT_QUETOO);

  for (int32_t i = 0; i < 60; i++) {
    Test_Command(&pm, TEST_INTENT, 0, 0);
  }

  const float speed = Vec2_Length(Vec3_XY(pm.s.velocity));

  ck_assert_msg(pm.s.flags & PMF_ON_GROUND, "did not stay on the ground");
  ck_assert_msg(fabsf(speed - PM_SPEED_RUN) < 1.f,
                "Quetoo ran at %g, expected %g", speed, PM_SPEED_RUN);
} END_TEST

/**
 * @brief The two movements stand in different boxes and look out of them from
 * different heights, which is a large part of why they feel different.
 */
START_TEST(check_Movement_BoxAndEye) {

  pm_move_t quake = Test_Move(PM_MOVEMENT_QUAKE);
  Test_Command(&quake, 0, 0, 0);

  pm_move_t quetoo = Test_Move(PM_MOVEMENT_QUETOO);
  for (int32_t i = 0; i < 20; i++) { // Quetoo lerps its eye, so let it settle
    Test_Command(&quetoo, 0, 0, 0);
  }

  ck_assert_msg(quake.bounds.maxs.z == 32.f,
                "Quake stood %g tall, expected 32", quake.bounds.maxs.z);
  ck_assert_msg(quetoo.bounds.maxs.z == 36.f,
                "Quetoo stood %g tall, expected 36", quetoo.bounds.maxs.z);

  ck_assert_msg(quake.s.view_offset.z == 22.f,
                "Quake's eye was at %g, expected 22", quake.s.view_offset.z);
  ck_assert_msg(fabsf(quetoo.s.view_offset.z - 30.f) < .01f,
                "Quetoo's eye was at %g, expected 30", quetoo.s.view_offset.z);
} END_TEST

/**
 * @brief Every movement must answer to the name its cvar and worldspawn key use,
 * and none of them to "default", which those reserve.
 */
START_TEST(check_Movement_Names) {

  for (size_t i = 0; i < Pm_MovementCount(); i++) {
    const pm_movement_info_t *info = Pm_Movement((pm_movement_t) i);

    ck_assert_msg(info && info->name && *info->name, "movement %zu has no name", i);
    ck_assert_msg(q_strcasecmp(info->name, "default"),
                  "movement %zu is named \"default\", which is reserved", i);

    pm_movement_t resolved = (pm_movement_t) -1;
    ck_assert_msg(Pm_MovementByName(info->name, &resolved), "%s did not resolve", info->name);
    ck_assert_int_eq(resolved, (pm_movement_t) i);
  }

  pm_movement_t unused = PM_MOVEMENT_QUETOO;
  ck_assert_msg(!Pm_MovementByName("default", &unused), "\"default\" resolved to a movement");
  ck_assert_msg(!Pm_MovementByName("nonesuch", &unused), "a garbage name resolved");
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_pmove");

  tcase_add_test(tcase, check_Quake_GroundSpeed);
  tcase_add_test(tcase, check_Quake_Jump);
  tcase_add_test(tcase, check_Quake_DoesNotDuck);
  tcase_add_test(tcase, check_Quake_NoAirFriction);
  tcase_add_test(tcase, check_Quake_BunnyHop);
  tcase_add_test(tcase, check_Quetoo_GroundSpeed);
  tcase_add_test(tcase, check_Movement_BoxAndEye);
  tcase_add_test(tcase, check_Movement_Names);

  Suite *suite = suite_create("check_pmove");
  suite_add_tcase(suite, tcase);

  const int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
