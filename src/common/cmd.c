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

#include <Objectively/HashTable.h>
#include <Objectively/PointerArray.h>

#include "console.h"
#include "filesystem.h"

typedef struct CmdArgs {
  int32_t argc;
  char argv[MAX_STRING_TOKENS][MAX_TOKEN_CHARS];
  char args[MAX_STRING_CHARS];
} CmdArgs;

#define CBUF_CHARS 65536

typedef struct CmdState {
  HashTable *commands;

  MemBuf buf;
  char buffers[2][CBUF_CHARS];

  CmdArgs args;

  bool wait; // commands may be deferred one frame

  int32_t aliasLoopCount;
} CmdState;

static CmdState cmdState;

#define MAX_ALIAS_LOOP_COUNT 8

/**
 * @brief Adds command text at the end of the buffer
 */
void Cbuf_AddText(const char *text) {

  const size_t l = q_strlen(text);

  if (cmdState.buf.size + l >= cmdState.buf.maxSize) {
    Com_Warn("Overflow\n");
    return;
  }

  Mem_WriteBuffer(&cmdState.buf, text, l);
}

/**
 * @brief Inserts command text at the beginning of the buffer.
 */
void Cbuf_InsertText(const char *text) {

  if (text  && q_strlen(text)) {
    void *temp;

    // copy off any commands still remaining in the exec buffer
    const size_t size = cmdState.buf.size;
    if (size) {
      temp = Mem_TagMalloc(size, MEM_TAG_CMD);
      memcpy(temp, cmdState.buf.data, size);
      Mem_ClearBuffer(&cmdState.buf);
    } else {
      temp = NULL; // shut up compiler
    }

    // add the entire text of the file
    Cbuf_AddText(text);

    // add the copied off data
    if (size) {
      Mem_WriteBuffer(&cmdState.buf, temp, size);
      Mem_Free(temp);
    }
  }
}

/**
 * @brief Copies the current command buffer to the deferred slot and clears the primary buffer.
 */
void Cbuf_CopyToDefer(void) {

  memcpy(cmdState.buffers[1], cmdState.buffers[0], cmdState.buf.size);

  memset(cmdState.buffers[0], 0, sizeof(cmdState.buffers[0]));

  cmdState.buf.size = 0;
}

/**
 * @brief Inserts the contents of the deferred buffer at the front of the command buffer.
 */
void Cbuf_InsertFromDefer(void) {

  Cbuf_InsertText(cmdState.buffers[1]);

  memset(cmdState.buffers[1], 0, sizeof(cmdState.buffers[1]));
}

/**
 * @brief Executes the pending command buffer.
 */
void Cbuf_Execute(void) {

  cmdState.aliasLoopCount = 0; // don't allow infinite alias loops

  while (cmdState.buf.size) {

    // read a single command line from the buffer
    char line[sizeof(cmdState.args.args)] = "";

    // find a \n or; line break
    char *text = (char *) cmdState.buf.data;

    uint32_t i, quotes = 0;
    for (i = 0; i < cmdState.buf.size; i++) {
      if (text[i] == '"') {
        quotes++;
      }
      if (!(quotes & 1) && text[i] == ';') {
        break; // don't break if inside a quoted string
      }
      if (text[i] == '\n') {
        break;
      }
    }

    if (i >= sizeof(line)) {
      Com_Warn("Command exceeded %" PRIuPTR " chars, discarded\n", sizeof(line));
    } else {
      q_strlcpy(line, text, i + 1);
    }

    // delete the text from the command buffer and move remaining commands down
    // this is necessary because commands (exec, alias) can insert data at the
    // beginning of the text buffer

    if (i == cmdState.buf.size) {
      cmdState.buf.size = 0;
    } else {
      i++;

      cmdState.buf.size = cmdState.buf.size - i;
      memmove(text, text + i, cmdState.buf.size);
    }

    // execute the command linequit

    Cmd_ExecuteString(line);

    // skip out while text still remains in buffer, leaving it for next frame
    if (cmdState.wait) {
      cmdState.wait = false;
      break;
    }
  }
}

/**
 * @return The command argument count.
 */
int32_t Cmd_Argc(void) {
  return cmdState.args.argc;
}

/**
 * @return The command argument at the specified index.
 */
const char *Cmd_Argv(int32_t arg) {
  if (arg >= cmdState.args.argc) {
    return "";
  }
  return cmdState.args.argv[arg];
}

/**
 * @return A single string containing all command arguments.
 */
const char *Cmd_Args(void) {
  return cmdState.args.args;
}

/**
 * @brief Parses the given string into command line tokens.
 */
