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
#include <unistd.h>

#include "shared/shared.h"

START_TEST(check_q_str_has_token) {
  ck_assert(q_str_has_token("dm ctf race", "dm"));
  ck_assert(q_str_has_token("dm ctf race", "ctf"));
  ck_assert(q_str_has_token("dm ctf race", "race"));
  ck_assert(q_str_has_token("  dm\tctf\n", "ctf"));
  ck_assert(!q_str_has_token("tdm", "dm"));
  ck_assert(!q_str_has_token("dm", "tdm"));
  ck_assert(!q_str_has_token("dm ctf", "dm ctf"));
  ck_assert(!q_str_has_token("dm ", ""));
  ck_assert(!q_str_has_token("", "dm"));
  ck_assert(!q_str_has_token(NULL, "dm"));
  ck_assert(!q_str_has_token("dm", NULL));
} END_TEST

START_TEST(check_q_str_ident_equal) {
  ck_assert(q_str_ident_equal("r_swapInterval", "r_swapInterval"));
  ck_assert(q_str_ident_equal("cg_addDecals", "cg_addDecals"));
  ck_assert(q_str_ident_equal("+move_forward", "+moveForward"));
  ck_assert(q_str_ident_equal("noClip", "noClip"));
  ck_assert(q_str_ident_equal("r_swapInterval", "r_swapInterval"));
  ck_assert(q_str_ident_equal("R_SWAP_INTERVAL", "r_swapInterval"));
  ck_assert(q_str_ident_equal("__r__swap__interval__", "r_swapInterval"));
  ck_assert(q_str_ident_equal("", "____"));
  ck_assert(q_str_ident_equal(NULL, NULL));

  ck_assert(!q_str_ident_equal("r_swapInterval", "r_swapIntervals"));
  ck_assert(!q_str_ident_equal("cg_addDecals", "cl_addDecals"));
  ck_assert(!q_str_ident_equal("+moveForward", "-moveForward"));
  ck_assert(!q_str_ident_equal("r_swapInterval", NULL));
  ck_assert(!q_str_ident_equal(NULL, "r_swapInterval"));
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  TCase *tcase = tcase_create("check_shared");
  tcase_add_test(tcase, check_q_str_has_token);
  tcase_add_test(tcase, check_q_str_ident_equal);

  Suite *suite = suite_create("check_shared");
  suite_add_tcase(suite, tcase);

  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_VERBOSE);
  int32_t failed = srunner_ntests_failed(runner);

  srunner_free(runner);
  return failed;
}
