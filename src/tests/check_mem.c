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
#include "mem.h"

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

START_TEST(check_Mem_LinkMalloc) {
	byte *parent = Mem_Malloc(1);

	byte *child1 = Mem_LinkMalloc(1, parent);
	byte *child2 = Mem_LinkMalloc(1, parent);

	ck_assert(Mem_Size() == 3);

	Mem_Free(child2);

	ck_assert(Mem_Size() == 2);

	byte *grandchild1 = Mem_Malloc(1);

	ck_assert(Mem_Size() == 3);

	Mem_Link(grandchild1, child1);

	Mem_Free(parent);

	ck_assert(Mem_Size() == 0);

}
END_TEST

START_TEST(check_Mem_CopyString) {
	char *test = Mem_CopyString("test");

	ck_assert(Mem_Size() == strlen(test) + 1);

	Mem_Free(test);

	ck_assert(Mem_Size() == 0);
}
END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_mem");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Mem_LinkMalloc);
	tcase_add_test(tcase, check_Mem_CopyString);

	Suite *suite = suite_create("check_mem");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