void Cmd_TokenizeString(const char *text) {

  // clear the command state from the last string
  memset(&cmdState.args, 0, sizeof(cmdState.args));

  if (!text) {
    return;
  }

  // prevent overflows
  if (q_strlen(text) >= MAX_STRING_CHARS) {
    Com_Warn("MAX_STRING_CHARS exceeded\n");
    return;
  }

  Parser parser = Parse_Init(text, PARSER_DEFAULT);

  while (true) {
    // stop after we've exhausted our token buffer
    if (cmdState.args.argc == MAX_STRING_TOKENS) {
      Com_Warn("MAX_STRING_TOKENS exceeded\n");
      return;
    }

    // set cmdState.args to everything after the command name
    if (cmdState.args.argc == 1) {
      q_strlcpy(cmdState.args.args, parser.position.ptr + 1, MAX_STRING_CHARS);

      // strip off any trailing whitespace
      size_t l = q_strlen(cmdState.args.args);
      if (l > 0) {
        char *c = &cmdState.args.args[l - 1];

        while (*c <= ' ') {
          *c-- = '\0';
        }
      }
    }

    if (!Parse_Token(&parser, PARSE_NO_WRAP | PARSE_COPY_QUOTED_LITERALS, cmdState.args.argv[cmdState.args.argc], MAX_TOKEN_CHARS)) { // we're done
      return;
    }

    // expand console variables
    if (*cmdState.args.argv[cmdState.args.argc] == '$' && q_strcmp(cmdState.args.argv[0], "alias")) {
      const char *c = Cvar_GetString(cmdState.args.argv[cmdState.args.argc] + 1);
      q_strlcpy(cmdState.args.argv[cmdState.args.argc], c, MAX_TOKEN_CHARS);
    }

    cmdState.args.argc++;
  }
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
typedef struct {
  const char *name;
  Cmd *cmd;
} CmdLegacyCtx;

/**
 * @brief Finds a command whose name matches but for case and underscores.
 */
static void Cmd_Legacy_enumerate(const HashTable *table, ident key, ident value, ident data) {
  CmdLegacyCtx *ctx = data;

  if (ctx->cmd) {
    return;
  }

  const List *list = value;
  for (const ListNode *node = list->head; node; node = node->next) {
    Cmd *cmd = node->element;
    if (q_str_ident_equal(cmd->name, ctx->name)) {
      ctx->cmd = cmd;
      return;
    }
  }
}

static Cmd *Cmd_Get_(const char *name, const bool caseSensitive) {

  if (cmdState.commands) {
    List *list = $(cmdState.commands, get, (void *) name);

    if (list) {
      if (list->count == 1) { // only 1 entry, return it
        Cmd *cmd = list->head->element;

        if (!caseSensitive || q_strcmp(cmd->name, name) == 0) {
          return cmd;
        }
      } else {
        // only return the exact match
        for (const ListNode *node = list->head; node; node = node->next) {
          Cmd *cmd = node->element;

          if (!q_strcmp(cmd->name, name)) {
            return cmd;
          }
        }
      }
    }
  }

  CmdLegacyCtx ctx = { .name = name };
  $(cmdState.commands, enumerate, Cmd_Legacy_enumerate, &ctx);

  if (ctx.cmd) {
    Com_Warn("%s is now %s\n", name, ctx.cmd->name);
  }

  return ctx.cmd;
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
Cmd *Cmd_Get(const char *name) {
  return Cmd_Get_(name, false);
}

static Order Cmd_Enumerate_comparator(const ident a, const ident b) {
  const int32_t cmp = q_strcasecmp(((const Cmd *) a)->name, ((const Cmd *) b)->name);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

typedef struct {
  PointerArray *cmds;
} CmdEnumerateCtx;

static void Cmd_Enumerate_collect(const HashTable *table, ident key, ident value, ident data) {
  CmdEnumerateCtx *ctx = data;
  const List *list = value;
  for (const ListNode *node = list->head; node; node = node->next) {
    $(ctx->cmds, add, node->element);
  }
}

/**
 * @brief Enumerates all known commands with the given function.
 */
void Cmd_Enumerate(Cmd_Enumerator func, void *data) {
  CmdEnumerateCtx ctx = {
    .cmds = $(alloc(PointerArray), init),
  };

  $(cmdState.commands, enumerate, Cmd_Enumerate_collect, &ctx);
  $(ctx.cmds, sort, Cmd_Enumerate_comparator);

  for (size_t i = 0; i < ctx.cmds->count; i++) {
    func((Cmd *) $(ctx.cmds, get, i), data);
  }

  release(ctx.cmds);
}

/**
 * @brief Adds the specified command, bound to the given function.
 */
Cmd *Cmd_Add(const char *name, CmdExecuteFunc function, uint32_t flags,
               const char *description) {
  Cmd *cmd;

  if (Cvar_Get(name)) {
    Com_Debug(DEBUG_CONSOLE, "%s already defined as a var\n", name);
    return NULL;
  }

  if ((cmd = Cmd_Get_(name, true))) {
    Com_Debug(DEBUG_CONSOLE, "%s already defined\n", name);
    return cmd;
  }

  cmd = Mem_TagMalloc(sizeof(*cmd), MEM_TAG_CMD);

  cmd->name = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CMD), cmd);
  cmd->Execute = function;
  cmd->flags = flags;

  if (description) {
    cmd->description = Mem_Link(Mem_TagCopyString(description, MEM_TAG_CMD), cmd);
  }

  void *key = (void *) cmd->name;
  List *list = $(cmdState.commands, get, key);

  if (!list) {
    list = $(alloc(List), init);
    list->destroy = Mem_Free;
    $(cmdState.commands, set, key, list);
  }

  $(list, prepend, cmd);
  return cmd;
}

/**
 * @brief Assign the specified autocomplete function to the given command.
 */
void Cmd_SetAutocomplete(Cmd *cmd, AutocompleteFunc autocomplete) {
  cmd->Autocomplete = autocomplete;
}

/**
 * @brief Adds the specified alias command, bound to the given commands string.
 */
static Cmd *Cmd_Alias(const char *name, const char *commands) {
  Cmd *cmd;

  if (Cvar_Get(name)) {
    Com_Debug(DEBUG_CONSOLE, "%s already defined as a var\n", name);
    return NULL;
  }

  if ((cmd = Cmd_Get_(name, true))) {
    Com_Debug(DEBUG_CONSOLE, "%s already defined\n", name);
    return cmd;
  }

  cmd = Mem_TagMalloc(sizeof(*cmd), MEM_TAG_CMD);

  cmd->name = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CMD), cmd);
  cmd->commands = Mem_Link(Mem_TagCopyString(commands, MEM_TAG_CMD), cmd);

  void *key = (void *) cmd->name;
  List *list = $(cmdState.commands, get, key);

  if (!list) {
    list = $(alloc(List), init);
    list->destroy = Mem_Free;
    $(cmdState.commands, set, key, list);
  }

  $(list, prepend, cmd);
  return cmd;
}

