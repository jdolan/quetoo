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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "tests.h"

#include "net/net_chan.h"

Quetoo quetoo;

static NetChan chan;
static byte payload[MAX_MSG_SIZE + MAX_MSG_FRAGMENT_PAYLOAD];

/**
 * @brief Setup fixture.
 */
void setup(void) {

  Mem_Init();
  Cmd_Init();
  Cvar_Init();

  NetChan_Init();

  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = i & 0xff;
  }

  NetAddr addr = { .type = NA_DATAGRAM };
  NetChan_Setup(NS_UDP_CLIENT, &chan, &addr, 0);
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  NetChan_Shutdown();

  Cvar_Shutdown();
  Cmd_Shutdown();
  Mem_Shutdown();
}

/**
 * @brief Writes one unfragmented datagram of `len` payload bytes to `netMessage`.
 */
static void Test_WriteDatagram(uint32_t sequence, size_t len) {

  Mem_ClearBuffer(&netMessage);

  Net_WriteLong(&netMessage, sequence);
  Net_WriteLong(&netMessage, 0);
  Net_WriteData(&netMessage, payload, len);
}

/**
 * @brief Writes one fragment datagram to `netMessage`, its bytes taken from `payload` at `offset`.
 */
static void Test_WriteFragment(uint32_t sequence, bool reliable, size_t offset, size_t len) {

  Mem_ClearBuffer(&netMessage);

  Net_WriteLong(&netMessage, sequence | NETCHAN_FRAGMENT_BIT | (reliable ? NETCHAN_RELIABLE_BIT : 0));
  Net_WriteLong(&netMessage, 0);
  Net_WriteShort(&netMessage, (uint16_t) offset);
  Net_WriteShort(&netMessage, (uint16_t) len);
  Net_WriteData(&netMessage, payload + offset, len);
}

/**
 * @brief Feeds `len` bytes of `payload` to the channel as fragments, and returns the result of the
 * last fragment. Every fragment before the last MUST return false.
 */
static bool Test_ProcessFragments(uint32_t sequence, bool reliable, size_t len) {

  size_t offset = 0;
  while (true) {
    const size_t fragment = Mini(len - offset, MAX_MSG_FRAGMENT_PAYLOAD);

    Test_WriteFragment(sequence, reliable, offset, fragment);
    const bool complete = NetChan_Process(&chan, &netMessage);

    offset += fragment;
    if (fragment < MAX_MSG_FRAGMENT_PAYLOAD) {
      return complete;
    }

    ck_assert(!complete);
  }
}

START_TEST(check_NetChan_Unfragmented) {

  Test_WriteDatagram(1, 100);

  ck_assert(NetChan_Process(&chan, &netMessage));
  ck_assert_int_eq(netMessage.size - netMessage.read, 100);
  ck_assert_int_eq(chan.incomingSequence, 1);

} END_TEST

START_TEST(check_NetChan_Reassemble) {

  for (size_t offset = 0; offset < 2 * MAX_MSG_FRAGMENT_PAYLOAD; offset += MAX_MSG_FRAGMENT_PAYLOAD) {
    Test_WriteFragment(1, false, offset, MAX_MSG_FRAGMENT_PAYLOAD);
    ck_assert(!NetChan_Process(&chan, &netMessage));
    ck_assert_int_eq(chan.incomingSequence, 0);
  }

  Test_WriteFragment(1, false, 2 * MAX_MSG_FRAGMENT_PAYLOAD, 3000 - 2 * MAX_MSG_FRAGMENT_PAYLOAD);
  ck_assert(NetChan_Process(&chan, &netMessage));

  ck_assert_int_eq(netMessage.size, 3000);
  ck_assert_int_eq(netMessage.read, 0);
  ck_assert_int_eq(chan.incomingSequence, 1);

  for (size_t i = 0; i < 3000; i++) {
    ck_assert_int_eq(netMessage.data[i], i & 0xff);
  }

} END_TEST

