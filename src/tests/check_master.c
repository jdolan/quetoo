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

#define main Ms_Main
#include "../master/main.c"
#undef main

/**
 * @brief Setup fixture.
 */
void setup(void) {

  Mem_Init();

  Fs_Init(FS_NONE);

  ck_assert(Fs_SetGame(TEST_GAME, NULL));
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  Fs_Shutdown();

  Mem_Shutdown();
}

START_TEST(check_Ms_AddServer) {
  ck_assert_int_eq(msServers ? (int) msServers->count : 0, 0);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("192.168.1.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_SERVER);

  Ms_AddServer(&addr);
  ck_assert_int_eq((int) msServers->count, 1);

  MasterServer *server = (MasterServer *) msServers->head->element;
  ck_assert_msg(server->addr.sin_addr.s_addr == addr.sin_addr.s_addr, "Corrupt server address");

  Ms_AddServer(&addr);
  ck_assert_int_eq((int) msServers->count, 1);

  *(in_addr_t *) &addr.sin_addr = inet_addr("192.168.1.2");

  Ms_AddServer(&addr);
  ck_assert_int_eq((int) msServers->count, 2);

  server = Ms_GetServer(&addr);
  ck_assert_msg(server != NULL, "Server was not registered");

  server->challenge = 42u;

  Ms_RemoveServer(&addr, "shutdown");
  ck_assert_int_eq((int) msServers->count, 2);

  Ms_RemoveServer(&addr, "shutdown 41");
  ck_assert_int_eq((int) msServers->count, 2);

  Ms_RemoveServer(&addr, va("shutdown %u", server->challenge));
  ck_assert_int_eq((int) msServers->count, 1);

  MasterServer *s = Ms_GetServer(&addr);
  ck_assert_msg(!s, "Server was not NULL");

} END_TEST

/**
 * @brief Replaces the blacklist file with the specified rules.
 */
static void write_blacklist(const char *rules) {
  File *f = Fs_OpenWrite("servers-blacklist");
  ck_assert_msg(f != NULL, "Failed to open servers-blacklist");

  const int64_t len = Fs_Write(f, (void *) rules, 1, strlen(rules));

  ck_assert_msg((size_t) len == strlen(rules), "Failed to write servers-blacklist");
  ck_assert_msg(Fs_Close(f), "Failed to close servers-blacklist");
}

START_TEST(check_Ms_BlacklistServer) {
  write_blacklist("192.168.0.*\n// a comment\n\n10.0.0.1:27910 # a trailing comment\n");

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("192.168.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_SERVER);

  ck_assert_msg(Ms_BlacklistServer(&addr), "Missed %s", inet_ntoa(addr.sin_addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("127.0.0.1");

  ck_assert_msg(!Ms_BlacklistServer(&addr), "False positive for %s", inet_ntoa(addr.sin_addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("10.0.0.1");
  addr.sin_port = htons(27910);

  ck_assert_msg(Ms_BlacklistServer(&addr), "Missed %s", inet_ntoa(addr.sin_addr));

  addr.sin_port = htons(27911);

  ck_assert_msg(!Ms_BlacklistServer(&addr), "False positive for %s", inet_ntoa(addr.sin_addr));

  // a rewrite within the same second must still be picked up
  write_blacklist("10.0.0.*\n");

  ck_assert_msg(Ms_BlacklistServer(&addr), "Missed %s after reload", inet_ntoa(addr.sin_addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("192.168.0.1");

  ck_assert_msg(!Ms_BlacklistServer(&addr), "Stale rule matched %s", inet_ntoa(addr.sin_addr));

} END_TEST

/**
 * @brief Registers a validated server at the given address and protocol.
 */
static void add_validated_server(const char *ip, uint16_t port, int32_t protocol) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr(ip);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  Ms_AddServer(&addr);

  MasterServer *server = Ms_GetServer(&addr);
  ck_assert_msg(server != NULL, "Server was not registered");

  server->validated = true;
  server->protocol = protocol;
}

/**
 * @brief Issues cmd to the master over loopback, either dispatched through
 * Ms_ParseMessage as a datagram would be, or handed straight to Ms_GetServers.
 * @return The number of servers in the reply, or -1 if there was no reply.
 */
static int32_t query_master(const char *cmd, bool dispatch) {
  const int32_t rx = (int32_t) socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ck_assert_msg(rx != -1, "Failed to create the receiving socket");

  struct sockaddr_in to;
  memset(&to, 0, sizeof(to));

  *(in_addr_t *) &to.sin_addr = inet_addr("127.0.0.1");
  to.sin_family = AF_INET;
  to.sin_port = 0;

  ck_assert_msg(bind(rx, (struct sockaddr *) &to, sizeof(to)) != -1, "Failed to bind");

  socklen_t len = sizeof(to);
  ck_assert_msg(getsockname(rx, (struct sockaddr *) &to, &len) != -1, "Failed to resolve the port");

  // so that a regression which sends nothing fails rather than hanging
  struct timeval timeout = { .tv_usec = 250000 };
  ck_assert_msg(setsockopt(rx, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != -1,
                "Failed to set the receive timeout");

  msSock = (int32_t) socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ck_assert_msg(msSock != -1, "Failed to create the master socket");

  if (dispatch) {
    char data[256];
    q_snprintf(data, sizeof(data), "\xFF\xFF\xFF\xFF%s", cmd);
    Ms_ParseMessage(&to, data);
  } else {
    Ms_GetServers(&to, cmd);
  }

  byte buffer[0xffff];
  const ssize_t received = recv(rx, (char *) buffer, sizeof(buffer), 0);

  close(rx);
  close(msSock);
  msSock = 0;

  if (received == -1) {
    return -1;
  }

  const char *header = "\xFF\xFF\xFF\xFF" "servers ";
  const size_t headerLen = q_strlen(header);

  ck_assert_msg(received >= (ssize_t) headerLen, "Truncated reply to '%s'", cmd);
  ck_assert_msg(!memcmp(buffer, header, headerLen), "Corrupt reply to '%s'", cmd);

  const size_t servers = (size_t) received - headerLen;
  ck_assert_msg(servers % 6 == 0, "Ragged server list for '%s'", cmd);

  return (int32_t) (servers / 6);
}

/**
 * @brief Issues cmd straight to Ms_GetServers over loopback.
 * @return The number of servers in the reply.
 */
static int32_t query_servers(const char *cmd) {
  return query_master(cmd, false);
}

START_TEST(check_Ms_GetServers) {
  add_validated_server("192.168.1.1", PORT_SERVER, PROTOCOL_MAJOR);
  add_validated_server("192.168.1.2", PORT_SERVER, PROTOCOL_MAJOR - 1);

  // a server registered before sv_protocol existed reports no protocol at all
  add_validated_server("192.168.1.3", PORT_SERVER, 0);

  // a query naming no protocol gets only servers a current client could join
  ck_assert_int_eq(query_servers("getservers"), 1);

  ck_assert_int_eq(query_servers(va("getservers %d", PROTOCOL_MAJOR)), 1);
  ck_assert_int_eq(query_servers(va("getservers %d", PROTOCOL_MAJOR - 1)), 1);
  ck_assert_int_eq(query_servers(va("getservers %d", PROTOCOL_MAJOR + 1)), 0);

  // anything below one asks for the whole registry
  ck_assert_int_eq(query_servers("getservers 0"), 3);
  ck_assert_int_eq(query_servers("getservers -1"), 3);

  // an argument we cannot parse must not be mistaken for that wildcard
  ck_assert_int_eq(query_servers("getservers abc"), 1);
  ck_assert_int_eq(query_servers(va("getservers %dx", PROTOCOL_MAJOR)), 1);

  // nor may one we cannot represent wrap onto a live protocol, or onto nothing
  ck_assert_int_eq(query_servers("getservers 99999999999999999999"), 1);
  ck_assert_int_eq(query_servers("getservers 2147483648"), 1);
  ck_assert_int_eq(query_servers(va("getservers %lld", 4294967296LL + PROTOCOL_MAJOR)), 1);

  // surrounding whitespace is not an argument we cannot parse
  ck_assert_int_eq(query_servers(va("getservers  %d ", PROTOCOL_MAJOR)), 1);

  // an unvalidated server is never published, whatever is asked for
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("192.168.1.4");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_SERVER);

  Ms_AddServer(&addr);
  Ms_GetServer(&addr)->protocol = PROTOCOL_MAJOR;

  ck_assert_int_eq(query_servers("getservers"), 1);
  ck_assert_int_eq(query_servers("getservers 0"), 3);

} END_TEST

START_TEST(check_Ms_ParseMessage) {
  add_validated_server("192.168.1.1", PORT_SERVER, PROTOCOL_MAJOR);

  ck_assert_int_eq(query_master("getservers", true), 1);
  ck_assert_int_eq(query_master(va("getservers %d", PROTOCOL_MAJOR), true), 1);

  // the master speaks for Quetoo alone; the Quake2 aliases answer to nobody
  ck_assert_int_eq(query_master("y", true), -1);
  ck_assert_int_eq(query_master("yo", true), -1);
  ck_assert_int_eq(query_master("query", true), -1);

} END_TEST

START_TEST(check_Ms_InfoValue) {
  char val[256];

  const char *status = "\\sv_hostname\\Test\\sv_protocol\\2035\n\\name\\player\\ai\\0\n";

  ck_assert_msg(Ms_InfoValue(status, "sv_protocol", val, sizeof(val)), "Missed sv_protocol");
  ck_assert_str_eq(val, "2035");

  // a key carried by a player line is not the server's to report
  const char *spoofed = "\\sv_hostname\\Test\n\\name\\\\sv_protocol\\2035\n";

  ck_assert_msg(!Ms_InfoValue(spoofed, "sv_protocol", val, sizeof(val)), "Spoofed sv_protocol");

  // but an isolated player line is still searched in full
  ck_assert_msg(Ms_InfoValue("\\name\\player\\ai\\1", "ai", val, sizeof(val)), "Missed ai");
  ck_assert_str_eq(val, "1");

  // so a status response cannot list a server that never named its protocol
  add_validated_server("192.168.1.1", PORT_SERVER, 0);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  *(in_addr_t *) &addr.sin_addr = inet_addr("192.168.1.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_SERVER);

  MasterServer *server = Ms_GetServer(&addr);
  Ms_ParseStatusString(server, spoofed);

  ck_assert_int_eq(server->protocol, 0);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_master");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_Ms_AddServer);
  tcase_add_test(tcase, check_Ms_BlacklistServer);
  tcase_add_test(tcase, check_Ms_GetServers);
  tcase_add_test(tcase, check_Ms_ParseMessage);
  tcase_add_test(tcase, check_Ms_InfoValue);

  Suite *suite = suite_create("check_master");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
