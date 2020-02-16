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

#include <signal.h>

#include "console.h"

console_state_t console_state;

/**
 * @brief Allocates a new `console_string_t`.
 */
static console_string_t *Con_AllocString(int32_t level, const char *string) {

	console_string_t *str = g_new0(console_string_t, 1);
	if (str == NULL) {
		raise(SIGABRT);
		return NULL;
	}

	const size_t string_len = strlen(string) + 4;

	str->level = level;
	str->chars = g_new0(char, string_len);

	g_strlcpy(str->chars, string, string_len); // copy in input
	g_strchomp(str->chars); // remove \n if it's there

	if (!g_str_has_suffix(str->chars, "^7")) { // append ^7 if we need it
		g_strlcat(str->chars, "^7", string_len);
	}

	if (g_strlcat(str->chars, "\n", string_len) >= string_len) {
		raise(SIGABRT);
		return NULL;
	}

	if (str->chars == NULL) {
		raise(SIGABRT);
		return NULL;
	}

	str->size = string_len;
	str->length = StrColorLen(str->chars);

	str->timestamp = quetoo.ticks;

	return str;
}

/**
 * @brief Frees the specified console_str_t.
 */
static void Con_FreeString(console_string_t *str, gpointer user_data) {

	if (str) {
		g_free(str->chars);
		g_free(str);
	}
}

/**
 * @brief Frees all console_str_t.
 */
static void Con_FreeStrings(void) {

	g_queue_foreach(&console_state.strings, (GFunc) Con_FreeString, NULL);
	g_queue_clear(&console_state.strings);

	console_state.size = 0;
}

/**
 * @brief Clears the console buffer.
 */
static void Con_Clear_f(void) {

	Con_FreeStrings();
}

/**
 * @brief Save the console buffer to a file.
 */
static void Con_Dump_f(void) {

	const char *path = "console.log";

	if (Cmd_Argc() > 1) {
		path = Cmd_Argv(1);
	}

	file_t *file;
	if (!(file = Fs_OpenWrite(path))) {
		Com_Warn("Couldn't open %s\n", path);
	} else {
		SDL_LockMutex(console_state.lock);

		const GList *list = console_state.strings.head;
		while (list) {
			const char *c = ((console_string_t *) list->data)->chars;
			while (*c) {
				if (StrIsColor(c)) {
					c++;
				} else {
					if (Fs_Write(file, c, 1, 1) != 1) {
						fputs("Failed to dump console\n", stderr);
						break;
					}
				}
				c++;
			}
			list = list->next;
		}

		SDL_UnlockMutex(console_state.lock);

		Fs_Close(file);
		Com_Print("Dumped console to %s.\n", path);
	}
}

/**
 * @return True if `str` passes the console's filter, false otherwise.
 */
static _Bool Con_Filter(const console_t *console, const console_string_t *str) {

	if (console->level) {
		return console->level & str->level;
	}

	return true;
}

/**
 * @brief Append a message to the console data buffer.
 */
