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

#define main Ms_Main
#include "../tools/quetoo-master/main.c"
#undef main

/**
 * @brief Setup fixture.
 */
void setup(void) {

	Mem_Init();

	Fs_Init(FS_NONE);
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

	Fs_Shutdown();

	Mem_Shutdown();
}

START_TEST(check_Ms_AddServer) {
	ck_assert_int_eq(g_list_length(ms_servers), 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	*(in_addr_t *) &addr.sin_addr = inet_addr("192.168.1.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_SERVER);

	Ms_AddServer(&addr);
	ck_assert_int_eq(g_list_length(ms_servers), 1);

	ms_server_t *server = g_list_nth_data(ms_servers, 0);
	ck_assert_msg(server->addr.sin_addr.s_addr == addr.sin_addr.s_addr, "Corrupt server address");

	Ms_AddServer(&addr);
	ck_assert_int_eq(g_list_length(ms_servers), 1);

	*(in_addr_t *) &addr.sin_addr = inet_addr("192.168.1.2");

	Ms_AddServer(&addr);
	ck_assert_int_eq(g_list_length(ms_servers), 2);

	Ms_RemoveServer(&addr);
	ck_assert_int_eq(g_list_length(ms_servers), 1);

	ms_server_t *s = Ms_GetServer(&addr);
	ck_assert_msg(!s, "Server was not NULL");

} END_TEST

START_TEST(check_Ms_BlacklistServer) {
	file_t *f = Fs_OpenAppend("servers-blacklist");
	ck_assert_msg(f != NULL, "Failed to open servers-blacklist");

	const char *test = "192.168.0.*\n";
	int64_t len = Fs_Write(f, (void *) test, 1, strlen(test));

	ck_assert_msg((size_t) len == strlen(test), "Failed to write servers-blacklist");
	ck_assert_msg(Fs_Close(f), "Failed to close servers-blacklist");

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	*(in_addr_t *) &addr.sin_addr = inet_addr("192.168.0.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_SERVER);

	ck_assert_msg(Ms_BlacklistServer(&addr), "Missed %s", inet_ntoa(addr.sin_addr));

	*(in_addr_t *) &addr.sin_addr = inet_addr("127.0.0.1");

	ck_assert_msg(!Ms_BlacklistServer(&addr), "False positive for %s", inet_ntoa(addr.sin_addr));

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

	Suite *suite = suite_create("check_master");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
