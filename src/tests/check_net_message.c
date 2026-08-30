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

#include "net/net_message.h"
#include "game/common/bg_pmove.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {
  Mem_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {
  Mem_Shutdown();
}

/**
 * @brief Helper: populate every pm_params_t field with a distinct non-default
 * value so a round-trip can detect any dropped or mis-ordered field.
 */
static void Fill_TestParams(pm_params_t *p) {
  p->gravity = 750;
  // a byte no `pm_movement_t` owns: what is under test is that the field reaches
  // the other side intact, not what it selects once it arrives
  p->movement = 200;
  p->accel_ground = 11.f;       p->accel_ground_slick = 4.5f;
  p->accel_air = 3.f;           p->accel_water = 3.5f;
  p->accel_spectator = 2.75f;   p->accel_ladder = 17.f;
  p->friction_ground = 6.5f;    p->friction_ground_slick = 2.5f;
  p->friction_air = 0.25f;      p->friction_water = 2.25f;
  p->friction_spectator = 2.6f; p->friction_ladder = 5.5f;
  p->speed_ground = 310.f;      p->speed_air = 360.f;
  p->speed_water = 150.f;       p->speed_ladder = 130.f;
  p->speed_spectator = 510.f;   p->speed_stop = 110.f;
  p->speed_jump = 280.f;        p->speed_ducked = 145.f;
  p->speed_duck_stand = 205.f;  p->speed_water_jump = 430.f;
  // every component distinct, so a dropped or transposed one shows, and every
  // one fractional, so a serializer that quantized the box to whole units - as
  // Net_WriteBounds does - would fail here rather than in someone's prediction
  p->bounds = (box3_t) { .mins = { { -17.25f, -18.5f, -25.75f } },
                         .maxs = { {  19.25f,  20.5f,  37.75f } } };
  p->bounds_ducked = (box3_t) { .mins = { { -21.25f, -22.5f, -26.75f } },
                                .maxs = { {  23.25f,  24.5f,   7.75f } } };
  p->bounds_dead = (box3_t) { .mins = { { -27.25f, -28.5f, -29.75f } },
                              .maxs = { {  30.25f,  31.5f,  -3.25f } } };
}

/**
 * @brief Every server-tunable movement parameter must survive a player-state
 * delta round-trip so that client prediction reads identical values.
 */
START_TEST(check_PlayerState_Params_RoundTrip) {
  byte buffer[MAX_MSG_SIZE];
  mem_buf_t buf;
  Mem_InitBuffer(&buf, buffer, sizeof(buffer));

  player_state_t from;
  memset(&from, 0, sizeof(from));

  player_state_t to;
  memset(&to, 0, sizeof(to));
  Fill_TestParams(&to.pm_state.params);

  Net_WriteDeltaPlayerState(&buf, &from, &to);
  buf.read = 0;

  player_state_t result;
  memset(&result, 0, sizeof(result));
  Net_ReadDeltaPlayerState(&buf, &from, &result);

  ck_assert_int_eq(result.pm_state.params.gravity, to.pm_state.params.gravity);
  ck_assert_int_eq(result.pm_state.params.movement, to.pm_state.params.movement);

  ck_assert_msg(memcmp(&result.pm_state.params.accel_ground, &to.pm_state.params.accel_ground,
                       sizeof(pm_params_t) - offsetof(pm_params_t, accel_ground)) == 0,
                "pm_params_t (non-gravity) did not survive the round-trip");
} END_TEST

/**
 * @brief When the params are unchanged from the delta baseline, no params
 * payload should be written (delta compression must skip the block).
 */
START_TEST(check_PlayerState_Params_DeltaCompressed) {
  byte buf_a[MAX_MSG_SIZE], buf_b[MAX_MSG_SIZE];
  mem_buf_t equal, diff;
  Mem_InitBuffer(&equal, buf_a, sizeof(buf_a));
  Mem_InitBuffer(&diff, buf_b, sizeof(buf_b));

  player_state_t base;
  memset(&base, 0, sizeof(base));
  Fill_TestParams(&base.pm_state.params);

  player_state_t zero;
  memset(&zero, 0, sizeof(zero));

  // identical params: no PS_PM_PARAMS payload
  Net_WriteDeltaPlayerState(&equal, &base, &base);
  // changed params: full PS_PM_PARAMS payload
  Net_WriteDeltaPlayerState(&diff, &zero, &base);

  ck_assert_msg(equal.size < diff.size,
                "unchanged params still wrote a payload (%zu vs %zu)",
                equal.size, diff.size);
} END_TEST

/**
 * @brief The movement is delta-compressed on its own bit: a change to it alone
 * must reach the client, and must not cost the whole parameter block.
 */
START_TEST(check_PlayerState_Movement_DeltaCompressed) {
  byte buf_a[MAX_MSG_SIZE], buf_b[MAX_MSG_SIZE];
  mem_buf_t movement_only, unchanged;
  Mem_InitBuffer(&movement_only, buf_a, sizeof(buf_a));
  Mem_InitBuffer(&unchanged, buf_b, sizeof(buf_b));

  player_state_t from;
  memset(&from, 0, sizeof(from));
  Fill_TestParams(&from.pm_state.params);

  player_state_t to = from;
  to.pm_state.params.movement = PM_MOVEMENT_QUAKE; // Fill_TestParams left it elsewhere

  Net_WriteDeltaPlayerState(&movement_only, &from, &to);
  Net_WriteDeltaPlayerState(&unchanged, &from, &from);

  ck_assert_msg(movement_only.size == unchanged.size + 1,
                "a movement change should cost one byte over no change (%zu vs %zu)",
                movement_only.size, unchanged.size);

  movement_only.read = 0;

  player_state_t result = from;
  Net_ReadDeltaPlayerState(&movement_only, &from, &result);

  ck_assert_int_eq(result.pm_state.params.movement, to.pm_state.params.movement);
  ck_assert_int_eq(result.pm_state.params.gravity, from.pm_state.params.gravity);
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_net_message");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_PlayerState_Params_RoundTrip);
  tcase_add_test(tcase, check_PlayerState_Params_DeltaCompressed);
  tcase_add_test(tcase, check_PlayerState_Movement_DeltaCompressed);

  Suite *suite = suite_create("check_net_message");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
