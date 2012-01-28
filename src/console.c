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

/*
 * console.c
 * Common structures and functions for the client console and
 * the server curses console.
 */

#include "console.h"

console_data_t console_data;

#ifdef BUILD_CLIENT
extern console_t cl_con;
extern void Cl_UpdateNotify(int last_line);
extern void Cl_ClearNotify(void);
#endif

cvar_t *ansi;

/*
 * Con_Update
 *
 * Update a console index struct, start parsing at pos
 */
static void Con_Update(console_t *con, char *pos) {
	char *wordstart;
	int linelen;
	int wordlen;
	int i;
	int curcolor;

	linelen = 0;
	wordlen = 0;
	curcolor = CON_COLOR_DEFAULT;
	con->line_start[con->last_line] = pos;
	con->line_color[con->last_line] = curcolor;

	if (con->width < 1)
		return;

	/* FIXME color at line_start is off by one line */
	wordstart = pos;
	while (*pos) {
		if (*pos == '\n') {
			while (wordlen > con->width && con->last_line < CON_MAX_LINES - 2) {
				// force wordsplit
				con->last_line++;
				con->line_start[con->last_line] = wordstart;
				con->line_color[con->last_line] = curcolor;
				wordstart = wordstart + (size_t) con->width;
				wordlen -= con->width;
			}
			if (linelen + wordlen > con->width) {
				// force linebreak
				con->last_line++;
				con->line_start[con->last_line] = wordstart;
				con->line_color[con->last_line] = curcolor;
			}
			con->last_line++;
			con->line_start[con->last_line] = pos + 1;
			curcolor = CON_COLOR_DEFAULT;
			con->line_color[con->last_line] = curcolor;
			linelen = 0;
			wordlen = 0;
			wordstart = pos + 1;
		} else if (*pos == ' ') {
			if (linelen + wordlen > con->width) {
				while (wordlen > con->width && con->last_line < CON_MAX_LINES
						- 2) {
					// force wordsplit
					con->last_line++;
					con->line_start[con->last_line] = wordstart;
					con->line_color[con->last_line] = curcolor;
					wordstart = wordstart + (size_t) con->width;
					wordlen -= con->width;
				}
				// force linebreak
				con->last_line++;
				con->line_start[con->last_line] = wordstart;
				con->line_color[con->last_line] = curcolor;
				linelen = wordlen + 1;
				wordlen = 0;
				wordstart = pos + 1;
			} else {
				linelen += wordlen + 1;
				wordlen = 0;
				wordstart = pos + 1;
			}
		} else if (IS_COLOR(pos)) {
			curcolor = (int) *(pos + 1) - '0';
			pos++;
		} else if (IS_LEGACY_COLOR(pos)) {
			curcolor = CON_COLOR_ALT;
		} else {
			wordlen++;
		}
		pos++;

		// handle line overflow
		if (con->last_line >= CON_MAX_LINES - 4) {
			for (i = 0; i < CON_MAX_LINES - (CON_MAX_LINES >> 2); i++) {
				con->line_start[i] = con->line_start[i + (CON_MAX_LINES >> 2)];
				con->line_color[i] = con->line_color[i + (CON_MAX_LINES >> 2)];
			}
			con->last_line -= CON_MAX_LINES >> 2;
		}
	}

	// sentinel
	con->line_start[con->last_line + 1] = pos;
}

/*
 * Con_Resize
 *
 * Change the width of an index, parse the console data structure if needed
 */
void Con_Resize(console_t *con, unsigned short width, unsigned short height) {
	if (!console_data.insert)
		return;

	if (con->height != height)
		con->height = height;

	if (con->width == width)
		return;

	// update the requested index
	con->width = width;
	con->last_line = 0;
	Con_Update(con, console_data.text);

#ifdef BUILD_CLIENT
	if (dedicated && !dedicated->value) {
		// clear client notification timings
		if (con == &cl_con)
			Cl_ClearNotify();
	}
#endif
}

/*
 * Con_Clear_f
 *
 * Clear the console data buffer
 */
