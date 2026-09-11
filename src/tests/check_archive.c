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

#include "tests.h"

#include "common/archive.h"

quetoo_t quetoo;

#define DEST "/tmp/quetoo-archive"

START_TEST(check_archive_safe_paths) {

  char out[MAX_OS_PATH];

  ck_assert(Archive_SafePath(DEST, "bin/quetoo", out, sizeof(out)));
  ck_assert_str_eq(DEST "/bin/quetoo", out);

  ck_assert(Archive_SafePath(DEST, "default/maps/edge.bsp", out, sizeof(out)));
  ck_assert_str_eq(DEST "/default/maps/edge.bsp", out);

  ck_assert(Archive_SafePath(DEST, "a.b..c/d", out, sizeof(out)));
  ck_assert_str_eq(DEST "/a.b..c/d", out);

  ck_assert(Archive_SafePath(DEST, "lib\\default\\cgame.dll", out, sizeof(out)));
  ck_assert_str_eq(DEST "/lib/default/cgame.dll", out);

} END_TEST

START_TEST(check_archive_traversal) {

  char out[MAX_OS_PATH];

  ck_assert(!Archive_SafePath(DEST, "../evil", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "a/../../evil", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "a/..", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "..", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "..\\evil", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "a\\..\\..\\evil", out, sizeof(out)));

} END_TEST

START_TEST(check_archive_absolute) {

  char out[MAX_OS_PATH];

  ck_assert(!Archive_SafePath(DEST, "/etc/cron.d/evil", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "\\windows\\system32\\evil.dll", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "C:\\windows\\evil.dll", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "c:/windows/evil.dll", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "", out, sizeof(out)));

} END_TEST

START_TEST(check_archive_reserved) {

  char out[MAX_OS_PATH];

  ck_assert(!Archive_SafePath(DEST, "CON", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "nul.txt", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "a/COM1/b", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "LPT9.dat", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "trailing.", out, sizeof(out)));
  ck_assert(!Archive_SafePath(DEST, "trailing ", out, sizeof(out)));

  ck_assert(Archive_SafePath(DEST, "console.cfg", out, sizeof(out)));
  ck_assert(Archive_SafePath(DEST, "coma/b", out, sizeof(out)));

} END_TEST

START_TEST(check_archive_overflow) {

  char out[64];

  ck_assert(!Archive_SafePath(DEST, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", out, sizeof(out)));

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  TCase *tcase = tcase_create("check_archive");

  tcase_add_test(tcase, check_archive_safe_paths);
  tcase_add_test(tcase, check_archive_traversal);
  tcase_add_test(tcase, check_archive_absolute);
  tcase_add_test(tcase, check_archive_reserved);
  tcase_add_test(tcase, check_archive_overflow);

  Suite *suite = suite_create("check_archive");
  suite_add_tcase(suite, tcase);

  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_VERBOSE);
  int32_t failed = srunner_ntests_failed(runner);

  srunner_free(runner);
  return failed;
}
