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

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {

	Mem_Init();

	Fs_Init(FS_NONE);

	Cmd_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

	Cmd_Shutdown();

	Fs_Shutdown();

	Mem_Shutdown();
}

static void Cmd1(void) {
}

static void Cmd2(void) {
}

START_TEST(check_Cmd_RemoveAll) {
	cmd_t *cmd1 = Cmd_Add("cmd1", Cmd1, (CMD_SYSTEM | CMD_SERVER), NULL);
	cmd_t *cmd2 = Cmd_Add("cmd2", Cmd2, CMD_GAME, NULL);

	ck_assert(cmd1 != NULL);
	ck_assert(cmd2 != NULL);

	Cmd_RemoveAll(CMD_GAME);

	cmd1 = Cmd_Get("cmd1");
	cmd2 = Cmd_Get("cmd2");

	ck_assert(cmd1 != NULL);
	ck_assert(cmd2 == NULL);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_cmd");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Cmd_RemoveAll);

	Suite *suite = suite_create("check_cmd");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
