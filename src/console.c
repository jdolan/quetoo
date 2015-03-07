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

#include "console.h"

#include <signal.h>

console_state_t console_state;

/*
 * @brief Allocates a new `console_string_t`.
 */
static console_string_t *Con_AllocString(const char *text) {

	console_string_t *str = g_new0(console_string_t, 1);
	if (str == NULL) {
		raise(SIGABRT);
	}

	str->chars = g_strdup(text ?: "");
	if (str->chars == NULL) {
		raise(SIGABRT);
	}

	str->size = strlen(str->chars);
	str->length = StrColorLen(str->chars);

	str->timestamp = Sys_Milliseconds();

	return str;
}

/*
 * @brief Frees the specified console_str_t.
 */
static void Con_FreeString(console_string_t *str) {

	if (str) {
		g_free(str->chars);
		g_free(str);
	}
}

/*
 * @brief Frees all console_str_t.
 */
static void Con_FreeStrings(void) {

	g_list_free_full(console_state.strings, Con_FreeString);

	console_state.strings = NULL;
	console_state.size = 0;
}

/*
 * @brief Clears the console buffer.
 */
static void Con_Clear_f(void) {

	Con_FreeStrings();
}

/*
 * @brief Save the console buffer to a file.
 */
static void Con_Dump_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <file_name>\n", Cmd_Argv(0));
		return;
	}

	file_t *file;
	if (!(file = Fs_OpenWrite(Cmd_Argv(1)))) {
		Com_Warn("Couldn't open %s\n", Cmd_Argv(1));
	} else {
		SDL_LockMutex(console_state.lock);

		const GList *list = console_state.strings;
		while (list) {
			const char *c = ((console_string_t *) list->data)->chars;
			while (*c) {
				if (IS_COLOR(c)) {
					c++;
				} else if (!IS_LEGACY_COLOR(c)) {
					if (Fs_Write(file, c, 1, 1) != 1) {
						Com_Warn("Failed to dump console\n");
						break;
					}
				}
				c++;
			}
		}

		SDL_UnlockMutex(console_state.lock);

		Fs_Close(file);
		Com_Print("Dumped console to %s.\n", Cmd_Argv(1));
	}
}

/*
 * @brief Append a message to the console data buffer.
 */
void Con_Print(const char *chars) {

	console_string_t *str = Con_AllocString(chars);

	SDL_LockMutex(console_state.lock);

	console_state.strings = g_list_append(console_state.strings, str);
	console_state.size += str->size;

	while (console_state.size > CON_MAX_SIZE) {
		GList *first = g_list_first(console_state.strings);
		str = first->data;

		console_state.strings = g_list_remove_link(console_state.strings, first);
		console_state.size -= str->size;

		g_list_free_full(first, Con_FreeString);
	}

	SDL_UnlockMutex(console_state.lock);
}

/**
 * @brief Wraps the specified string for the given line width.
 *
 * @param chars The null-terminated C string.
 * @param line_width The desired line width.
 * @param lines The output to store the line offsets.
 * @param max_lines The maximum number of line offsets to store.
 *
 * @return The number of line offsets.
 *
 * @remark If `lines` is `NULL`, this function simply counts the number of
 * wrapped lines in `chars`.
 */
size_t Con_Wrap(const char *chars, size_t line_width, const char **lines, size_t max_lines) {

	size_t count = 0;

	const char *c = chars;
	while (*c) {

		if (lines) {
			if (count < max_lines) {
				lines[count++] = c;
			} else {
				break;
			}
		}

		if (StrColorLen(c) < line_width) {
			break;
		}

		const char *d = c + line_width;
		while (!isspace(*d) && d > c) {
			d--;
		}

		c = d > c ? d + 1 : c + line_width;
	}

	return count;
}

/**
 * @brief Tails the console, returning as many as `max_lines` in `lines.
 *
 * @param console The console to tail.
 * @param lines The output to store line offsets.
 * @param max_lines The maximum number of line offsets to store.
 *
 * @return The number of line offsets.
 */
size_t Con_Tail(const console_t *console, const char **lines, size_t max_lines) {

	int32_t back = console->scroll + max_lines;

	GList *list = g_list_last(console_state.strings);
	while (list && back > 0) {
		const console_string_t *str = list->data;

		back -= Con_Wrap(str->chars, console->width, NULL, 0);

		list = list->prev;
	}

	size_t count = 0;

	while (list) {
		const console_string_t *str = list->data;

		count += Con_Wrap(str->chars, console->width, lines, max_lines);

		list = list->next;
	}

	return count;
}

/*
 * @brief Tab completion. Query various subsystems for potential
 * matches, and append an appropriate string to the input buffer. If no
 * matches are found, do nothing. If only one match is found, simply
 * append it. If multiple matches are found, append the longest possible
 * common prefix they all share.
 */
_Bool Con_CompleteCommand(char *input, uint16_t *pos, uint16_t len) {
	const char *pattern, *match;
	GList *matches = NULL;

	char *partial = input;
	if (*partial == '\\' || *partial == '/')
		partial++;

	if (!*partial)
		return false; // lets start with at least something

	// handle special cases for commands which accept filenames
	if (g_str_has_prefix(partial, "demo ")) {
		partial += strlen("demo ");
		pattern = va("demos/%s*.dem", partial);
		Fs_CompleteFile(pattern, &matches);
	} else if (g_str_has_prefix(partial, "exec ")) {
		partial += strlen("exec ");
		pattern = va("%s*.cfg", partial);
		Fs_CompleteFile(pattern, &matches);
	} else if (g_str_has_prefix(partial, "map ")) {
		partial += strlen("map ");
		pattern = va("maps/%s*.bsp", partial);
		Fs_CompleteFile(pattern, &matches);
	} else if (g_str_has_prefix(partial, "set ")) {
		partial += strlen("set ");
		pattern = va("%s*", partial);
		Cvar_CompleteVar(pattern, &matches);
	} else { // handle general case for commands and variables
		pattern = va("%s*", partial);
		Cmd_CompleteCommand(pattern, &matches);
		Cvar_CompleteVar(pattern, &matches);
	}

	if (g_list_length(matches) == 0)
		return false;

	if (g_list_length(matches) == 1)
		match = va("%s ", (char *) g_list_nth_data(matches, 0));
	else
		match = CommonPrefix(matches);

	g_snprintf(partial, len - (partial - input), "%s", match);
	(*pos) = strlen(input);

	g_list_free_full(matches, Mem_Free);

	return true;
}

/*
 * @brief Initialize the console subsystem. For Windows environments running
 * servers, we explicitly allocate a console and redirect stdio to and from it.
 */
void Con_Init(void) {

	memset(&console_state, 0, sizeof(console_state));

	console_state.lock = SDL_CreateMutex();

	Cmd_Add("clear", Con_Clear_f, 0, NULL);
	Cmd_Add("dump", Con_Dump_f, 0, NULL);
}

/*
 * @brief Shutdown the console subsystem
 */
void Con_Shutdown(void) {

	Cmd_Remove("clear");
	Cmd_Remove("dump");

	Con_FreeStrings();

	SDL_DestroyMutex(console_state.lock);
}
