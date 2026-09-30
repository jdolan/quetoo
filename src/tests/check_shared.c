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

#include <check.h>
#include <unistd.h>

#include "shared/shared.h"

START_TEST(check_Str_HasToken) {
  ck_assert(Str_HasToken("dm ctf race", "dm"));
  ck_assert(Str_HasToken("dm ctf race", "ctf"));
  ck_assert(Str_HasToken("dm ctf race", "race"));
  ck_assert(Str_HasToken("  dm\tctf\n", "ctf"));
  ck_assert(!Str_HasToken("tdm", "dm"));
  ck_assert(!Str_HasToken("dm", "tdm"));
  ck_assert(!Str_HasToken("dm ctf", "dm ctf"));
  ck_assert(!Str_HasToken("dm ", ""));
  ck_assert(!Str_HasToken("", "dm"));
  ck_assert(!Str_HasToken(NULL, "dm"));
  ck_assert(!Str_HasToken("dm", NULL));
} END_TEST

START_TEST(check_InfoString_Get) {
  const char *info =
    "\\g_gameplayMode\\deathmatch"
    "\\g_movementMode\\quetoo"
    "\\sv_guid\\57491bc7-1480-41e0-836d-31bc08e1b2d5"
    "\\sv_map\\pits"
    "\\sv_maxClients\\16"
    "\\g_motd\\";

  char value[MAX_INFO_STRING_VALUE];

  ck_assert_int_eq(InfoString_Get(info, "sv_map", value, sizeof(value)), 4);
  ck_assert_str_eq(value, "pits");

  // a key whose name is a prefix of another must not match it
  ck_assert_int_eq(InfoString_Get(info, "sv_maxClients", value, sizeof(value)), 2);
  ck_assert_str_eq(value, "16");

  // a key present with an empty value is distinct from a key that is absent
  ck_assert_int_eq(InfoString_Get(info, "g_motd", value, sizeof(value)), 0);
  ck_assert_str_eq(value, "");

  ck_assert_int_eq(InfoString_Get(info, "sv_hostname", value, sizeof(value)), -1);
  ck_assert_str_eq(value, "");

  // the result is not invalidated by any number of later lookups
  char guid[MAX_INFO_STRING_VALUE];
  ck_assert_int_eq(InfoString_Get(info, "sv_guid", guid, sizeof(guid)), 36);
  InfoString_Get(info, "g_gameplayMode", value, sizeof(value));
  InfoString_Get(info, "g_movementMode", value, sizeof(value));
  InfoString_Get(info, "sv_maxClients", value, sizeof(value));
  ck_assert_str_eq(guid, "57491bc7-1480-41e0-836d-31bc08e1b2d5");

  // the value is truncated to the buffer, and always terminated
  char small[5];
  ck_assert_int_eq(InfoString_Get(info, "sv_guid", small, sizeof(small)), 4);
  ck_assert_str_eq(small, "5749");
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  TCase *tcase = tcase_create("check_shared");
  tcase_add_test(tcase, check_Str_HasToken);
  tcase_add_test(tcase, check_InfoString_Get);

  Suite *suite = suite_create("check_shared");
  suite_add_tcase(suite, tcase);

  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_VERBOSE);
  int32_t failed = srunner_ntests_failed(runner);

  srunner_free(runner);
  return failed;
}
