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
#include <Objectively/Vector.h>

#include "console.h"
#include "filesystem.h"

typedef struct cmd_args_s {
  int32_t argc;
  char argv[MAX_STRING_TOKENS][MAX_TOKEN_CHARS];
  char args[MAX_STRING_CHARS];
} cmd_args_t;

#define CBUF_CHARS 65536

typedef struct cmd_state_s {
  HashTable *commands;

  mem_buf_t buf;
  char buffers[2][CBUF_CHARS];

  cmd_args_t args;

  bool wait; // commands may be deferred one frame

  int32_t alias_loop_count;
} cmd_state_t;

static cmd_state_t cmd_state;

#define MAX_ALIAS_LOOP_COUNT 8

/**
 * @brief Adds command text at the end of the buffer
 */
void Cbuf_AddText(const char *text) {

  const size_t l = q_strlen(text);

  if (cmd_state.buf.size + l >= cmd_state.buf.max_size) {
    Com_Warn("Overflow\n");
    return;
  }

  Mem_WriteBuffer(&cmd_state.buf, text, l);
}

/**
 * @brief Inserts command text at the beginning of the buffer.
 */
void Cbuf_InsertText(const char *text) {

  if (text  && q_strlen(text)) {
    void *temp;

    // copy off any commands still remaining in the exec buffer
    const size_t size = cmd_state.buf.size;
    if (size) {
      temp = Mem_TagMalloc(size, MEM_TAG_CMD);
      memcpy(temp, cmd_state.buf.data, size);
      Mem_ClearBuffer(&cmd_state.buf);
    } else {
      temp = NULL; // shut up compiler
    }

    // add the entire text of the file
    Cbuf_AddText(text);

    // add the copied off data
    if (size) {
      Mem_WriteBuffer(&cmd_state.buf, temp, size);
      Mem_Free(temp);
    }
  }
}

/**
 * @brief Copies the current command buffer to the deferred slot and clears the primary buffer.
 */
void Cbuf_CopyToDefer(void) {

  memcpy(cmd_state.buffers[1], cmd_state.buffers[0], cmd_state.buf.size);

  memset(cmd_state.buffers[0], 0, sizeof(cmd_state.buffers[0]));

  cmd_state.buf.size = 0;
}

/**
 * @brief Inserts the contents of the deferred buffer at the front of the command buffer.
 */
void Cbuf_InsertFromDefer(void) {

  Cbuf_InsertText(cmd_state.buffers[1]);

  memset(cmd_state.buffers[1], 0, sizeof(cmd_state.buffers[1]));
}

/**
 * @brief Executes the pending command buffer.
 */
