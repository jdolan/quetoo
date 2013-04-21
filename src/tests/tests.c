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

quake2world_t quake2world;

/**
 * @brief Runs the specified suite, returning the number of tests that failed.
 */
int32_t Test_Run(Suite *suite) {

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_NORMAL);
	int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}

/*
 * @brief Initializes testing facilities.
 */
void Test_Init(int32_t argc, char **argv) {

	memset(&quake2world, 0, sizeof(quake2world));

	Com_InitArgv(argc, argv);
}

/*
 * @brief Shuts down testing facilities.
 */
void Test_Shutdown(void) {

}