/**
 * @brief Removes the specified command from its backing list.
 * @note Disables the list destroy callback so the caller controls cmd lifetime.
 */
static List *Cmd_RemovePtr_(Cmd *cmd) {
  List *list = $(cmdState.commands, get, (void *) cmd->name);

  ListNode *node = $(list, nodeForElement, cmd);
  if (!node) {
    Com_Error(ERROR_FATAL, "Missing command: %s\n", cmd->name);
  }

  // Disable destroy so we control when cmd is freed
  Consumer saved = list->destroy;
  list->destroy = NULL;
  $(list, removeNode, node);
  list->destroy = saved;

  return list;
}

/**
 * @brief Removes the specified command.
 */
void Cmd_Remove(const char *name) {
  Cmd *cmd = Cmd_Get_(name, true);

  if (cmd) {
    List *list = Cmd_RemovePtr_(cmd);

    if (!list->count) {
      $(cmdState.commands, remove, (void *) name);
      release(list);
    }

    Mem_Free(cmd);
  }
}

typedef struct {
  uint32_t flags;
  List *cmds;
} CmdRemoveAllCtx;

static void Cmd_RemoveAll_collect(const HashTable *table, ident key, ident value, ident data) {
  CmdRemoveAllCtx *ctx = data;
  const List *list = value;
  for (const ListNode *node = list->head; node; node = node->next) {
    Cmd *cmd = node->element;
    if (cmd->flags & ctx->flags) {
      $(ctx->cmds, append, cmd);
    }
  }
}

/**
 * @brief Removes all commands which match the specified flags mask.
 */
void Cmd_RemoveAll(uint32_t flags) {
  CmdRemoveAllCtx ctx = {
    .flags = flags,
    .cmds = $(alloc(List), init),
  };

  $(cmdState.commands, enumerate, Cmd_RemoveAll_collect, &ctx);

  for (const ListNode *node = ctx.cmds->head; node; node = node->next) {
    Cmd *cmd = node->element;
    List *list = Cmd_RemovePtr_(cmd);

    if (!list->count) {
      $(cmdState.commands, remove, (void *) cmd->name);
      release(list);
    }

    Mem_Free(cmd);
  }

  release(ctx.cmds);
}