START_TEST(check_NetChan_ExactMultiple) {

  for (size_t offset = 0; offset < 2 * MAX_MSG_FRAGMENT_PAYLOAD; offset += MAX_MSG_FRAGMENT_PAYLOAD) {
    Test_WriteFragment(1, false, offset, MAX_MSG_FRAGMENT_PAYLOAD);
    ck_assert(!NetChan_Process(&chan, &netMessage));
  }

  Test_WriteFragment(1, false, 2 * MAX_MSG_FRAGMENT_PAYLOAD, 0);
  ck_assert(NetChan_Process(&chan, &netMessage));

  ck_assert_int_eq(netMessage.size, 2 * MAX_MSG_FRAGMENT_PAYLOAD);
  ck_assert_int_eq(chan.incomingSequence, 1);

} END_TEST

START_TEST(check_NetChan_LostFragment) {

  Test_WriteFragment(1, false, 0, MAX_MSG_FRAGMENT_PAYLOAD);
  ck_assert(!NetChan_Process(&chan, &netMessage));

  Test_WriteFragment(1, false, 2 * MAX_MSG_FRAGMENT_PAYLOAD, 100);
  ck_assert(!NetChan_Process(&chan, &netMessage));
  ck_assert_int_eq(chan.incomingSequence, 0);

  ck_assert(Test_ProcessFragments(2, false, 2 * MAX_MSG_FRAGMENT_PAYLOAD + 100));
  ck_assert_int_eq(netMessage.size, 2 * MAX_MSG_FRAGMENT_PAYLOAD + 100);
  ck_assert_int_eq(chan.incomingSequence, 2);

} END_TEST

START_TEST(check_NetChan_NewerSequenceDiscards) {

  Test_WriteFragment(1, false, 0, MAX_MSG_FRAGMENT_PAYLOAD);
  ck_assert(!NetChan_Process(&chan, &netMessage));

  Test_WriteDatagram(2, 100);
  ck_assert(NetChan_Process(&chan, &netMessage));
  ck_assert_int_eq(chan.incomingSequence, 2);

  Test_WriteFragment(1, false, MAX_MSG_FRAGMENT_PAYLOAD, 100);
  ck_assert(!NetChan_Process(&chan, &netMessage));
  ck_assert_int_eq(chan.incomingSequence, 2);

} END_TEST

START_TEST(check_NetChan_ReliableOnce) {

  for (size_t offset = 0; offset < 2 * MAX_MSG_FRAGMENT_PAYLOAD; offset += MAX_MSG_FRAGMENT_PAYLOAD) {
    Test_WriteFragment(1, true, offset, MAX_MSG_FRAGMENT_PAYLOAD);
    ck_assert(!NetChan_Process(&chan, &netMessage));
    ck_assert_int_eq(chan.reliableIncoming, 0);
  }

  Test_WriteFragment(1, true, 2 * MAX_MSG_FRAGMENT_PAYLOAD, 100);
  ck_assert(NetChan_Process(&chan, &netMessage));
  ck_assert_int_eq(chan.reliableIncoming, 1);

} END_TEST

START_TEST(check_NetChan_Oversize) {

  for (size_t offset = 0; offset <= MAX_MSG_SIZE; offset += MAX_MSG_FRAGMENT_PAYLOAD) {
    Test_WriteFragment(1, false, offset, MAX_MSG_FRAGMENT_PAYLOAD);
    ck_assert(!NetChan_Process(&chan, &netMessage));
  }

  ck_assert_int_eq(chan.incomingSequence, 0);

  ck_assert(Test_ProcessFragments(2, false, 100));
  ck_assert_int_eq(netMessage.size, 100);
  ck_assert_int_eq(chan.incomingSequence, 2);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_net_chan");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_NetChan_Unfragmented);
  tcase_add_test(tcase, check_NetChan_Reassemble);
  tcase_add_test(tcase, check_NetChan_ExactMultiple);
  tcase_add_test(tcase, check_NetChan_LostFragment);
  tcase_add_test(tcase, check_NetChan_NewerSequenceDiscards);
  tcase_add_test(tcase, check_NetChan_ReliableOnce);
  tcase_add_test(tcase, check_NetChan_Oversize);

  Suite *suite = suite_create("check_net_chan");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
