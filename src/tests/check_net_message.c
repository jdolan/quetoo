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

Quetoo quetoo;

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
 * @brief Helper: populate every PlayerMoveParams field with a distinct non-default
 * value so a round-trip can detect any dropped or mis-ordered field.
 */
static void Fill_TestParams(PlayerMoveParams *p) {
  p->gravity = 750;
  // a byte no `PlayerMovement` owns: what is under test is that the field reaches
  // the other side intact, not what it selects once it arrives
  p->movement = 200;
  p->accelGround = 11.f;       p->accelGroundSlick = 4.5f;
  p->accelAir = 3.f;           p->accelWater = 3.5f;
  p->accelSpectator = 2.75f;   p->accelLadder = 17.f;
  p->frictionGround = 6.5f;    p->frictionGroundSlick = 2.5f;
  p->frictionAir = 0.25f;      p->frictionWater = 2.25f;
  p->frictionSpectator = 2.6f; p->frictionLadder = 5.5f;
  p->speedGround = 310.f;      p->speedAir = 360.f;
  p->speedWater = 150.f;       p->speedLadder = 130.f;
  p->speedSpectator = 510.f;   p->speedStop = 110.f;
  p->speedJump = 280.f;        p->speedDucked = 145.f;
  p->speedDuckStand = 205.f;  p->speedWaterJump = 430.f;
  // every component distinct, so a dropped or transposed one shows, and every
  // one fractional, so a serializer that quantized the box to whole units - as
  // Net_WriteBounds does - would fail here rather than in someone's prediction
  p->bounds = (Box3) { .mins = { { -17.25f, -18.5f, -25.75f } },
                         .maxs = { {  19.25f,  20.5f,  37.75f } } };
  p->boundsDucked = (Box3) { .mins = { { -21.25f, -22.5f, -26.75f } },
                                .maxs = { {  23.25f,  24.5f,   7.75f } } };
  p->boundsDead = (Box3) { .mins = { { -27.25f, -28.5f, -29.75f } },
                              .maxs = { {  30.25f,  31.5f,  -3.25f } } };
}

/**
 * @brief Every server-tunable movement parameter must survive a player-state
 * delta round-trip so that client prediction reads identical values.
 */
START_TEST(check_PlayerState_Params_RoundTrip) {
  byte buffer[MAX_MSG_SIZE];
  MemBuf buf;
  Mem_InitBuffer(&buf, buffer, sizeof(buffer));

  PlayerState from;
  memset(&from, 0, sizeof(from));

  PlayerState to;
  memset(&to, 0, sizeof(to));
  Fill_TestParams(&to.pmState.params);

  Net_WriteDeltaPlayerState(&buf, &from, &to);
  buf.read = 0;

  PlayerState result;
  memset(&result, 0, sizeof(result));
  Net_ReadDeltaPlayerState(&buf, &from, &result);

  ck_assert_int_eq(result.pmState.params.gravity, to.pmState.params.gravity);
  ck_assert_int_eq(result.pmState.params.movement, to.pmState.params.movement);

  ck_assert_msg(memcmp(&result.pmState.params.accelGround, &to.pmState.params.accelGround,
                       sizeof(PlayerMoveParams) - offsetof(PlayerMoveParams, accelGround)) == 0,
                "PlayerMoveParams (non-gravity) did not survive the round-trip");
} END_TEST

/**
 * @brief When the params are unchanged from the delta baseline, no params
 * payload should be written (delta compression must skip the block).
 */
START_TEST(check_PlayerState_Params_DeltaCompressed) {
  byte bufA[MAX_MSG_SIZE], bufB[MAX_MSG_SIZE];
  MemBuf equal, diff;
  Mem_InitBuffer(&equal, bufA, sizeof(bufA));
  Mem_InitBuffer(&diff, bufB, sizeof(bufB));

  PlayerState base;
  memset(&base, 0, sizeof(base));
  Fill_TestParams(&base.pmState.params);

  PlayerState zero;
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
  byte bufA[MAX_MSG_SIZE], bufB[MAX_MSG_SIZE];
  MemBuf movementOnly, unchanged;
  Mem_InitBuffer(&movementOnly, bufA, sizeof(bufA));
  Mem_InitBuffer(&unchanged, bufB, sizeof(bufB));

  PlayerState from;
  memset(&from, 0, sizeof(from));
  Fill_TestParams(&from.pmState.params);

  PlayerState to = from;
  to.pmState.params.movement = PM_MOVEMENT_QUAKE; // Fill_TestParams left it elsewhere

  Net_WriteDeltaPlayerState(&movementOnly, &from, &to);
  Net_WriteDeltaPlayerState(&unchanged, &from, &from);

  ck_assert_msg(movementOnly.size == unchanged.size + 1,
                "a movement change should cost one byte over no change (%zu vs %zu)",
                movementOnly.size, unchanged.size);

  movementOnly.read = 0;

  PlayerState result = from;
  Net_ReadDeltaPlayerState(&movementOnly, &from, &result);

  ck_assert_int_eq(result.pmState.params.movement, to.pmState.params.movement);
  ck_assert_int_eq(result.pmState.params.gravity, from.pmState.params.gravity);
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