static void Con_Clear_f(void) {
	memset(console_data.text, 0, sizeof(console_data.text));
	console_data.insert = console_data.text;

#ifdef BUILD_CLIENT
	if (dedicated && !dedicated->value) {
		// update the index for the client console
		cl_con.last_line = 0;
		Con_Update(&cl_con, console_data.insert);
	}
#endif
#ifdef HAVE_CURSES
	// update the index for the server console
	sv_con.last_line = 0;
	Con_Update(&sv_con, console_data.insert);
	// redraw the server console
	Curses_Refresh();
#endif
}

/*
 * Con_Dump_f
 *
 * Save the console contents to a file
 */
static void Con_Dump_f(void) {
	FILE *f;
	char name[MAX_OSPATH];
	char *pos;

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <file_name>\n", Cmd_Argv(0));
		return;
	}

	snprintf(name, sizeof(name), "%s/%s", Fs_Gamedir(), Cmd_Argv(1));

	Fs_CreatePath(name);
	f = fopen(name, "w");
	if (!f) {
		Com_Warn("Couldn't open %s.\n", name);
	} else {
		pos = console_data.text;
		while (pos < console_data.insert) {
			if (IS_COLOR(pos))
				pos++;
			else if (!IS_LEGACY_COLOR(pos))
				if (fwrite(pos, 1, 1, f) <= 0)
					Com_Warn("Failed to write console dump\n");
			pos++;
		}
		fclose(f);
		Com_Print("Dumped console text to %s.\n", name);
	}
}

/*
 * Con_PrintStdOut
 *
 * Print a color-coded string to stdout, remove color codes if requested
 */
static void Con_PrintStdOut(const char *text) {
	char buf[MAX_PRINT_MSG];
	int bold, color;
	unsigned int i;

	// start the string with foreground color
	memset(buf, 0, sizeof(buf));
	if (ansi && ansi->value) {
		strcpy(buf, "\033[0;39m");
		i = 7;
	} else {
		i = 0;
	}

	while (*text && i < sizeof(buf) - 8) {

		if (IS_LEGACY_COLOR(text)) {
			if (ansi && ansi->value) {
				strcpy(&buf[i], "\033[0;32m");
				i += 7;
			}
			text++;
			continue;
		}

		if (IS_COLOR(text)) {
			if (ansi && ansi->value) {
				bold = 0;
				color = 39;
				switch (*(text + 1)) {
				case '0': // black is mapped to bold
					bold = 1;
					break;
				case '1': // red
					color = 31;
					break;
				case '2': // green
					color = 32;
					break;
				case '3': // yellow
					bold = 1;
					color = 33;
					break;
				case '4': // blue
					color = 34;
					break;
				case '5': // cyan
					color = 36;
					break;
				case '6': // magenta
					color = 35;
					break;
				case '7': // white is mapped to foreground color
					color = 39;
					break;
				default:
					break;
				}
				snprintf(&buf[i], 8, "\033[%d;%dm", bold, color);
				i += 7;
			}
			text += 2;
			continue;
		}

		if (*text == '\n' && ansi && ansi->value) {
			strcat(buf, "\033[0;39m");
			i += 7;
		}

		buf[i++] = *text;
		text++;
	}

	if (ansi && ansi->value) // restore foreground color
		strcat(buf, "\033[0;39m");

	// print to stdout
	if (buf[0] != '\0')
		fputs(buf, stdout);
}

/*
 * Con_Print
 *
 * Print a message to the console data buffer
 */
