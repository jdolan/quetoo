/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "scriplib.h"

/*
 * 						PARSING STUFF
 */

typedef struct {
	char file_name[MAX_OSPATH];
	char *buffer, *script_p, *end_p;
	int line;
} script_t;

#define	MAX_INCLUDES 8
static script_t scriptstack[MAX_INCLUDES];
static script_t *script;
static int scriptline;

char token[MAXTOKEN];
static boolean_t endofscript;

/*
 * AddScriptToStack
 */
static void AddScriptToStack(const char *file_name){
	int size;

	script++;
	if(script == &scriptstack[MAX_INCLUDES])
		Com_Error(ERR_FATAL, "Script file exceeded MAX_INCLUDES\n");

	strcpy(script->file_name, file_name);

	size = Fs_LoadFile(script->file_name, (void **)(char *)&script->buffer);

	if(size == -1)
		Com_Error(ERR_FATAL, "Could not load %s\n", script->file_name);

	Com_Verbose("Loading %s (%d bytes)\n", script->file_name, size);

	script->line = 1;

	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
}


/*
 * LoadScriptFile
 */
void LoadScriptFile(const char *file_name){
	script = scriptstack;
	AddScriptToStack(file_name);

	endofscript = false;
}


/*
 * ParseFromMemory
 */
void ParseFromMemory(char *buffer, int size){
	script = scriptstack;
	script++;
	if(script == &scriptstack[MAX_INCLUDES])
		Com_Error(ERR_FATAL, "script file exceeded MAX_INCLUDES\n");
	strcpy(script->file_name, "memory buffer");

	script->buffer = buffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	endofscript = false;
}

/*
 * EndOfScript
 */
static boolean_t EndOfScript(boolean_t crossline){
	if(!crossline)
		Com_Error(ERR_FATAL, "EndOfScript: Line %i is incomplete\n", scriptline);

	if(!strcmp(script->file_name, "memory buffer")){
		endofscript = true;
		return false;
	}

	Z_Free(script->buffer);
	if(script == scriptstack + 1){
		endofscript = true;
		return false;
	}
	script--;
	scriptline = script->line;
	Com_Verbose("returning to %s\n", script->file_name);
	return GetToken(crossline);
}


/*
 * GetToken
 */
boolean_t GetToken(boolean_t crossline){
	char *token_p;

	if(script->script_p >= script->end_p)
		return EndOfScript(crossline);

// skip space
skipspace:
	while(script->script_p < script->end_p && *script->script_p <= 32){
		if(script->script_p >= script->end_p)
			return EndOfScript(crossline);
		if(*script->script_p++ == '\n'){
			if(!crossline)
				Com_Error(ERR_FATAL, "GetToken 0: Line %i is incomplete\n", scriptline);
			scriptline = script->line++;
		}
	}

	if(script->script_p >= script->end_p)
		return EndOfScript(crossline);

	// comments
	if((script->script_p[0] == '/' && script->script_p[1] == '/') ||
			script->script_p[0] == ';'){
		if(!crossline)
			Com_Error(ERR_FATAL, "GetToken 1: Line %i is incomplete\n", scriptline);
		while(*script->script_p++ != '\n')
			if(script->script_p >= script->end_p)
				return EndOfScript(crossline);
		goto skipspace;
	}

	// /* */ comments
	if(script->script_p[0] == '/' && script->script_p[1] == '*'){
		if(!crossline)
			Com_Error(ERR_FATAL, "GetToken 2: Line %i is incomplete\n", scriptline);
		script->script_p+=2;
		while(script->script_p[0] != '*' && script->script_p[1] != '/'){
			script->script_p++;
			if(script->script_p >= script->end_p)
				return EndOfScript(crossline);
		}
		script->script_p += 2;
		goto skipspace;
	}

	// copy token
	token_p = token;

	if(*script->script_p == '"'){
		// quoted token
		script->script_p++;
		while(*script->script_p != '"'){
			*token_p++ = *script->script_p++;
			if(script->script_p == script->end_p)
				break;
			if(token_p == &token[MAXTOKEN])
				Com_Error(ERR_FATAL, "Token too large on line %i\n", scriptline);
		}
		script->script_p++;
	} else	// regular token
		while(*script->script_p > 32 && *script->script_p != ';'){
			*token_p++ = *script->script_p++;
			if(script->script_p == script->end_p)
				break;
			if(token_p == &token[MAXTOKEN])
				Com_Error(ERR_FATAL, "Token too large on line %i\n", scriptline);
		}

	*token_p = 0;

	if(!strcmp(token, "$include")){
		GetToken(false);
		AddScriptToStack(token);
		return GetToken(crossline);
	}

	return true;
}


/*
 * TokenAvailable
 */
boolean_t TokenAvailable(void){
	char *search_p;

	search_p = script->script_p;

	if(search_p >= script->end_p)
		return false;

	while(*search_p <= 32){

		if(*search_p == '\n')
			return false;

		search_p++;

		if(search_p == script->end_p)
			return false;
	}

	if(*search_p == ';')
		return false;

	return true;
}