void Cbuf_Execute(void) {

  cmd_state.alias_loop_count = 0; // don't allow infinite alias loops

  while (cmd_state.buf.size) {

    // read a single command line from the buffer
    char line[sizeof(cmd_state.args.args)] = "";

    // find a \n or; line break
    char *text = (char *) cmd_state.buf.data;

    uint32_t i, quotes = 0;
    for (i = 0; i < cmd_state.buf.size; i++) {
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

    if (i == cmd_state.buf.size) {
      cmd_state.buf.size = 0;
    } else {
      i++;

      cmd_state.buf.size = cmd_state.buf.size - i;
      memmove(text, text + i, cmd_state.buf.size);
    }

    // execute the command linequit

    Cmd_ExecuteString(line);

    // skip out while text still remains in buffer, leaving it for next frame
    if (cmd_state.wait) {
      cmd_state.wait = false;
      break;
    }
  }
}

/**
 * @return The command argument count.
 */
int32_t Cmd_Argc(void) {
  return cmd_state.args.argc;
}

/**
 * @return The command argument at the specified index.
 */
const char *Cmd_Argv(int32_t arg) {
  if (arg >= cmd_state.args.argc) {
    return "";
  }
  return cmd_state.args.argv[arg];
}

/**
 * @return A single string containing all command arguments.
 */
const char *Cmd_Args(void) {
  return cmd_state.args.args;
}

/**
 * @brief Parses the given string into command line tokens.
 */
void Cmd_TokenizeString(const char *text) {

  // clear the command state from the last string
  memset(&cmd_state.args, 0, sizeof(cmd_state.args));

  if (!text) {
    return;
  }

  // prevent overflows
  if (q_strlen(text) >= MAX_STRING_CHARS) {
    Com_Warn("MAX_STRING_CHARS exceeded\n");
    return;
  }

  parser_t parser = Parse_Init(text, PARSER_DEFAULT);

  while (true) {
    // stop after we've exhausted our token buffer
    if (cmd_state.args.argc == MAX_STRING_TOKENS) {
      Com_Warn("MAX_STRING_TOKENS exceeded\n");
      return;
    }

    // set cmd_state.args to everything after the command name
    if (cmd_state.args.argc == 1) {
      q_strlcpy(cmd_state.args.args, parser.position.ptr + 1, MAX_STRING_CHARS);

      // strip off any trailing whitespace
      size_t l = q_strlen(cmd_state.args.args);
      if (l > 0) {
        char *c = &cmd_state.args.args[l - 1];

        while (*c <= ' ') {
          *c-- = '\0';
        }
      }
    }

    if (!Parse_Token(&parser, PARSE_NO_WRAP | PARSE_COPY_QUOTED_LITERALS, cmd_state.args.argv[cmd_state.args.argc], MAX_TOKEN_CHARS)) { // we're done
      return;
    }

    // expand console variables
    if (*cmd_state.args.argv[cmd_state.args.argc] == '$' && q_strcmp(cmd_state.args.argv[0], "alias")) {
      const char *c = Cvar_GetString(cmd_state.args.argv[cmd_state.args.argc] + 1);
      q_strlcpy(cmd_state.args.argv[cmd_state.args.argc], c, MAX_TOKEN_CHARS);
    }

    cmd_state.args.argc++;
  }
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
static cmd_t *Cmd_Get_(const char *name, const bool case_sensitive) {

  if (cmd_state.commands) {
    List *list = $(cmd_state.commands, get, (void *) name);

    if (list) {
      if (list->count == 1) { // only 1 entry, return it
        cmd_t *cmd = list->head->element;

        if (!case_sensitive || q_strcmp(cmd->name, name) == 0) {
          return cmd;
        }
      } else {
        // only return the exact match
        for (const ListNode *node = list->head; node; node = node->next) {
          cmd_t *cmd = node->element;

          if (!q_strcmp(cmd->name, name)) {
            return cmd;
          }
        }
      }
    }
  }

  return NULL;
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
cmd_t *Cmd_Get(const char *name) {
  return Cmd_Get_(name, false);
}

static Order Cmd_Enumerate_comparator(const ident a, const ident b) {
  const int32_t cmp = q_strcasecmp((*(const cmd_t *const *) a)->name, (*(const cmd_t *const *) b)->name);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

typedef struct {
  Vector *cmds;
} Cmd_Enumerate_ctx_t;

static void Cmd_Enumerate_collect(const HashTable *table, ident key, ident value, ident data) {
  Cmd_Enumerate_ctx_t *ctx = data;
  const List *list = value;
  for (const ListNode *node = list->head; node; node = node->next) {
    cmd_t *cmd = node->element;
    $(ctx->cmds, addElement, &cmd);
  }
}

/**
 * @brief Enumerates all known commands with the given function.
 */
void Cmd_Enumerate(Cmd_Enumerator func, void *data) {
  Cmd_Enumerate_ctx_t ctx = {
    .cmds = $(alloc(Vector), initWithSize, sizeof(cmd_t *)),
  };

  $(cmd_state.commands, enumerate, Cmd_Enumerate_collect, &ctx);
  $(ctx.cmds, sort, Cmd_Enumerate_comparator);

  for (size_t i = 0; i < ctx.cmds->count; i++) {
    cmd_t *cmd = VectorValue(ctx.cmds, cmd_t *, i);
    func(cmd, data);
  }

  release(ctx.cmds);
}

/**
 * @brief Adds the specified command, bound to the given function.
 */
cmd_t *Cmd_Add(const char *name, CmdExecuteFunc function, uint32_t flags,
               const char *description) {
  cmd_t *cmd;

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
  List *list = $(cmd_state.commands, get, key);

  if (!list) {
    list = $(alloc(List), init);
    list->destroy = Mem_Free;
    $(cmd_state.commands, set, key, list);
  }

  $(list, prependElement, cmd);
  return cmd;
}

/**
 * @brief Assign the specified autocomplete function to the given command.
 */
void Cmd_SetAutocomplete(cmd_t *cmd, AutocompleteFunc autocomplete) {
  cmd->Autocomplete = autocomplete;
}

/**
 * @brief Adds the specified alias command, bound to the given commands string.
 */
static cmd_t *Cmd_Alias(const char *name, const char *commands) {
  cmd_t *cmd;

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
  List *list = $(cmd_state.commands, get, key);

  if (!list) {
    list = $(alloc(List), init);
    list->destroy = Mem_Free;
    $(cmd_state.commands, set, key, list);
  }

  $(list, prependElement, cmd);
  return cmd;
}

/**
 * @brief Removes the specified command from its backing list.
 * @note Disables the list destroy callback so the caller controls cmd lifetime.
 */
static List *Cmd_RemovePtr_(cmd_t *cmd) {
  List *list = $(cmd_state.commands, get, (void *) cmd->name);

  ListNode *node = $(list, nodeForElement, cmd);
  if (!node) {
    Com_Error(ERROR_FATAL, "Missing command: %s\n", cmd->name);
    return list;
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
  cmd_t *cmd = Cmd_Get_(name, true);

  if (cmd) {
    List *list = Cmd_RemovePtr_(cmd);

    if (!list->count) {
      $(cmd_state.commands, remove, (void *) name);
      release(list);
    }

    Mem_Free(cmd);
  }
}

typedef struct {
  uint32_t flags;
  List *cmds;
} Cmd_RemoveAll_ctx_t;

static void Cmd_RemoveAll_collect(const HashTable *table, ident key, ident value, ident data) {
  Cmd_RemoveAll_ctx_t *ctx = data;
  const List *list = value;
  for (const ListNode *node = list->head; node; node = node->next) {
    cmd_t *cmd = node->element;
    if (cmd->flags & ctx->flags) {
      $(ctx->cmds, appendElement, cmd);
    }
  }
}

/**
 * @brief Removes all commands which match the specified flags mask.
 */
void Cmd_RemoveAll(uint32_t flags) {
  Cmd_RemoveAll_ctx_t ctx = {
    .flags = flags,
    .cmds = $(alloc(List), init),
  };

  $(cmd_state.commands, enumerate, Cmd_RemoveAll_collect, &ctx);

  for (const ListNode *node = ctx.cmds->head; node; node = node->next) {
    cmd_t *cmd = node->element;
    List *list = Cmd_RemovePtr_(cmd);

    if (!list->count) {
      $(cmd_state.commands, remove, (void *) cmd->name);
      release(list);
    }

    Mem_Free(cmd);
  }

  release(ctx.cmds);
}

/**
 * @brief Stringify a command. This memory is temporary.
 */
static const char *Cmd_Stringify(const cmd_t *cmd) {
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

static char cmd_complete_pattern[MAX_STRING_CHARS];

/**
 * @brief Enumeration helper for `Cmd_CompleteCommand`.
 */
static void Cmd_CompleteCommand_enumerate(cmd_t *cmd, void *data) {
  List *matches = data;

  if (GlobMatch(cmd_complete_pattern, cmd->name, GLOB_CASE_INSENSITIVE)) {
    Con_AutocompleteMatch(matches, cmd->name, Cmd_Stringify(cmd));
  }
}

/**
 * @brief Console completion for commands and aliases.
 */
void Cmd_CompleteCommand(const char *pattern, List *matches) {
  q_strlcpy(cmd_complete_pattern, pattern, sizeof(cmd_complete_pattern));
  Cmd_Enumerate(Cmd_CompleteCommand_enumerate, matches);
}

/**
 * @brief A complete command line has been parsed, so try to execute it
 */
void Cmd_ExecuteString(const char *text) {
  cmd_t *cmd;

  Cmd_TokenizeString(text);

  if (!Cmd_Argc()) {
    return;
  }

  // execute the command line
  if ((cmd = Cmd_Get(Cmd_Argv(0)))) {
    if (cmd->Execute) {
      cmd->Execute();
    } else if (cmd->commands) {
      if (++cmd_state.alias_loop_count == MAX_ALIAS_LOOP_COUNT) {
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
static void Cmd_Alias_f_enumerate(cmd_t *cmd, void *data) {

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
  Vector *strs;
} Cmd_List_ctx_t;

static void Cmd_List_f_enumerate(cmd_t *cmd, void *data) {
  Cmd_List_ctx_t *ctx = data;
  char *str = q_strdup(Cmd_Stringify(cmd));
  $(ctx->strs, addElement, &str);
}

static Order Cmd_List_sortfn(const ident a, const ident b) {
  const int32_t cmp = q_strcolorcmp(*(const char *const *) a, *(const char *const *) b);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Lists all known commands at the console.
 */
static void Cmd_List_f(void) {
  Cmd_List_ctx_t ctx = {
    .strs = $(alloc(Vector), initWithSize, sizeof(char *)),
  };

  Cmd_Enumerate(Cmd_List_f_enumerate, &ctx);
  $(ctx.strs, sort, Cmd_List_sortfn);

  for (size_t i = 0; i < ctx.strs->count; i++) {
    char *str = VectorValue(ctx.strs, char *, i);
    Com_Print("%s\n", str);
    free(str);
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
  cmd_state.wait = true;
}

typedef struct {
  Vector *lists;
} Cmd_Shutdown_ctx_t;

static void Cmd_Shutdown_collect(const HashTable *table, ident key, ident value, ident data) {
  Vector *lists = data;
  $(lists, addElement, &value);
}

/**
 * @brief Initializes the command subsystem.
 */
void Cmd_Init(void) {

  memset(&cmd_state, 0, sizeof(cmd_state));

  cmd_state.commands = $(alloc(HashTable), init, HashTableHashStri, HashTableEqualStri);

  Mem_InitBuffer(&cmd_state.buf, (byte *) cmd_state.buffers[0], sizeof(cmd_state.buffers[0]));

  Cmd_Add("cmd_list", Cmd_List_f, 0, NULL);
  cmd_t *exec_cmd = Cmd_Add("exec", Cmd_Exec_f, CMD_SYSTEM, NULL);
  Cmd_SetAutocomplete(exec_cmd, Cmd_Exec_Autocomplete_f);
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

  // Com_Debug("Deferred buffer: %s", cmd_state.buffers[1]);
}

/**
 * @brief Shuts down the command subsystem.
 */
void Cmd_Shutdown(void) {

  Cmd_Shutdown_ctx_t ctx = {
    .lists = $(alloc(Vector), initWithSize, sizeof(ident)),
  };

  $(cmd_state.commands, enumerate, Cmd_Shutdown_collect, ctx.lists);

  for (size_t i = 0; i < ctx.lists->count; i++) {
    release(VectorValue(ctx.lists, List *, i));
  }

  release(ctx.lists);

  cmd_state.commands = release(cmd_state.commands);
}

/*
 * An optional function pointer the client will implement; the server will not.
 */
void (*Cmd_ForwardToServer)(void) = NULL;
