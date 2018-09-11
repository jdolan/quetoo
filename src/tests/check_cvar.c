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
#include "cmd.h"
#include "cvar.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {

	Mem_Init();

	Fs_Init(FS_NONE);

	Cmd_Init();

	Cvar_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

	Cvar_Shutdown();

	Cmd_Shutdown();

	Fs_Shutdown();

	Mem_Shutdown();
}

START_TEST(check_Cvar_Get) {
	// check that we can create a variable and that its fields are populated correctly
	cvar_t *var = Cvar_Add("var", "3.2", CVAR_ARCHIVE, __func__);

	ck_assert(var != NULL);
	ck_assert_str_eq(var->name, "var");
	ck_assert_str_eq(var->string, "3.2");
	ck_assert_msg(var->integer == 3, "var->integer was %d", var->integer);
	ck_assert_msg(var->value > 3.19 && var->value < 3.21, "var->value was %f", var->value);
	ck_assert_msg(var->flags == CVAR_ARCHIVE, "var->flags was %d", var->flags);
	ck_assert_str_eq(var->description, __func__);

	// check that a subsequent call for the same variable does not modify the value
	// but does modify all meta-data
	cvar_t *var_copy = Cvar_Add("var", "0.5", CVAR_USER_INFO, "Some other description");

	ck_assert(var_copy == var);
	ck_assert_str_eq(var->string, "3.2");
	ck_assert_msg(var->integer == 3, "var->integer was %d", var->integer);
	ck_assert_msg(var->value > 3.19 && var->value < 3.21, "var->value was %f", var->value);
	ck_assert_msg(var->flags == (CVAR_ARCHIVE | CVAR_USER_INFO), "var->flags was %d", var->flags);
	ck_assert_str_eq(var->description, "Some other description");

	// modify the variable and inspect it for changes
	Cmd_ExecuteString("set var 1.4\n");

	ck_assert(var->modified);
	ck_assert_str_eq(var->string, "1.4");
	ck_assert_msg(var->value > 1.39 && var->value < 1.41, "var->value was %f", var->value);
	ck_assert_msg(var->integer == 1, "var->integer was %d", var->integer);

}
END_TEST

START_TEST(check_Cvar_WriteAll) {
	cvar_t *vars[32];
	file_t *file;

	// create a whole mess of variables
	for (uint32_t i = 0; i < lengthof(vars); i++) {
		vars[i] = Cvar_Add(va("var%02d", i), va("%d", i), CVAR_ARCHIVE, NULL);
		vars[i]->modified = false;
	}

	if ((file = Fs_OpenWrite(__func__))) {

		// flush them to a file
		Cvar_WriteAll(file);

		Fs_Close(file);

		if ((file = Fs_OpenRead(__func__))) {
			char line[MAX_STRING_CHARS];

			// each line in the file should correspond to a variable
			for (uint32_t i = 0; i < lengthof(vars); i++) {

				if (Fs_ReadLine(file, line, sizeof(line))) {
					ck_assert_str_eq(line, va("set var%02d \"%d\"", i, i));
				} else {
					ck_abort_msg("var%02d had no line", i);
				}
			}

			Fs_Close(file);
		} else {
			ck_abort_msg("Failed to open %s for reading", __func__);
		}
	} else {
		ck_abort_msg("Failed to open %s for writing", __func__);
	}

}
END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_cvar");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Cvar_Get);
	tcase_add_test(tcase, check_Cvar_WriteAll);

	Suite *suite = suite_create("check_cvar");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
