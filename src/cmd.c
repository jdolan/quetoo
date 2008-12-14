/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#include "common.h"
#include "hash.h"

#define MAX_ALIAS_NAME 32

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

static cmdalias_t *cmd_alias;

static qboolean cmd_wait;

#define ALIAS_LOOP_COUNT 16
static int alias_count;  // for detecting runaway loops


/*
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "+attack; wait; -attack;"
*/
static void Cmd_Wait_f(void){
	cmd_wait = true;
}


/*

COMMAND BUFFER

*/

static sizebuf_t cmd_text;
static byte cmd_text_buf[8192];

static char defer_text_buf[8192];


/*
Cbuf_Init
*/
void Cbuf_Init(void){
	Sb_Init(&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}


/*
Cbuf_AddText

Adds command text at the end of the buffer
*/
void Cbuf_AddText(const char *text){
	int l;

	l = strlen(text);

	if(cmd_text.cursize + l >= cmd_text.maxsize){
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}
	Sb_Write(&cmd_text, text, l);
}


/*
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
*/
void Cbuf_InsertText(const char *text){
	char *temp;
	int templen;

	// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if(templen){
		temp = Z_Malloc(templen);
		memcpy(temp, cmd_text.data, templen);
		Sb_Clear(&cmd_text);
	} else
		temp = NULL;  // shut up compiler

	// add the entire text of the file
	Cbuf_AddText(text);

	// add the copied off data
	if(templen){
		Sb_Write(&cmd_text, temp, templen);
		Z_Free(temp);
	}
}


/*
Cbuf_CopyToDefer
*/
void Cbuf_CopyToDefer(void){
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}


/*
Cbuf_InsertFromDefer
*/
void Cbuf_InsertFromDefer(void){
	Cbuf_InsertText(defer_text_buf);
	defer_text_buf[0] = 0;
}