/**
 * @brief Stringify a command. This memory is temporary.
 */
static const char *Cmd_Stringify(const Cmd *cmd) {
  static char buffer[MAX_STRING_CHARS];
  buffer[0] = '\0';

  if (cmd->Execute) {
    q_strlcat(buffer, va("^1%s^7", cmd->name), sizeof(buffer));

    if (cmd->description) {
      q_strlcat(buffer, va("\n\t%s", cmd->description), sizeof(buffer));
    }
  } else if (cmd->commands) {
    q_strlcat(buffer, va("^3%s^7\n\t%s", cmd->name, cmd->commands), sizeof(buffer));
  } else {
    q_strlcat(buffer, va("^3%s^7", cmd->name), sizeof(buffer));
  }

  return buffer;
}

static char cmdCompletePattern[MAX_STRING_CHARS];

/**
 * @brief Enumeration helper for `Cmd_CompleteCommand`.
 */
static void Cmd_CompleteCommand_enumerate(Cmd *cmd, void *data) {
  List *matches = data;

  if (GlobMatch(cmdCompletePattern, cmd->name, GLOB_CASE_INSENSITIVE)) {
    Con_AutocompleteMatch(matches, cmd->name, Cmd_Stringify(cmd));
  }
}

/**
 * @brief Console completion for commands and aliases.
 */
void Cmd_CompleteCommand(const char *pattern, List *matches) {
  q_strlcpy(cmdCompletePattern, pattern, sizeof(cmdCompletePattern));
  Cmd_Enumerate(Cmd_CompleteCommand_enumerate, matches);
}

/**
 * @brief A complete command line has been parsed, so try to execute it
 */
void Cmd_ExecuteString(const char *text) {
  Cmd *cmd;

  Cmd_TokenizeString(text);

  if (!Cmd_Argc()) {
    return;
  }

  // execute the command line
  if ((cmd = Cmd_Get(Cmd_Argv(0)))) {
    if (cmd->Execute) {
      cmd->Execute();
    } else if (cmd->commands) {
      if (++cmdState.aliasLoopCount == MAX_ALIAS_LOOP_COUNT) {
        Com_Warn("ALIAS_LOOP_COUNT\n");
      } else {
        Cbuf_AddText(cmd->commands);
      }
    } else if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer) {
      Cmd_ForwardToServer();
    }
    return;
  }

  // check cvars
  if (Cvar_Command()) {
    return;
  }

  // send it as a server command if we are connected
  if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer) {
    Cmd_ForwardToServer();
  }
}

/**
 * @brief Enumeration helper for `Cmd_Alias_f`.
 */
static void Cmd_Alias_f_enumerate(Cmd *cmd, void *data) {

  if (cmd->commands) {
    Com_Print("%s: %s\n", cmd->name, cmd->commands);
  }
}

/**
 * @brief Creates a new command that executes a command string (possibly ; separated)
 */
static void Cmd_Alias_f(void) {
  char cmd[MAX_STRING_CHARS];

  if (Cmd_Argc() == 1) {
    Cmd_Enumerate(Cmd_Alias_f_enumerate, NULL);
    return;
  }

  if (Cmd_Argc() < 3) {
    Com_Print("Usage: %s <commands>", Cmd_Argv(0));
    return;
  }

  if (Cvar_Get(Cmd_Argv(1))) {
    Com_Print("%s is a variable\n", Cmd_Argv(1));
    return;
  }

  if (Cmd_Get_(Cmd_Argv(1), true)) {
    Com_Print("%s is a command\n", Cmd_Argv(1));
    return;
  }

  cmd[0] = '\0';
  for (int32_t i = 2; i < Cmd_Argc(); i++) {
    q_strlcat(cmd, Cmd_Argv(i), sizeof(cmd));
    if (i != (Cmd_Argc() - 1)) {
      q_strlcat(cmd, " ", sizeof(cmd));
    }
  }
  q_strlcat(cmd, "\n", sizeof(cmd));

  Cmd_Alias(Cmd_Argv(1), cmd);
}

typedef struct {
  PointerArray *strs;
} CmdListCtx;

static void Cmd_List_f_enumerate(Cmd *cmd, void *data) {
  CmdListCtx *ctx = data;
  $(ctx->strs, add, q_strdup(Cmd_Stringify(cmd)));
}

