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
    const size_t fragment = Minz(len - offset, MAX_MSG_FRAGMENT_PAYLOAD);

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

#define TEST_PORT "27993"

static NetChan client, server;
static size_t largestDatagram;

/**
 * @brief Setup fixture for tests over real UDP sockets on 127.0.0.1.
 */
static void setup_udp(void) {

  setup();

  Cvar_Add("net_port", TEST_PORT, CVAR_NO_SET, NULL);

  Net_Config(NS_UDP_SERVER, true);
  Net_Config(NS_UDP_CLIENT, true);

  NetAddr addr;
  ck_assert(Net_StringToNetaddr("127.0.0.1:" TEST_PORT, &addr));

  NetChan_Setup(NS_UDP_CLIENT, &client, &addr, 42);

  largestDatagram = 0;
}

/**
 * @brief Teardown fixture for tests over real UDP sockets.
 */
static void teardown_udp(void) {
  teardown();
}

/**
 * @brief Reads datagrams for `source` until `NetChan_Process` completes a message on `chan`, or
 * one second passes. Records the largest datagram in `largestDatagram`.
 */
static bool Test_Receive(NetSrc source, NetChan *chan) {

  const uint64_t deadline = SDL_GetTicks() + 1000;

  while (SDL_GetTicks() < deadline) {
    NetAddr from;
    if (Net_ReceiveDatagram(source, &from, &netMessage)) {
      largestDatagram = Maxz(largestDatagram, netMessage.size);
      if (NetChan_Process(chan, &netMessage)) {
        return true;
      }
    } else {
      SDL_Delay(1);
    }
  }

  return false;
}

/**
 * @brief Connects the server channel to the client, as the server does on `connect`.
 */
static void Test_Connect(void) {

  NetChan_Transmit(&client, payload, 10);

  NetAddr from;
  const uint64_t deadline = SDL_GetTicks() + 1000;
  while (!Net_ReceiveDatagram(NS_UDP_SERVER, &from, &netMessage)) {
    ck_assert(SDL_GetTicks() < deadline);
    SDL_Delay(1);
  }

  NetChan_Setup(NS_UDP_SERVER, &server, &from, 42);
  ck_assert(NetChan_Process(&server, &netMessage));
}

/**
 * @brief Asserts that `netMessage` holds exactly the first `len` bytes of `payload`.
 */
static void Test_AssertPayload(size_t len) {

  ck_assert_int_eq(netMessage.size - netMessage.read, len);
  ck_assert(memcmp(netMessage.data + netMessage.read, payload, len) == 0);
}

START_TEST(check_NetChan_TransmitSmall) {

  Test_Connect();

  NetChan_Transmit(&server, payload, 500);

  ck_assert(Test_Receive(NS_UDP_CLIENT, &client));
  Test_AssertPayload(500);
  ck_assert_int_eq(largestDatagram, 500 + 8);

} END_TEST

START_TEST(check_NetChan_TransmitLarge) {

  Test_Connect();

  NetChan_Transmit(&server, payload, 10000);

  ck_assert(Test_Receive(NS_UDP_CLIENT, &client));
  Test_AssertPayload(10000);
  ck_assert_int_le(largestDatagram, MAX_MSG_FRAGMENT_SIZE);

} END_TEST

START_TEST(check_NetChan_TransmitExactMultiple) {

  Test_Connect();

  NetChan_Transmit(&server, payload, 3 * MAX_MSG_FRAGMENT_PAYLOAD);

  ck_assert(Test_Receive(NS_UDP_CLIENT, &client));
  Test_AssertPayload(3 * MAX_MSG_FRAGMENT_PAYLOAD);
  ck_assert_int_le(largestDatagram, MAX_MSG_FRAGMENT_SIZE);

} END_TEST

START_TEST(check_NetChan_TransmitClientToServer) {

  Test_Connect();

  NetChan_Transmit(&client, payload, 5000);

  ck_assert(Test_Receive(NS_UDP_SERVER, &server));
  Test_AssertPayload(5000);
  ck_assert_int_le(largestDatagram, MAX_MSG_FRAGMENT_SIZE);

} END_TEST

START_TEST(check_NetChan_TransmitReliable) {

  Test_Connect();

  Net_WriteData(&server.message, payload, 6000);
  NetChan_Transmit(&server, NULL, 0);

  ck_assert(Test_Receive(NS_UDP_CLIENT, &client));
  Test_AssertPayload(6000);
  ck_assert_int_eq(client.reliableIncoming, 1);
  ck_assert_int_le(largestDatagram, MAX_MSG_FRAGMENT_SIZE);

} END_TEST

START_TEST(check_NetChan_TransmitDumpsUnreliable) {

  Test_Connect();

  Net_WriteData(&server.message, payload, 4000);
  NetChan_Transmit(&server, payload, MAX_MSG_SIZE - 1000);

  ck_assert(Test_Receive(NS_UDP_CLIENT, &client));
  Test_AssertPayload(4000);

} END_TEST

START_TEST(check_NetChan_TransmitMaximum) {

  ck_assert_int_eq(MAX_MSG_SIZE, 65536);

  Test_Connect();

  NetChan_Transmit(&server, payload, sizeof(server.messageBuffer));

  ck_assert(Test_Receive(NS_UDP_CLIENT, &client));
  Test_AssertPayload(sizeof(server.messageBuffer));
  ck_assert_int_le(largestDatagram, MAX_MSG_FRAGMENT_SIZE);

} END_TEST

START_TEST(check_NetChan_LoopbackUnfragmented) {

  NetAddr addr = { .type = NA_LOOP };
  NetChan_Setup(NS_UDP_SERVER, &server, &addr, 0);

  NetChan_Transmit(&server, payload, 10000);

  NetAddr from;
  ck_assert(Net_ReceiveDatagram(NS_UDP_CLIENT, &from, &netMessage));
  ck_assert_int_eq(from.type, NA_LOOP);
  ck_assert_int_eq(netMessage.size, 10000 + 8);
  ck_assert(!Net_ReceiveDatagram(NS_UDP_CLIENT, &from, &netMessage));

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

  TCase *udp = tcase_create("check_net_chan_udp");
  tcase_add_checked_fixture(udp, setup_udp, teardown_udp);

  tcase_add_test(udp, check_NetChan_TransmitSmall);
  tcase_add_test(udp, check_NetChan_TransmitLarge);
  tcase_add_test(udp, check_NetChan_TransmitExactMultiple);
  tcase_add_test(udp, check_NetChan_TransmitClientToServer);
  tcase_add_test(udp, check_NetChan_TransmitReliable);
  tcase_add_test(udp, check_NetChan_TransmitDumpsUnreliable);
  tcase_add_test(udp, check_NetChan_TransmitMaximum);
  tcase_add_test(udp, check_NetChan_LoopbackUnfragmented);

  Suite *suite = suite_create("check_net_chan");
  suite_add_tcase(suite, tcase);
  suite_add_tcase(suite, udp);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
