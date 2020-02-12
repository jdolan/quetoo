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

#include <check.h>

#include "ai_local.h"

/**
 * @brief Setup fixture.
 */
void setup(void) {
	Ai_InitAnn();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {
	Ai_ShutdownAnn();
}

START_TEST(check_Ai_Learn) {

	for (int32_t i = 0; i < 64; i++) {

		g_entity_t player = {
			.class_name = "player",
			.in_use = true,
			.client = &(g_client_t) {
				.ai = false,
				.ps = {
					.pm_state = {
						.type = PM_NORMAL
					}
				}
			}
		};

		const vec3_t origin = { (i - 32) * 64.0, (i - 32) * 64.0, 0.0 };

		player.s.origin = origin;

		pm_cmd_t cmd = {
			.forward = 300
		};

		Ai_Learn(&player, &cmd);

		g_entity_t bot = {
			.class_name = "player",
			.in_use = true,
			.client = &(g_client_t) {
				.ai = true
			}
		};

		bot.s.origin = vec3_add(origin, vec3_scale(vec3_down, Randomc() * 16.0);

		vec3_t dir;
		Ai_Predict(&bot, dir);

		printf("%.2f %.2f %.2f\n", dir[0], dir[1], dir[2]);
		ck_assert(vec3_length(dir) >= 1.0 - FLT_EPSILON);
	}
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	TCase *tcase = tcase_create("check_ai_ann");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Ai_Learn);

	Suite *suite = suite_create("check_ai_ann");
	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}
