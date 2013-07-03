/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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
#include "cvar.h"

/*
 * @brief Setup fixture.
 */
void setup(void) {

	Z_Init();

	Cmd_Init();

	Cvar_Init();

	Thread_Init();
}

/*
 * @brief Teardown fixture.
 */
void teardown(void) {

	Thread_Shutdown();

	Cvar_Shutdown();

	Cmd_Shutdown();

	Z_Shutdown();
}

START_TEST(check_CriticalSection)
	{


	}END_TEST

/*
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_critical_section");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_CriticalSection);

	Suite *suite = suite_create("check_threads");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
