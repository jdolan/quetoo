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

START_TEST(check_Mem_TagMalloc) {

  char *string = Mem_CopyString("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut tincidunt justo nec arcu mattis, quis pretium nisi dapibus. Nam placerat lacinia arcu, eu rhoncus justo ornare quis. Phasellus quis pulvinar turpis. Mauris congue, sem a fringilla facilisis, nisl nibh rutrum dolor, sed sagittis justo nulla vitae velit. Nulla non tincidunt turpis, at laoreet ex. Phasellus ut ante fringilla, tempus dui vitae, sagittis ligula. Curabitur pharetra quam turpis, et fermentum sapien lacinia quis. Curabitur sed efficitur nunc.");

  const size_t len = strlen(string) + 1;

  ck_assert_int_eq(len, Mem_Size());

#define TAG 10

  for (int32_t i = 0; i < 1024; i++) {
    Mem_TagMalloc(1024, TAG);
  }

  ck_assert_int_eq(1024 * 1024 + len, Mem_Size());

  Mem_FreeTag(TAG);

  ck_assert_int_eq(len, Mem_Size());

  ck_assert_str_eq("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut tincidunt justo nec arcu mattis, quis pretium nisi dapibus. Nam placerat lacinia arcu, eu rhoncus justo ornare quis. Phasellus quis pulvinar turpis. Mauris congue, sem a fringilla facilisis, nisl nibh rutrum dolor, sed sagittis justo nulla vitae velit. Nulla non tincidunt turpis, at laoreet ex. Phasellus ut ante fringilla, tempus dui vitae, sagittis ligula. Curabitur pharetra quam turpis, et fermentum sapien lacinia quis. Curabitur sed efficitur nunc.", string);

  Mem_Free(string);

  ck_assert_int_eq(0, Mem_Size());

} END_TEST

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

} END_TEST

START_TEST(check_Mem_LinkMalloc_MultipleChildren) {
  // Test that a parent with many children frees them all correctly
  byte *parent = Mem_Malloc(1);
  
  byte *children[10];
  for (int i = 0; i < 10; i++) {
    children[i] = Mem_LinkMalloc(100, parent);
  }
  
  ck_assert(Mem_Size() == 1 + (100 * 10));
  
  // Free parent should free all children
  Mem_Free(parent);
  
  ck_assert(Mem_Size() == 0);
} END_TEST

START_TEST(check_Mem_LinkMalloc_DeepHierarchy) {
  // Test deep parent->child->grandchild chains
  byte *parent = Mem_Malloc(1);
  byte *child = Mem_LinkMalloc(1, parent);
  byte *grandchild = Mem_LinkMalloc(1, child);
  byte *greatgrandchild = Mem_LinkMalloc(1, grandchild);
  
  ck_assert(Mem_Size() == 4);
  
  // Freeing parent should cascade through all descendants
  Mem_Free(parent);
  
  ck_assert(Mem_Size() == 0);
} END_TEST

START_TEST(check_Mem_TagMalloc_MixedWithLinks) {
  // Test that tag-based freeing works with linked allocations
  byte *parent = Mem_TagMalloc(1, 100);
  byte *child1 = Mem_LinkMalloc(1, parent);
  byte *child2 = Mem_LinkMalloc(1, parent);
  
  // Add some unrelated allocations with different tag
  Mem_TagMalloc(10, 200);
  Mem_TagMalloc(10, 200);
  
  ck_assert(Mem_Size() == 3 + 20);
  
  // Free tag 100 should free parent and its children
  Mem_FreeTag(100);
  
  ck_assert(Mem_Size() == 20);
  
  // Clean up
  Mem_FreeTag(200);
  
  ck_assert(Mem_Size() == 0);
} END_TEST

START_TEST(check_Mem_Realloc_PreservesLinks) {
  // Test that reallocating a parent preserves its children
  byte *parent = Mem_Malloc(10);
  byte *child = Mem_LinkMalloc(5, parent);
  
  ck_assert(Mem_Size() == 15);
  
  // Realloc the parent
  parent = Mem_Realloc(parent, 20);
  
  ck_assert(Mem_Size() == 25);
  
  // Freeing parent should still free child
  Mem_Free(parent);
  
  ck_assert(Mem_Size() == 0);
} END_TEST

START_TEST(check_Mem_Link_Reparenting) {
  // Test that linking a child to a new parent works correctly
  byte *parent1 = Mem_Malloc(1);
  byte *parent2 = Mem_Malloc(1);
  byte *child = Mem_LinkMalloc(1, parent1);
  
  ck_assert(Mem_Size() == 3);
  
  // Reparent child to parent2
  Mem_Link(child, parent2);
  
  // Free parent1 should NOT free child
  Mem_Free(parent1);
  
  ck_assert(Mem_Size() == 2);
  
  // Free parent2 should free child
  Mem_Free(parent2);
  
  ck_assert(Mem_Size() == 0);
} END_TEST

START_TEST(check_Mem_CopyString) {
  char *test = Mem_CopyString("test");

  ck_assert(Mem_Size() == strlen(test) + 1);

  Mem_Free(test);

  ck_assert(Mem_Size() == 0);
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_mem");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_Mem_TagMalloc);
  tcase_add_test(tcase, check_Mem_LinkMalloc);
  tcase_add_test(tcase, check_Mem_LinkMalloc_MultipleChildren);
  tcase_add_test(tcase, check_Mem_LinkMalloc_DeepHierarchy);
  tcase_add_test(tcase, check_Mem_TagMalloc_MixedWithLinks);
  tcase_add_test(tcase, check_Mem_Realloc_PreservesLinks);
  tcase_add_test(tcase, check_Mem_Link_Reparenting);
  tcase_add_test(tcase, check_Mem_CopyString);

  Suite *suite = suite_create("check_mem");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
