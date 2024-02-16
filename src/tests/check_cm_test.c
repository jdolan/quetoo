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
#include "collision/cm_test.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {
	Mem_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {
	Mem_Shutdown();
}

START_TEST(check_Cm_BoxOnPlaneSide_axial_front) {

	for (float i = -MAX_WORLD_AXIAL; i <= MAX_WORLD_AXIAL; i += 1.f) {
		const cm_bsp_plane_t plane = Cm_Plane(Vec3_Up(), i);

		const vec3_t mins = Vec3(0.f, 0.f, i);
		const vec3_t maxs = Vec3(0.f, 0.f, i + 1.f);

		ck_assert_int_eq(SIDE_FRONT, Cm_BoxOnPlaneSide(Box3(mins, maxs), &plane));
	}

} END_TEST

START_TEST(check_Cm_BoxOnPlaneSide_axial_back) {

	for (float i = -MAX_WORLD_AXIAL; i <= MAX_WORLD_AXIAL; i += 1.f) {
		const cm_bsp_plane_t plane = Cm_Plane(Vec3_Up(), i);

		const vec3_t mins = Vec3(0.f, 0.f, i - 1.f);
		const vec3_t maxs = Vec3(0.f, 0.f, i - ON_EPSILON);

		ck_assert_int_eq(SIDE_BACK, Cm_BoxOnPlaneSide(Box3(mins, maxs), &plane));
	}

} END_TEST

START_TEST(check_Cm_BoxOnPlaneSide_general_front) {

	/*for (float i = -MAX_WORLD_AXIAL; i <= MAX_WORLD_AXIAL; i += 1.f) {
		const cm_bsp_plane_t plane = Cm_Plane(Vec3_Up(), i);

		const vec3_t mins = Vec3(0.f, 0.f, i + SIDE_EPSILON);
		const vec3_t maxs = Vec3(0.f, 0.f, i + SIDE_EPSILON + 1.f);

		ck_assert_int_eq(SIDE_FRONT, Cm_BoxOnPlaneSide(mins, maxs, &plane));
	}*/

} END_TEST

START_TEST(check_Cm_BoxOnPlaneSide_general_back) {

	/*for (float i = -MAX_WORLD_AXIAL; i <= MAX_WORLD_AXIAL; i += 1.f) {
		
		const cm_bsp_plane_t plane = Cm_Plane(Vec3_Normalize(Vec3(1.f, 1.f, 1.f)), i);

		const vec3_t mins = Vec3_Scale(plane.normal, i - SIDE_EPSILON - 1.f);
		const vec3_t maxs = Vec3_Scale(plane.normal, i - SIDE_EPSILON);

		printf("%g\n", i);

		ck_assert_int_eq(SIDE_BACK, Cm_BoxOnPlaneSide(mins, maxs, &plane));
	}*/

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	Suite *suite = suite_create("check_cm_test");

	{
		TCase *tcase = tcase_create("Cm_BoxOnPlaneSide");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Cm_BoxOnPlaneSide_axial_front);
		tcase_add_test(tcase, check_Cm_BoxOnPlaneSide_axial_back);
		tcase_add_test(tcase, check_Cm_BoxOnPlaneSide_general_front);
		tcase_add_test(tcase, check_Cm_BoxOnPlaneSide_general_back);
		suite_add_tcase(suite, tcase);
	}

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