/*
Cbuf_Execute
*/
void Cbuf_Execute(void){
	int i;
	char *text;
	char line[MAX_STRING_CHARS];
	int quotes;

	alias_count = 0;  // don't allow infinite alias loops

	while(cmd_text.cursize){
		// find a \n or; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for(i = 0; i < cmd_text.cursize; i++){
			if(text[i] == '"')
				quotes++;
			if(!(quotes & 1) && text[i] == ';')
				break;  // don't break if inside a quoted string
			if(text[i] == '\n')
				break;
		}

		if(i > MAX_STRING_CHARS){  //length check each command
			Com_Printf("Command exceeded %i chars, discarded\n", MAX_STRING_CHARS);
			return;
		}

		memcpy(line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands(exec, alias) can insert data at the
		// beginning of the text buffer

		if(i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else {
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString(line);

		if(cmd_wait){
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}


/*
Cbuf_AddEarlyCommands

Adds command line parameters as script statements
Commands lead with a +, and continue until another +.

Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
*/
void Cbuf_AddEarlyCommands(qboolean clear){
	int i;
	char *s;

	for(i = 0; i < Com_Argc(); i++){
		s = Com_Argv(i);
		if(strcmp(s, "+set"))
			continue;
		Cbuf_AddText(va("set %s %s\n", Com_Argv(i + 1), Com_Argv(i + 2)));
		if(clear){
			Com_ClearArgv(i);
			Com_ClearArgv(i + 1);
			Com_ClearArgv(i + 2);
		}
		i += 2;
	}
	Cbuf_Execute();
}


/*
Cbuf_AddLateCommands

Adds remaining command line parameters as script statements
Commands lead with a + and continue until another +.
*/
void Cbuf_AddLateCommands(void){
	int i, j, k;
	char *c, text[MAX_STRING_CHARS];

	j = 0;
	memset(text, 0, sizeof(text));

	for(i = 1; i < Com_Argc(); i++){

		c = Com_Argv(i);
		k = strlen(c);

		if(j + k > sizeof(text) - 1)
			break;

		strcat(text, c);
		strcat(text, " ");
		j += k + 1;
	}

	for(i = 0; i < j; i++){
		if(text[i] == '+')
			text[i] = '\n';
	}
	strcat(text, "\n");

	Cbuf_AddText(text);
	Cbuf_Execute();
}


/*

						SCRIPT COMMANDS

*/


/*
Cmd_Exec_f
*/
static void Cmd_Exec_f(void){
	void *buf;
	int len;

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <filename> : execute a script file\n", Cmd_Argv(0));
		return;
	}

	if((len = Fs_LoadFile(Cmd_Argv(1), &buf)) == -1){
		Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
		return;
	}

	Cbuf_InsertText((const char *)buf);
	Fs_FreeFile(buf);
}


/*
Cmd_Echo_f

Just prints the rest of the line to the console
*/
static void Cmd_Echo_f(void){
	int i;

	for(i = 1; i < Cmd_Argc(); i++)
		Com_Printf("%s ", Cmd_Argv(i));
	Com_Printf("\n");
}


/*
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
*/
static void Cmd_Alias_f(void){
	cmdalias_t *a;
	char cmd[MAX_STRING_CHARS];
	int i, c;
	char *s;

	if(Cmd_Argc() == 1){
		Com_Printf("Current alias commands:\n");
		for(a = cmd_alias; a; a = a->next)
			Com_Printf("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv(1);
	if(strlen(s) >= MAX_ALIAS_NAME){
		Com_Printf("Alias name is too long\n");
		return;
	}

	// if the alias already exists, reuse it
	for(a = cmd_alias; a; a = a->next){
		if(!strcmp(s, a->name)){
			Z_Free(a->value);
			break;
		}
	}

	if(!a){
		a = Z_Malloc(sizeof(*a));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strcpy(a->name, s);

	// copy the rest of the command line
	cmd[0] = 0;  // start out with a null string
	c = Cmd_Argc();
	for(i = 2; i < c; i++){
		strcat(cmd, Cmd_Argv(i));
		if(i != (c - 1))
			strcat(cmd, " ");
	}
	strcat(cmd, "\n");

	a->value = CopyString(cmd);
}

/*

COMMAND EXECUTION

*/

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	const char *name;
	xcommand_t function;
	const char *description;
} cmd_function_t;


static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static char *cmd_null_string = "";
static char cmd_args[MAX_STRING_CHARS];

static cmd_function_t *cmd_functions;  // possible commands to execute

static hashtable_t cmd_hashtable;  // hashed for fast lookups


/*
Cmd_Argc
*/
int Cmd_Argc(void){
	return cmd_argc;
}


/*
Cmd_Argv
*/
char *Cmd_Argv(int arg){
	if((unsigned)arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}


/*
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
*/
char *Cmd_Args(void){
	return cmd_args;
}


/*
Cmd_TokenizeString

Parses the given string into command line tokens.
*/
void Cmd_TokenizeString(const char *text){
	int i, l, cmd_pointer;
	char *com_token;

	// clear the args from the last string
	for(i = 0; i < cmd_argc; i++)
		Z_Free(cmd_argv[i]);

	cmd_argc = 0;
	cmd_args[0] = 0;
	cmd_pointer = 0;

	if(!text)
		return;

	while(true){
		// skip whitespace up to a \n
		while(*text && *text <= ' ' && *text != '\n'){
			text++;
		}

		if(*text == '\n'){  // a newline seperates commands in the buffer
			text++;
			break;
		}

		if(!*text)
			return;

		// set cmd_args to everything after the first arg
		if(cmd_argc == 1){

			strncpy(cmd_args, text, MAX_STRING_CHARS);

			if((l = strlen(cmd_args) - 1) == MAX_STRING_CHARS - 1)
				return;  // catch maliciously long tokens

			// strip off any trailing whitespace
			for(; l >= 0; l--)
				if(cmd_args[l] <= ' ')
					cmd_args[l] = 0;
				else
					break;
		}

		com_token = Com_Parse(&text);
		if(!text)
			return;

		if(*com_token == '$' && strcmp(cmd_argv[0], "alias"))  // expand cvar values
			com_token = Cvar_VariableString(com_token + 1);

		if(cmd_argc < MAX_STRING_TOKENS){
			if((l = strlen(com_token)) == MAX_STRING_CHARS)
				return;  // catch maliciously long tokens

			cmd_argv[cmd_argc] = Z_Malloc(l + 1);
			strcpy(cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}


/*
Cmd_AddCommand
*/
void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *description){
	cmd_function_t *c, *cmd;

	// fail if the command is a variable name
	if(Cvar_VariableString(cmd_name)[0]){
		Com_Dprintf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	// fail if the command already exists
	for(cmd = cmd_functions; cmd; cmd = cmd->next){
		if(!strcmp(cmd_name, cmd->name)){
			Com_Dprintf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Z_Malloc(sizeof(*cmd));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->description = description;

	// hash the command
	Com_HashInsert(&cmd_hashtable, cmd_name, cmd);

	// and add it to the chain
	if(!cmd_functions){
		cmd_functions = cmd;
		return;
	}

	c = cmd_functions;
	while(c->next){
		if(strcmp(cmd->name, c->next->name) < 0){  // insert it
			cmd->next = c->next;
			c->next = cmd;
			return;
		}
		c = c->next;
	}

	c->next = cmd;
}


/*
Cmd_RemoveCommand
*/
void Cmd_RemoveCommand(const char *cmd_name){
	cmd_function_t *cmd, **back;

	Com_HashRemove(&cmd_hashtable, cmd_name);

	back = &cmd_functions;
	while(true){
		cmd = *back;
		if(!cmd){
			Com_Dprintf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if(!strcmp(cmd_name, cmd->name)){
			*back = cmd->next;
			Z_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}


/*
Cmd_CompleteCommand
*/
int Cmd_CompleteCommand(const char *partial, const char *matches[]){
	cmd_function_t *cmd;
	cmdalias_t *a;
	int len;
	int m;

	len = strlen(partial);
	m = 0;

	// check for partial matches in commands
	for(cmd = cmd_functions; cmd; cmd = cmd->next){
		if(!strncmp(partial, cmd->name, len)){
			Com_Printf("%c%s\n", (char)1, cmd->name);
			if (cmd->description)
				Com_Printf("\t%s\n", cmd->description);
			matches[m] = cmd->name;
			m++;
		}
	}

	// and then aliases
	for(a = cmd_alias; a; a = a->next){
		if(!strncmp(partial, a->name, len)){
			Com_Printf("%c%s\n", (char)1, a->name);
			matches[m] = a->name;
			m++;
		}
	}

	return m;
}


/*
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
*/
void Cmd_ExecuteString(const char *text){
	cmd_function_t *cmd;
	cmdalias_t *a;

	Cmd_TokenizeString(text);

	// execute the command line
	if(!Cmd_Argc())
		return;  // no tokens

	if((cmd = Com_HashValue(&cmd_hashtable, cmd_argv[0]))){
		if(cmd->function)  // forward to server command
			cmd->function();
		else if(!dedicated->value)
			Cmd_ForwardToServer();
		return;
	}

	// check alias
	for(a = cmd_alias; a; a = a->next){
		if(!strcasecmp(cmd_argv[0], a->name)){
			if(++alias_count == ALIAS_LOOP_COUNT){
				Com_Warn("ALIAS_LOOP_COUNT reached.\n");
				return;
			}
			Cbuf_InsertText(a->value);
			return;
		}
	}

	// check cvars
	if(Cvar_Command())
		return;

	// send it as a server command if we are connected
	if (!dedicated->value)
		Cmd_ForwardToServer();
}


/*
Cmd_List_f
*/
static void Cmd_List_f(void){
	cmd_function_t *cmd;
	int i;

	i = 0;
	for(cmd = cmd_functions; cmd; cmd = cmd->next, i++) {
		Com_Printf("%s\n", cmd->name);
		if (cmd->description)
			Com_Printf("   ^2%s\n", cmd->description);
	}
	Com_Printf("%i commands\n", i);
}


/*
Cmd_Init
*/
void Cmd_Init(void){

	Com_HashInit(&cmd_hashtable);

	Cmd_AddCommand("cmd_list", Cmd_List_f, NULL);
	Cmd_AddCommand("exec", Cmd_Exec_f, NULL);
	Cmd_AddCommand("echo", Cmd_Echo_f, NULL);
	Cmd_AddCommand("alias", Cmd_Alias_f, NULL);
	Cmd_AddCommand("wait", Cmd_Wait_f, NULL);
}
