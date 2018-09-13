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

#include "scriptlib.h"

/*
 * 						PARSING STUFF
 */

typedef struct {
	char filename[MAX_OS_PATH];
	char *buffer, *script_p, *end_p;
	int32_t line;
} script_t;

#define	MAX_INCLUDES 8
static script_t scriptstack[MAX_INCLUDES];
static script_t *script;
static int32_t scriptline;

char token[MAXTOKEN];
static _Bool endofscript;

/**
 * @brief
 */
static void AddScriptToStack(const char *filename) {

	script++;
	if (script == &scriptstack[MAX_INCLUDES]) {
		Com_Error(ERROR_FATAL, "Script file exceeded MAX_INCLUDES\n");
	}

	strcpy(script->filename, filename);

	const int64_t size = Fs_Load(script->filename, (void **) (char *) &script->buffer);

	if (size == -1) {
		Com_Error(ERROR_FATAL, "Could not load %s\n", script->filename);
	}

	Com_Verbose("Loading %s (%u bytes)\n", script->filename, (uint32_t) size);

	script->line = 1;

	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
}

/**
 * @brief
 */
void LoadScriptFile(const char *filename) {
	script = scriptstack;
	AddScriptToStack(filename);

	endofscript = false;
}

/**
 * @brief
 */
void UnloadScriptFiles(void) {

	for (size_t i = 0; i < lengthof(scriptstack); i++) {
		if (scriptstack[i].buffer) {
			Com_Verbose("Unloading %s\n", scriptstack[i].filename);
			Fs_Free(scriptstack[i].buffer);
		}
	}

	memset(scriptstack, 0, sizeof(scriptstack));
}

/**
 * @brief
 */
void ParseFromMemory(char *buffer, int32_t size) {
	script = scriptstack;
	script++;
	if (script == &scriptstack[MAX_INCLUDES]) {
		Com_Error(ERROR_FATAL, "Script file exceeded MAX_INCLUDES\n");
	}
	strcpy(script->filename, "memory buffer");

	script->buffer = buffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	endofscript = false;
}

/**
 * @brief
 */
static _Bool EndOfScript(_Bool crossline) {
	if (!crossline) {
		Com_Error(ERROR_FATAL, "Line %i is incomplete\n", scriptline);
	}

	if (!g_strcmp0(script->filename, "memory buffer")) {
		endofscript = true;
		return false;
	}

	Fs_Free(script->buffer);
	script->buffer = NULL;
	if (script == scriptstack + 1) {
		endofscript = true;
		return false;
	}
	script--;
	scriptline = script->line;
	Com_Verbose("returning to %s\n", script->filename);
	return GetToken(crossline);
}

/**
 * @brief
 */
_Bool GetToken(_Bool crossline) {
	char *token_p;

	if (script->script_p >= script->end_p) {
		return EndOfScript(crossline);
	}

	// skip space
skipspace:
	while (script->script_p < script->end_p && *script->script_p <= 32) {
		if (script->script_p >= script->end_p) {
			return EndOfScript(crossline);
		}
		if (*script->script_p++ == '\n') {
			if (!crossline) {
				Com_Error(ERROR_FATAL, "Line %i is incomplete (skip space)\n", scriptline);
			}
			scriptline = script->line++;
		}
	}

	if (script->script_p >= script->end_p) {
		return EndOfScript(crossline);
	}

	// comments
	if ((script->script_p[0] == '/' && script->script_p[1] == '/') || script->script_p[0] == ';') {
		if (!crossline) {
			Com_Error(ERROR_FATAL, "Line %i is incomplete (// comments)\n", scriptline);
		}
		while (*script->script_p++ != '\n')
			if (script->script_p >= script->end_p) {
				return EndOfScript(crossline);
			}
		goto skipspace;
	}

	// /* */ comments
	if (script->script_p[0] == '/' && script->script_p[1] == '*') {
		if (!crossline) {
			Com_Error(ERROR_FATAL, "Line %i is incomplete (/* comments */)\n", scriptline);
		}
		script->script_p += 2;
		while (script->script_p[0] != '*' && script->script_p[1] != '/') {
			script->script_p++;
			if (script->script_p >= script->end_p) {
				return EndOfScript(crossline);
			}
		}
		script->script_p += 2;
		goto skipspace;
	}

	// copy token
	token_p = token;

	if (*script->script_p == '"') {
		// quoted token
		script->script_p++;
		while (*script->script_p != '"') {
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p) {
				break;
			}
			if (token_p == &token[MAXTOKEN]) {
				Com_Error(ERROR_FATAL, "Token too large on line %i\n", scriptline);
			}
		}
		script->script_p++;
	} else
		// regular token
		while (*script->script_p > 32 && *script->script_p != ';') {
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p) {
				break;
			}
			if (token_p == &token[MAXTOKEN]) {
				Com_Error(ERROR_FATAL, "Token too large on line %i\n", scriptline);
			}
		}

	*token_p = 0;

	if (!g_strcmp0(token, "$include")) {
		GetToken(false);
		AddScriptToStack(token);
		return GetToken(crossline);
	}

	return true;
}

/**
 * @brief
 */
_Bool TokenAvailable(void) {
	char *search_p;

	search_p = script->script_p;

	if (search_p >= script->end_p) {
		return false;
	}

	while (*search_p <= 32) {

		if (*search_p == '\n') {
			return false;
		}

		search_p++;

		if (search_p == script->end_p) {
			return false;
		}
	}

	if (*search_p == ';') {
		return false;
	}

	return true;
}
