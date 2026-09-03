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

#define TEST_FILE "check_filesystem.txt"
#define TEST_FILE_CONTENTS "This is a file written by check_filesystem.\n"

/**
 * @brief Setup fixture.
 */
void setup(void) {

  Mem_Init();

  Fs_Init(FS_AUTO_LOAD_ARCHIVES);

  ck_assert(Fs_SetGame(TEST_GAME, NULL));

  // the read tests run against a file this fixture writes, so that they do not
  // require the game data to be installed
  file_t *f = Fs_OpenWrite(TEST_FILE);
  ck_assert_msg(f != NULL, "Failed to open %s for writing", TEST_FILE);

  const size_t len = strlen(TEST_FILE_CONTENTS);

  ck_assert_msg((size_t) Fs_Write(f, TEST_FILE_CONTENTS, 1, len) == len,
                "Failed to write %s", TEST_FILE);
  ck_assert_msg(Fs_Close(f), "Failed to close %s", TEST_FILE);
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  Fs_Shutdown();

  Mem_Shutdown();
}

START_TEST(check_Fs_OpenRead) {

  ck_assert_msg(Fs_Exists(TEST_FILE), "%s does not exist", TEST_FILE);

  file_t *f = Fs_OpenRead(TEST_FILE);

  ck_assert_msg(f != NULL, "Failed to open %s", TEST_FILE);
  ck_assert_msg(Fs_Close(f), "Failed to close %s", TEST_FILE);

} END_TEST

START_TEST(check_Fs_OpenWrite) {
  file_t *f = Fs_OpenWrite(__func__);

  ck_assert_msg(f != NULL, "Failed to open %s", __func__);

  const char *testing = "testing";
  int64_t len = Fs_Write(f, (void *) testing, 1, strlen(testing));

  ck_assert_msg((size_t) len == strlen(testing), "Failed to write %s", __func__);
  ck_assert_msg(Fs_Close(f), "Failed to close %s", __func__);

} END_TEST

START_TEST(check_Fs_LoadFile) {
  void *buffer;
  int64_t len = Fs_Load(TEST_FILE, &buffer);

  ck_assert_msg(len == (int64_t) strlen(TEST_FILE_CONTENTS), "Failed to load %s", TEST_FILE);
  ck_assert(!strcmp((const char *) buffer, TEST_FILE_CONTENTS));

  Fs_Free(buffer);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_filesystem");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_Fs_OpenRead);
  tcase_add_test(tcase, check_Fs_OpenWrite);
  tcase_add_test(tcase, check_Fs_LoadFile);

  Suite *suite = suite_create("check_filesystem");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