static Order Cmd_List_sortfn(const ident a, const ident b) {
  const int32_t cmp = q_strcolorcmp((const char *) a, (const char *) b);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Lists all known commands at the console.
 */
static void Cmd_List_f(void) {
  CmdListCtx ctx = {
    .strs = $(alloc(PointerArray), initWithDestroy, free),
  };

  Cmd_Enumerate(Cmd_List_f_enumerate, &ctx);
  $(ctx.strs, sort, Cmd_List_sortfn);

  for (size_t i = 0; i < ctx.strs->count; i++) {
    Com_Print("%s\n", (char *) $(ctx.strs, get, i));
  }

  release(ctx.strs);
}

/**
 * @brief Demo command autocompletion.
 */
static void Cmd_Exec_Autocomplete_f(const uint32_t argi, List *matches) {
  const char *pattern = va("%s*.cfg", Cmd_Argv(argi));
  Fs_CompleteFile(pattern, matches);
}

/**
 * @brief Executes the specified script file (e.g. `autoexec.cfg`).
 */
static void Cmd_Exec_f(void) {
  char path[MAX_QPATH];
  void *buffer;

  if (Cmd_Argc() != 2) {
    Com_Print("Usage: %s <filename> : execute a script file\n", Cmd_Argv(0));
    return;
  }

  q_strlcpy(path, Cmd_Argv(1), sizeof(path));
  const size_t plen = q_strlen(path);
  if (plen < 4 || q_strcmp(path + plen - 4, ".cfg") != 0) {
    q_strlcat(path, ".cfg", sizeof(path));
  }

  if (Fs_Load(path, &buffer) == -1) {
    Com_Print("Couldn't exec %s\n", Cmd_Argv(1));
    return;
  }

  Cbuf_InsertText((const char *) buffer);
  Fs_Free(buffer);
}

/**
 * @brief Prints the remaining command arguments to the console.
 */
static void Cmd_Echo_f(void) {
  int32_t i;

  for (i = 1; i < Cmd_Argc(); i++) {
    Com_Print("%s ", Cmd_Argv(i));
  }

  Com_Print("\n");
}

/**
 * @brief Causes execution of the remainder of the command buffer to be delayed until
 * next frame. This allows commands like: bind g "+attack; wait; -attack;"
 */
static void Cmd_Wait_f(void) {
  cmdState.wait = true;
}

typedef struct {
  PointerArray *lists;
} CmdShutdownCtx;

static void Cmd_Shutdown_collect(const HashTable *table, ident key, ident value, ident data) {
  $(((PointerArray *) data), add, value);
}

/**
 * @brief Initializes the command subsystem.
 */
void Cmd_Init(void) {

  memset(&cmdState, 0, sizeof(cmdState));

  cmdState.commands = $(alloc(HashTable), init, HashTableHashStri, HashTableEqualStri);

  Mem_InitBuffer(&cmdState.buf, (byte *) cmdState.buffers[0], sizeof(cmdState.buffers[0]));

  Cmd_Add("cmdList", Cmd_List_f, 0, NULL);
  Cmd *execCmd = Cmd_Add("exec", Cmd_Exec_f, CMD_SYSTEM, NULL);
  Cmd_SetAutocomplete(execCmd, Cmd_Exec_Autocomplete_f);
  Cmd_Add("echo", Cmd_Echo_f, 0, NULL);
  Cmd_Add("alias", Cmd_Alias_f, CMD_SYSTEM, NULL);
  Cmd_Add("wait", Cmd_Wait_f, 0, NULL);

  for (int32_t i = 1; i < Com_Argc(); i++) {
    const char *c = Com_Argv(i);

    // if we encounter a non-set command, consume until the next + or EOL
    if (*c == '+' && q_strncmp(c, "+set", 4)) {
      Cbuf_AddText(c + 1);
      i++;

      while (i < Com_Argc()) {
        c = Com_Argv(i);
        if (*c == '+') {
          Cbuf_AddText("\n");
          i--;
          break;
        }
        Cbuf_AddText(va(" %s", c));
        i++;
      }
    }
  }
  Cbuf_AddText("\n");
  Cbuf_CopyToDefer();

  // Com_Debug("Deferred buffer: %s", cmdState.buffers[1]);
}

/**
 * @brief Shuts down the command subsystem.
 */
void Cmd_Shutdown(void) {

  CmdShutdownCtx ctx = {
    .lists = $(alloc(PointerArray), init),
  };

  $(cmdState.commands, enumerate, Cmd_Shutdown_collect, ctx.lists);

  for (size_t i = 0; i < ctx.lists->count; i++) {
    release((List *) $(ctx.lists, get, i));
  }

  release(ctx.lists);

  cmdState.commands = release(cmdState.commands);
}

/*
 * An optional function pointer the client will implement; the server will not.
 */
void (*Cmd_ForwardToServer)(void) = NULL;