void Con_Append(int32_t level, const char *string) {

	assert(string);

	console_string_t *str = Con_AllocString(level, string);

	SDL_LockMutex(console_state.lock);

	g_queue_push_tail(&console_state.strings, str);
	console_state.size += str->size;

	while (console_state.size > CON_MAX_SIZE) {
		GList *first = console_state.strings.head;
		str = first->data;

		g_queue_unlink(&console_state.strings, first);
		console_state.size -= str->size;

		g_list_free_full(first, (GDestroyNotify) Con_FreeString);
	}

	SDL_UnlockMutex(console_state.lock);

	if (console_state.consoles) {

		// iterate the configured consoles and append the new string

		for (GList *list = console_state.consoles; list; list = list->next) {
			const console_t *console = list->data;

			if (console->Append) {
				if (Con_Filter(console, str)) {
					console->Append(str);
				}
			}
		}
	} else {
		char stripped[strlen(string) + 1];

		StripColors(string, stripped);
		fputs(stripped, stdout);
	}
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
 * @remarks If `lines` is `NULL`, this function simply counts the number of
 * wrapped lines in `chars`.
 */
size_t Con_Wrap(const char *chars, size_t line_width, char **lines, size_t max_lines) {

	size_t count = 0;

	int8_t wrap_color = ESC_COLOR_DEFAULT, color = ESC_COLOR_DEFAULT;

	const char *line = chars;
	while (*line) {

		size_t width = 0;

		wrap_color = color;

		const char *c = line;
		while (*c && width < line_width) {

			if (*c == '\n') {
				break;
			}

			if (StrIsColor(c)) {
				color = *(c + 1) - '0';
				c += 2;
			} else {
				c++;
				width++;
			}
		}

		const char *eol = c;

		if (width == line_width) {
			while (!isspace(*eol)) {
				if (eol == line) {
					eol = c;
					break;
				}
				eol--;
			}
		}

		if (lines) {
			if (count < max_lines) {
				lines[count] = g_malloc((eol - line) + 3);
				lines[count][0] = ESC_COLOR;
				lines[count][1] = wrap_color + '0';
				lines[count][2] = '\0';
				strncat(lines[count], line, eol - line);
			} else {
				break;
			}
		}

		line = eol + 1;
		count++;
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
size_t Con_Tail(const console_t *console, char **lines, size_t max_lines) {

	assert(console);

	ssize_t back = console->scroll + max_lines;

	GList *start = NULL;
	GList *list = console_state.strings.tail;
	while (list) {
		const console_string_t *str = list->data;

		if (str->timestamp < console->whence) {
			break;
		}

		if (Con_Filter(console, str)) {

			back -= Con_Wrap(str->chars, console->width, NULL, 0);

			if (back < 0) {
				if (back < -1) {
					list = list->next;
				}
				break;
			}
		}

		start = list;
		list = list->prev;
	}

	size_t count = 0;

	while (start) {
		const console_string_t *str = start->data;

		if (Con_Filter(console, str)) {
			count += Con_Wrap(str->chars, console->width, lines + count, max_lines - count);
		}

		start = start->next;
	}

	return count;
}

/**
 * @brief Navigate history, copying the history line to the input buffer.
 */
void Con_NavigateHistory(console_t *console, console_history_nav_t nav) {

	assert(console);

	console_history_t *hist = &console->history;

	size_t p = 0;
	switch (nav) {
		case CON_HISTORY_PREV:
			p = (hist->pos - 1) % CON_HISTORY_SIZE;
			break;
		case CON_HISTORY_NEXT:
			p = (hist->pos + 1) % CON_HISTORY_SIZE;
			break;
	}

	if (strlen(hist->strings[p])) {
		console_input_t *in = &console->input;

		g_strlcpy(in->buffer, hist->strings[p], sizeof(in->buffer));
		in->pos = strlen(in->buffer);

		hist->pos = p;
	}
}

/**
 * @brief Reads the history log from file into the console's history.
 */
void Con_ReadHistory(console_t *console, file_t *file) {
	char str[MAX_PRINT_MSG];

	assert(console);
	assert(file);

	console_history_t *hist = &console->history;

	while (Fs_ReadLine(file, str, sizeof(str))) {
		g_strlcpy(hist->strings[hist->index++ % CON_HISTORY_SIZE], str, sizeof(str));
	}

	hist->pos = hist->index;
}

/**
 * @brief Writes the history buffer of the first configured console to file.
 */
void Con_WriteHistory(const console_t *console, file_t *file) {

	assert(console);
	assert(file);

	const console_history_t *hist = &console->history;

	for (size_t i = 1; i <= CON_HISTORY_SIZE; i++) {

		const char *str = hist->strings[(hist->index + i) % CON_HISTORY_SIZE];
		if (*str) {
			Fs_Print(file, "%s\n", str);
		}
	}
}

/**
 * @brief Tab completion for cvars & commands. This is the "generic case".
 */
void Con_AutocompleteInput_f(const uint32_t argi, GList **matches) {
	const char *partial = Cmd_Argv(argi);
	char pattern[strlen(partial) + 3];

	g_snprintf(pattern, sizeof(pattern), "%s*", partial);

	Cmd_CompleteCommand(pattern, matches);
	Cvar_CompleteVar(pattern, matches);
}

/**
 * @brief
 */
static void Con_PrintMatches(const console_t *console, GList *matches) {
	const guint num_matches = g_list_length(matches);

	if (!num_matches) {
		return;
	}

	// print empty line to separate old autocompletes
	Con_Append(PRINT_ECHO, "\n");

	size_t widest = 0;
	_Bool all_simple = true;

	// calculate width per column
	for (const GList *m = matches; m; m = m->next) {
		const com_autocomplete_match_t *match = (const com_autocomplete_match_t *) m->data;
		const char *str = (match->description ?: match->name);
		const size_t str_len = strlen(str);

		if (match->description) {
			all_simple = false;
		}

		if (strchr(str, '\n') != NULL) {
			widest = -1;
			break;
		}

		if (str_len > widest) {
			widest = str_len + 1;
		}
	}

	// calculate # that can fit in a row
	const size_t per_row = Maxf(console->width / widest, 1u);
	const size_t num_rows = Maxf(num_matches / per_row, 1u);

	// simple path
	if (per_row == 1 || (!all_simple && num_rows == 1)) {
		
		for (const GList *m = matches; m; m = m->next) {
			const com_autocomplete_match_t *match = (const com_autocomplete_match_t *) m->data;
			const char *str = (match->description ?: match->name);

			Con_Append(PRINT_ECHO, va("%s\n", str));
		}

		return;
	}

	const GList *m = matches;
	char line[per_row * widest + 1];

	while (m) {
		line[0] = '\0';

		for (size_t i = 0; m && i < per_row; i++, m = m->next) {
			const com_autocomplete_match_t *match = (const com_autocomplete_match_t *) m->data;
			const char *str = (match->description ?: match->name);
			const size_t str_len = strlen(str);

			g_strlcat(line, str, sizeof(line));

			for (size_t x = 0; x < widest - str_len; x++) {
				g_strlcat(line, " ", sizeof(line));
			}
		}

		if (line[0]) {
			g_strlcat(line, "\n", sizeof(line));
			Con_Append(PRINT_ECHO, line);
		}
	}
}

/**
 * @brief Returns the longest common prefix the specified words share.
 */
static char *Con_CommonPrefix(GList *matches) {
	static char common_prefix[MAX_TOKEN_CHARS];

	memset(common_prefix, 0, sizeof(common_prefix));

	if (!matches) {
		return common_prefix;
	}

	for (size_t i = 0; i < sizeof(common_prefix) - 1; i++) {
		GList *e = matches;
		const com_autocomplete_match_t *m = (const com_autocomplete_match_t *) e->data;
		const char c = m->name[i];

		e = e->next;

		while (e) {
			m = (const com_autocomplete_match_t *) e->data;
			const char *w = m->name;

			if (!c || tolower(w[i]) != tolower(c)) { // prefix no longer common
				return common_prefix;
			}

			e = e->next;
		}

		common_prefix[i] = c;
	}

	return common_prefix;
}

/**
 * @brief Tab completion. Query various subsystems for potential
 * matches, and append an appropriate string to the input buffer. If no
 * matches are found, do nothing. If only one match is found, simply
 * append it. If multiple matches are found, append the longest possible
 * common prefix they all share.
 */
_Bool Con_CompleteInput(console_t *console) {
	const char *match;
	GList *matches = NULL;

	char *partial = console->input.buffer;
	size_t max_len = sizeof(console->input.buffer) - 1;

	if (*partial == '\\' || *partial == '/') {
		partial++;
		max_len--; // prevent buffer overflow
	}

	const size_t partial_len = strlen(partial);

	if (!*partial) {
		return false;    // lets start with at least something
	}

	Cmd_TokenizeString(partial);

	uint32_t argi = Cmd_Argc() - 1;
	const _Bool new_argument = partial[strlen(partial) - 1] == ' ';

	if (new_argument) {
		argi++;
	}

	AutocompleteFunc autocomplete = NULL;

	if (argi == 0) {
		autocomplete = Con_AutocompleteInput_f;
	} else {
		const char *name = Cmd_Argv(0);
		const cmd_t *command = Cmd_Get(name);

		if (command) {
			autocomplete = command->Autocomplete;
		} else {
			const cvar_t *cvar = Cvar_Get(name);

			if (cvar) {
				autocomplete = cvar->Autocomplete;
			}
		}
	}

	if (!autocomplete) {
		return false;
	}

	autocomplete(argi, &matches);

	if (g_list_length(matches) == 0) {
		return false;
	}

	_Bool output_quotes = false;

	if (g_list_length(matches) == 1) {
		match = ((const com_autocomplete_match_t *) g_list_nth_data(matches, 0))->name;

		if (strchr(match, ' ') != NULL) {
			match = va("\"%s\" ", match);
			output_quotes = true;
		} else {
			match = va("%s ", match);
		}
	} else {
		match = Con_CommonPrefix(matches);

		if (!strlen(match)) {
			match = Cmd_Argv(argi);
		} else if (strchr(match, ' ') != NULL) {
			match = va("\"%s", match);
			output_quotes = true;
		}
	}

	if (new_argument) {
		g_strlcat(partial, match, max_len);
	} else {
		size_t arg_pos = 0;
		_Bool input_quotes = false;

		if (Cmd_Argc() > 1) {
			const char *last_arg = Cmd_Argv(Cmd_Argc() - 1);
			arg_pos = strlen(partial) - strlen(last_arg);

			uint8_t num_quotes = (partial[partial_len - 1] == '"') + (partial[arg_pos - 1 - (partial[partial_len - 1] == '"')] ==
			                     '"');

			if (num_quotes) {
				arg_pos -= num_quotes;
				input_quotes = true;
			}
		}

		if (!output_quotes && input_quotes) {

			if (g_list_length(matches) == 1) {
				match = va("\"%s\"", match);
			} else {
				match = va("\"%s", match);
			}
		}

		g_snprintf(partial + arg_pos, max_len - arg_pos, "%s", match);
	}

	console->input.pos = strlen(console->input.buffer);

	// print matches
	Con_PrintMatches(console, matches);

	g_list_free_full(matches, Mem_Free);

	return true;
}

/**
 * @brief Handle buffered input for the specified console, submitting it to the
 * command subsystem, appending it to all configured consoles, and resetting the
 * input state.
 */
void Con_SubmitInput(console_t *console) {

	if (*console->input.buffer) {

		const size_t h = console->history.index++ % CON_HISTORY_SIZE;
		g_strlcpy(console->history.strings[h], console->input.buffer, MAX_PRINT_MSG);

		console->history.pos = console->history.index;

		if (!g_str_has_suffix(console->input.buffer, "\n")) {
			g_strlcat(console->input.buffer, "\n", sizeof(console->input.buffer));
		}

		const char *cmd = console->input.buffer;
		if (*cmd == '\\' || *cmd == '/') {
			cmd++;
		}

		Cbuf_AddText(cmd);

		if (console->echo) {
			Con_Append(PRINT_ECHO, cmd);
		}

		memset(&console->input, 0, sizeof(console->input));
	}
}

/**
 * @brief Adds the given console to the configured consoles.
 */
void Con_AddConsole(const console_t *console) {

	SDL_LockMutex(console_state.lock);

	console_state.consoles = g_list_append(console_state.consoles, (gpointer) console);

	SDL_UnlockMutex(console_state.lock);
}

/**
 * @brief Removes the given console from the configured consoles.
 */
void Con_RemoveConsole(const console_t *console) {

	SDL_LockMutex(console_state.lock);

	console_state.consoles = g_list_remove(console_state.consoles, (gpointer) console);

	SDL_UnlockMutex(console_state.lock);
}

/**
 * @brief Initialize the console subsystem. For Windows environments running
 * servers, we explicitly allocate a console and redirect stdio to and from it.
 */
void Con_Init(void) {

	memset(&console_state, 0, sizeof(console_state));

	console_state.lock = SDL_CreateMutex();

	Cmd_Add("clear", Con_Clear_f, 0, NULL);
	Cmd_Add("dump", Con_Dump_f, 0, NULL);
}

/**
 * @brief Shutdown the console subsystem
 */
void Con_Shutdown(void) {

	Cmd_Remove("clear");
	Cmd_Remove("dump");

	Con_FreeStrings();

	g_list_free(console_state.consoles);

	SDL_DestroyMutex(console_state.lock);
	console_state.lock = NULL;
}
