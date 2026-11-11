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

Quetoo quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {

  Mem_Init();

  Fs_Init(FS_NONE);

  Cmd_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  Cmd_Shutdown();

  Fs_Shutdown();

  Mem_Shutdown();
}

static void Cmd1(void) {
}

static void Cmd2(void) {
}

START_TEST(check_Cmd_RemoveAll) {
  Cmd *cmd1 = Cmd_Add("cmd1", Cmd1, (CMD_SYSTEM | CMD_SERVER), NULL);
  Cmd *cmd2 = Cmd_Add("cmd2", Cmd2, CMD_GAME, NULL);

  ck_assert(cmd1 != NULL);
  ck_assert(cmd2 != NULL);

  Cmd_RemoveAll(CMD_GAME);

  cmd1 = Cmd_Get("cmd1");
  cmd2 = Cmd_Get("cmd2");

  ck_assert(cmd1 != NULL);
  ck_assert(cmd2 == NULL);

} END_TEST

/**
 * @brief Records what the stub commands below were called with.
 */
static struct {
  int32_t sayCalls;
  int32_t sayArgc;
  char sayArg1[MAX_STRING_CHARS];
  int32_t injectedCalls;
} chat;

static void Test_Say(void) {
  chat.sayCalls++;
  chat.sayArgc = Cmd_Argc();
  Str_Copy(chat.sayArg1, Cmd_Argv(1), sizeof(chat.sayArg1));
}

static void Test_Injected(void) {
  chat.injectedCalls++;
}

/**
 * @brief Registers the stubs and runs the given text through the command buffer.
 */
static void Test_ExecuteChat(const char *text) {

  memset(&chat, 0, sizeof(chat));

  Cmd_Add("say", Test_Say, 0, NULL);
  Cmd_Add("injected", Test_Injected, 0, NULL);

  Cbuf_AddText(text);
  Cbuf_Execute();
}

/**
 * @brief A semicolon inside a quoted chat message must not start a second command (#1164).
 */
START_TEST(check_Cbuf_Execute_quotedSemicolon) {

  Test_ExecuteChat("say \"Hello; injected^7\"\n");

  ck_assert_int_eq(1, chat.sayCalls);
  ck_assert_int_eq(2, chat.sayArgc);
  ck_assert_int_eq(0, chat.injectedCalls);
  ck_assert(strstr(chat.sayArg1, "Hello; injected") != NULL);

} END_TEST

/**
 * @brief Control case: an unquoted semicolon does separate commands, which is why chat
 * must be quoted before it is passed to the command buffer.
 */
START_TEST(check_Cbuf_Execute_unquotedSemicolon) {

  Test_ExecuteChat("say Hello; injected\n");

  ck_assert_int_eq(1, chat.sayCalls);
  ck_assert_int_eq(1, chat.injectedCalls);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_cmd");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_Cmd_RemoveAll);
  tcase_add_test(tcase, check_Cbuf_Execute_quotedSemicolon);
  tcase_add_test(tcase, check_Cbuf_Execute_unquotedSemicolon);

  Suite *suite = suite_create("check_cmd");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
