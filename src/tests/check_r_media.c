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
#include "r_local.h"

/*
 * @brief Setup fixture.
 */
void setup(void) {

	Z_Init();

	R_InitMedia();
}

/*
 * @brief Teardown fixture.
 */
void teardown(void) {

	R_ShutdownMedia();

	Z_Shutdown();
}

START_TEST(check_R_RegisterMedia)
	{
		R_BeginLoading();

		r_media_t *parent1 = R_MallocMedia("parent1", sizeof(r_media_t));
		R_RegisterMedia(parent1);

		ck_assert_msg(R_FindMedia("parent1") == parent1, "Failed to find parent1");

		r_media_t *child1 = R_MallocMedia("child1", sizeof(r_media_t));
		R_RegisterDependency(parent1, child1);

		ck_assert_msg(R_FindMedia("child1") == child1, "Failed to find child1");

		r_media_t *grandchild1 = R_MallocMedia("grandchild1", sizeof(r_media_t));
		R_RegisterDependency(child1, grandchild1);

		R_FreeMedia();

		ck_assert_msg(R_FindMedia("parent1") == parent1, "Erroneously freed parent1");
		ck_assert_msg(R_FindMedia("child1") == child1, "Erroneously freed child1");
		ck_assert_msg(R_FindMedia("grandchild1") == grandchild1, "Erroneously freed grandchild1");

		R_BeginLoading();

		ck_assert(R_FindMedia("parent1") == parent1);
		ck_assert_msg(child1->seed == parent1->seed, "Dependency child1 not retained");
		ck_assert_msg(grandchild1->seed == parent1->seed, "Dependency grandchild1 not retained");

	}END_TEST

/*
 * @brief Test entry point.
 */
int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {

	Test_Init();

	TCase *tcase = tcase_create("check_r_media");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_R_RegisterMedia);

	Suite *suite = suite_create("check_r_media");
	suite_add_tcase(suite, tcase);

	int failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
