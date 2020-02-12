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
#include "collision/cm_polylib.h"

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

START_TEST(check_Cm_ElementsForWinding_triangle) {

	cm_winding_t *w = Cm_AllocWinding(3);
	w->num_points = 3;

	w->points[0] = vec3(0, 0, 0);
	w->points[1] = vec3(1, 0, 0);
	w->points[2] = vec3(0, 1, 0);

	int32_t elements[(w->num_points - 2) * 3];
	const int32_t num_elements = Cm_ElementsForWinding(w, elements);

	ck_assert_int_eq(3, num_elements);

	ck_assert_int_eq(0, elements[0]);
	ck_assert_int_eq(1, elements[1]);
	ck_assert_int_eq(2, elements[2]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_quad) {

	cm_winding_t *w = Cm_AllocWinding(4);
	w->num_points = 4;

	w->points[0] = vec3(0, 0, 0);
	w->points[1] = vec3(1, 0, 0);
	w->points[2] = vec3(1, 1, 0);
	w->points[3] = vec3(0, 1, 0);

	int32_t elements[(w->num_points - 2) * 3];
	const int32_t num_elements = Cm_ElementsForWinding(w, elements);

	ck_assert_int_eq(6, num_elements);

	ck_assert_int_eq(0, elements[0]);
	ck_assert_int_eq(1, elements[1]);
	ck_assert_int_eq(2, elements[2]);

	ck_assert_int_eq(0, elements[3]);
	ck_assert_int_eq(2, elements[4]);
	ck_assert_int_eq(3, elements[5]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_skinnyQuad) {

	cm_winding_t *w = Cm_AllocWinding(4);
	w->num_points = 4;

	w->points[0] = vec3(0, 0, 0);
	w->points[1] = vec3(128, 0, 0);
	w->points[2] = vec3(128, 1, 0);
	w->points[3] = vec3(0, 1, 0);

	int32_t elements[(w->num_points - 2) * 3];
	const int32_t num_elements = Cm_ElementsForWinding(w, elements);

	ck_assert_int_eq(6, num_elements);

	ck_assert_int_eq(0, elements[0]);
	ck_assert_int_eq(1, elements[1]);
	ck_assert_int_eq(2, elements[2]);

	ck_assert_int_eq(0, elements[3]);
	ck_assert_int_eq(2, elements[4]);
	ck_assert_int_eq(3, elements[5]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_collinearQuad) {

	cm_winding_t *w = Cm_AllocWinding(6);
	w->num_points = 6;

	w->points[0] = vec3(0, 0, 0);
	w->points[1] = vec3(1, 0, 0);
	w->points[2] = vec3(2, 0, 0);

	w->points[3] = vec3(2, 2, 0);
	w->points[4] = vec3(1, 2, 0);
	w->points[5] = vec3(0, 2, 0);

	int32_t elements[(w->num_points - 2) * 3];
	const int32_t num_elements = Cm_ElementsForWinding(w, elements);

	ck_assert_int_eq(12, num_elements);

	ck_assert_int_eq(1, elements[0]);
	ck_assert_int_eq(2, elements[1]);
	ck_assert_int_eq(3, elements[2]);

	ck_assert_int_eq(4, elements[3]);
	ck_assert_int_eq(5, elements[4]);
	ck_assert_int_eq(0, elements[5]);

	ck_assert_int_eq(0, elements[6]);
	ck_assert_int_eq(1, elements[7]);
	ck_assert_int_eq(3, elements[8]);

	ck_assert_int_eq(0, elements[9]);
	ck_assert_int_eq(3, elements[10]);
	ck_assert_int_eq(4, elements[11]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_cornerCase) {

	cm_winding_t *w = Cm_AllocWinding(6);
	w->num_points = 6;

	w->points[0] = vec3(0, 0.000, 0.000);
	w->points[1] = vec3(0, -1.375, 2.000);
	w->points[2] = vec3(0, -20.875, 31.375);
	w->points[3] = vec3(0, -30.750, 30.750);
	w->points[4] = vec3(0, -19.500, 19.500);
	w->points[5] = vec3(0, -12.000, 12.000);

	int32_t elements[(w->num_points - 2) * 3];
	const int32_t num_elements = Cm_ElementsForWinding(w, elements);

	ck_assert_int_eq(12, num_elements);

} END_TEST

START_TEST(check_Cm_TriangleArea) {
	vec3_t a, b, c;

	a = vec3(0, 0, 0);
	b = vec3(0, 1, 0);
	c = vec3(1, 1, 0);

	const float area = Cm_TriangleArea(a, b, c);
	ck_assert(area == 0.5);

} END_TEST

START_TEST(check_Cm_Barycentric) {
	vec3_t a, b, c, p, out;

	a = vec3(0, 0, 0);
	b = vec3(0, 1, 0);
	c = vec3(1, 1, 0);

	p = vec3(0, 0, 0);

	Cm_Barycentric(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 1);
	ck_assert(out[1] == 0);
	ck_assert(out[2] == 0);

	p = vec3(0, 1, 0);

	Cm_Barycentric(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 0);
	ck_assert(out[1] == 1);
	ck_assert(out[2] == 0);

	p = vec3(1, 1, 0);

	Cm_Barycentric(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 0);
	ck_assert(out[1] == 0);
	ck_assert(out[2] == 1);

	p = vec3(0.5, 0.5, 0);

	Cm_Barycentric(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 0.5);
	ck_assert(out[1] == 0);
	ck_assert(out[2] == 0.5);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	Suite *suite = suite_create("check_cm_polylib");

	TCase *tcase = tcase_create("Cm_ElementsForWinding");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Cm_ElementsForWinding_triangle);
	tcase_add_test(tcase, check_Cm_ElementsForWinding_quad);
	tcase_add_test(tcase, check_Cm_ElementsForWinding_skinnyQuad);
	tcase_add_test(tcase, check_Cm_ElementsForWinding_collinearQuad);
	tcase_add_test(tcase, check_Cm_ElementsForWinding_cornerCase);

	suite_add_tcase(suite, tcase);

	tcase = tcase_create("Cm_TriangleArea");
	tcase_add_test(tcase, check_Cm_TriangleArea);

	suite_add_tcase(suite, tcase);

	tcase = tcase_create("Cm_Barycentric");
	tcase_add_test(tcase, check_Cm_Barycentric);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
