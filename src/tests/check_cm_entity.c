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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "tests.h"
#include "collision/cm_entity.h"

Quetoo quetoo;

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

START_TEST(check_Cm_EntityToInfoString) {

  const CmEntity entity = {
    .key = "classname",
    .string = "light",
    .next = &(CmEntity) {
      .key = "color",
      .string = "1 1 0",
      .next = &(CmEntity) {
        .key = "origin",
        .string = "128 256 512"
      }
    }
  };

  char *str = Cm_EntityToInfoString(&entity);

  char value[MAX_INFO_STRING_VALUE];

  ck_assert_int_eq(InfoString_Get(str, "classname", value, sizeof(value)), 5);
  ck_assert_str_eq(value, "light");

  ck_assert_int_eq(InfoString_Get(str, "color", value, sizeof(value)), 5);
  ck_assert_str_eq(value, "1 1 0");

  ck_assert_int_eq(InfoString_Get(str, "origin", value, sizeof(value)), 11);
  ck_assert_str_eq(value, "128 256 512");

  ck_assert_int_eq(InfoString_Get(str, "nope", value, sizeof(value)), -1);
  ck_assert_str_eq(value, "");

} END_TEST

START_TEST(check_Cm_EntityFromInfoString) {

  const char *info = "classname\\light\\color\\1 1 0\\origin\\128 256 512";

  CmEntity *entity = Cm_EntityFromInfoString(info);

  ck_assert_str_eq(Cm_EntityValue(entity, "classname")->string, "light");
  ck_assert_str_eq(Cm_EntityValue(entity, "color")->string, "1 1 0");
  ck_assert_str_eq(Cm_EntityValue(entity, "origin")->string, "128 256 512");

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  Suite *suite = suite_create("check_cm_entity");

  {
    TCase *tcase = tcase_create("Cm_EntityToInfoString");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_EntityToInfoString);

    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Cm_EntityFromInfoString");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_EntityFromInfoString);

    suite_add_tcase(suite, tcase);
  }

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
