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
#include "collision/entity.h"

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

START_TEST(check_Entity_ToInfoString) {

  const Entity entity = {
    .key = "classname",
    .string = "light",
    .next = &(Entity) {
      .key = "color",
      .string = "1 1 0",
      .next = &(Entity) {
        .key = "origin",
        .string = "128 256 512"
      }
    }
  };

  char *str = Entity_ToInfoString(&entity);

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

START_TEST(check_Entity_LoadAll_longValue) {

  char value[MAX_BSP_ENTITY_VALUE * 2];
  memset(value, 'x', sizeof(value) - 1);
  value[sizeof(value) - 1] = '\0';

  const char *format = "{\n\tname first\n\tai \"%s\"\n}\n{\n\tname second\n\tai \"Gork\"\n}\n";
  const size_t len = strlen(format) + strlen(value) + 1;
  char *string = malloc(len);
  ck_assert_ptr_nonnull(string);
  snprintf(string, len, format, value);

  List *entities = Entity_LoadAll(string);
  free(string);

  ck_assert_int_eq(entities->count, 2);

  const Entity *first = entities->head->element;
  const Entity *second = entities->head->next->element;

  ck_assert_str_eq(Entity_Value(first, "name")->string, "first");
  ck_assert_str_eq(Entity_Value(first, "ai")->string, value);

  ck_assert_str_eq(Entity_Value(second, "name")->string, "second");
  ck_assert_str_eq(Entity_Value(second, "ai")->string, "Gork");

  for (const ListNode *node = entities->head; node; node = node->next) {
    Entity_Free(node->element);
  }

  release(entities);

} END_TEST

START_TEST(check_Entity_LoadAll_longKey) {

  char key[301];
  memset(key, 'k', sizeof(key) - 1);
  key[sizeof(key) - 1] = '\0';

  const char *format = "{\n\tname first\n\t\"%s\" value\n}\n{\n\tname second\n}\n";
  const size_t len = strlen(format) + strlen(key) + 1;
  char *string = malloc(len);
  ck_assert_ptr_nonnull(string);
  snprintf(string, len, format, key);

  List *entities = Entity_LoadAll(string);
  free(string);

  ck_assert_int_eq(entities->count, 2);

  const Entity *first = entities->head->element;
  const Entity *second = entities->head->next->element;

  ck_assert_str_eq(Entity_Value(first, "name")->string, "first");
  ck_assert_str_eq(Entity_Value(first, key)->string, "value");
  ck_assert_str_eq(Entity_Value(second, "name")->string, "second");

  for (const ListNode *node = entities->head; node; node = node->next) {
    Entity_Free(node->element);
  }

  release(entities);

} END_TEST

START_TEST(check_Entity_FromInfoString) {

  const char *info = "classname\\light\\color\\1 1 0\\origin\\128 256 512";

  Entity *entity = Entity_FromInfoString(info);

  ck_assert_str_eq(Entity_Value(entity, "classname")->string, "light");
  ck_assert_str_eq(Entity_Value(entity, "color")->string, "1 1 0");
  ck_assert_str_eq(Entity_Value(entity, "origin")->string, "128 256 512");

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  Suite *suite = suite_create("check_entity");

  {
    TCase *tcase = tcase_create("Entity_ToInfoString");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Entity_ToInfoString);

    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Entity_FromInfoString");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Entity_FromInfoString);
    tcase_add_test(tcase, check_Entity_LoadAll_longValue);
    tcase_add_test(tcase, check_Entity_LoadAll_longKey);

    suite_add_tcase(suite, tcase);
  }

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