void Con_Print(const char *text) {
#ifdef BUILD_CLIENT
	int last_line;
#endif
	// this can get called before the console is initialized
	if (!console_data.insert) {
		memset(console_data.text, 0, sizeof(console_data.text));
		console_data.insert = console_data.text;
	}

	// prevent overflow, text should still have a reasonable size
	if (console_data.insert + strlen(text) >= console_data.text
			+ sizeof(console_data.text) - 1) {
		memcpy(console_data.text, console_data.text + (sizeof(console_data.text) >> 1), sizeof(console_data.text) >> 1);
		memset(console_data.text + (sizeof(console_data.text) >> 1) ,0 , sizeof(console_data.text) >> 1);
		console_data.insert -= sizeof(console_data.text) >> 1;
#ifdef BUILD_CLIENT
		if (dedicated && !dedicated->value) {
			// update the index for the client console
			cl_con.last_line = 0;
			Con_Update(&cl_con, console_data.text);
		}
#endif
#ifdef HAVE_CURSES
		// update the index for the server console
		sv_con.last_line = 0;
		Con_Update(&sv_con, console_data.text);
#endif
	}

	// copy the text into the console buffer
	strcpy(console_data.insert, text);

#ifdef BUILD_CLIENT
	if (dedicated && !dedicated->value) {
		last_line = cl_con.last_line;

		// update the index for the client console
		Con_Update(&cl_con, console_data.insert);

		// update client message notification times
		Cl_UpdateNotify(last_line);
	}
#endif

#ifdef HAVE_CURSES
	// update the index for the server console
	Con_Update(&sv_con, console_data.insert);
#endif

	console_data.insert += strlen(text);

#ifdef HAVE_CURSES
	if (!con_curses->value) {
		// print output to stdout
		Con_PrintStdOut(text);
	} else {
		// Redraw the server console
		Curses_Refresh();
	}
#else
	// print output to stdout
	Con_PrintStdOut(text);
#endif
}

#define MAX_COMPLETE_MATCHES 1024
static const char *complete[MAX_COMPLETE_MATCHES];

/*
 *  Tab completion.  Query the command and cvar subsystems for potential
 *  matches, and append an appropriate string to the input buffer.  If no
 *  matches are found, do nothing.  If only one match is found, simply
 *  append it.  If multiple matches are found, append the longest possible
 *  common prefix they all share.
 */
int Con_CompleteCommand(char *input_text, unsigned short *input_position) {
	const char *match, *partial;
	const char *cmd = 0, *dir = 0, *ext = 0;
	int matches;

	partial = input_text;
	if (*partial == '\\' || *partial == '/')
		partial++;

	if (!*partial)
		return false; // lets start with at least something

	memset(complete, 0, sizeof(complete));

	if (strstr(partial, "map ") == partial) {
		cmd = "map ";
		dir = "maps/";
		ext = ".bsp";
	} else if (strstr(partial, "demo ") == partial) {
		cmd = "demo ";
		dir = "demos/";
		ext = ".dem";
	} else if (strstr(partial, "exec ") == partial) {
		cmd = "exec ";
		dir = "";
		ext = ".cfg";
	}

	if (cmd) { // auto-complete parameters for a command
		partial += strlen(cmd);
		matches = Fs_CompleteFile(dir, partial, ext, &complete[0]);
	} else { // auto-complete a command or variable
		cmd = "";
		matches = Cmd_CompleteCommand(partial, &complete[0]);
		matches += Cvar_CompleteVar(partial, &complete[matches]);
	}

	if (matches == 1)
		match = complete[0];
	else
		match = CommonPrefix(complete, matches);

	if (!match || *match == '\0')
		return false;

	sprintf(input_text, "/%s%s", cmd, match);
	if (ext && strstr(input_text, ext) != NULL)
		*strstr(input_text, ext) = 0; // lop off file extenion
	(*input_position) = strlen(input_text);

	if (matches == 1 && *cmd == 0) { // append a trailing space for single matches
		input_text[*input_position] = ' ';
		(*input_position)++;
	}

	input_text[*input_position] = 0;
	return true;
}

/*
 * Con_Init
 *
 * Initialize the console subsystem
 */
void Con_Init(void) {

#ifdef _WIN32
	ansi = Cvar_Get("ansi", "0", CVAR_ARCHIVE, NULL);
#else
	ansi = Cvar_Get("ansi", "1", CVAR_ARCHIVE, NULL);
#endif

#ifdef HAVE_CURSES
	Curses_Init();
#endif

	Cmd_AddCommand("clearconsole", Con_Clear_f, NULL);
	Cmd_AddCommand("dumpconsole", Con_Dump_f, NULL);
}

/*
 * Con_Shutdown
 *
 * Shutdown the console subsystem
 */
void Con_Shutdown(void) {
#ifdef HAVE_CURSES
	Curses_Shutdown();
#endif
}
