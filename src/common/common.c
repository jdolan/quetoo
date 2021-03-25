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

#include "common.h"

/*
 * FIXME: This is a hack. I'd rather see this implemented as a Windows-only console appender
 * in console.c or something.
 */
#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
 #include <Windows.h>
#endif

/**
 * @brief Logs a string to the log file.
 */
void Com_LogString(const char *str) {

	if (!str || !*str || !quetoo.log_file) {
		return;
	}

	fprintf(quetoo.log_file, "%s", str);
	fflush(quetoo.log_file);
}

/**
 * @brief Sets up a log file that will be used for debugging issues
 * with the game's initialization routines.
 */
static void Com_InitLog(int32_t argc, char *argv[]) {

	quetoo.log_file = fopen(va("quetoo_%" PRIiMAX ".log", (intmax_t) time(NULL)), "w");

	Com_LogString(va("Quetoo %s %s %s\n", VERSION, BUILD, REVISION));

	if (argc)
	{
		Com_LogString("Launch arguments:\n");
		
		for (int32_t i = 0; i < argc; i++)
			Com_LogString(va("%s\n", argv[i]));
	}
}

// max len we'll try to parse for a category
#define DEBUG_CATEGORY_MAX_LEN	32

const char *DEBUG_CATEGORIES[DEBUG_TOTAL] = {
	"ai",
	"cgame",
	"client",
	"collision",
	"common",
	"console",
	"filesystem",
	"game",
	"net",
	"pmove_client",
	"pmove_server",
	"renderer",
	"server",
	"sound",
	"ui"
};

/**
 * @return A string containing all enabled debug categories.
 */
const char *Com_GetDebug(void) {
	static char debug[MAX_STRING_CHARS];

	debug[0] = '\0';

	for (size_t i = 0; i < lengthof(DEBUG_CATEGORIES); i++) {
		if (quetoo.debug_mask & (1 << i)) {
			if (strlen(debug)) {
				g_strlcat(debug, " ", sizeof(debug));
			}
			g_strlcat(debug, DEBUG_CATEGORIES[i], sizeof(debug));
		}
	}

	if (quetoo.debug_mask & DEBUG_BREAKPOINT) {
		g_strlcat(debug, " breakpoint", sizeof(debug));
	}

	return debug;
}

/**
 * @brief Parses a debug string and sets up the quetoo.debug value
 */
void Com_SetDebug(const char *debug) {

	static char token[DEBUG_CATEGORY_MAX_LEN];
	static parser_t parser;

	Parse_Init(&parser, debug, PARSER_NO_COMMENTS);

	while (true) {

		if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "none") || !g_strcmp0(token, "0")) {
			quetoo.debug_mask = 0;
		} else if (!g_strcmp0(token, "breakpoint") || !g_strcmp0(token, "bp")) {
			quetoo.debug_mask ^= DEBUG_BREAKPOINT;
		} else if (!g_strcmp0(token, "any") || !g_strcmp0(token, "all")) {
			quetoo.debug_mask ^= DEBUG_ALL;
		} else {
			for (size_t i = 0; i < lengthof(DEBUG_CATEGORIES); i++) {
				if (!g_strcmp0(token, DEBUG_CATEGORIES[i])) {
					quetoo.debug_mask ^= (1 << i);
				}
			}
		}
	}
}

/**
 * @brief A helper to handle function name embedding in common debug / warn / error routines.
 * @param str The outbut buffer.
 * @param size The size of the output buffer.
 * @param func An optional function name to prepend.
 * @param fmt The format string.
 * @param args The format arguments.
 * @return The number of characters printed.
 */
__attribute__((format(printf, 4, 0)))
static int32_t Com_Sprintfv(char *str, size_t size, const char *func, const char *fmt, va_list args) {

	assert(str);
	assert(size);
	assert(fmt);

	size_t len = 0;

	if (func) {
		if (fmt[0] == '!') { // skip it
			fmt++;
		} else {
			g_snprintf(str, (gulong) size, "%s: ", func);
			len = strlen(str);
		}
	}

	const int32_t count = vsnprintf(str + len, size - len, fmt, args);

	/*
	 * FIXME: This is a hack. I'd rather see this implemented as a Windows-only console appender
	 * in console.c or something.
	 */
#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	OutputDebugString(str);
#endif

	return count;
}

/**
 * @brief Print a debug statement. If the format begins with '!', the function name is omitted.
 */
void Com_Debug_(const debug_t debug, const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Debugv_(debug, func, fmt, args);

	va_end(args);
}

/**
 * @brief Print a debug statement. If the format begins with '!', the function name is omitted.
 */
void Com_Debugv_(const debug_t debug, const char *func, const char *fmt, va_list args) {

	if ((quetoo.debug_mask & debug) == 0) {
		return;
	}

	char msg[MAX_PRINT_MSG];
	Com_Sprintfv(msg, sizeof(msg), func, fmt, args);

	Com_LogString(msg);

	if (quetoo.Debug) {
		quetoo.Debug(debug, (const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/**
 * @brief An error condition has occurred. This function does not return.
 */
void Com_Error_(err_t error, const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Errorv_(error, func, fmt, args);

	va_end(args);
}

#include <SDL_assert.h>

/**
 * @brief An error condition has occurred. This function does not return.
 */
void Com_Errorv_(err_t error, const char *func, const char *fmt, va_list args) {

	if (quetoo.recursive_error) {
		if (quetoo.Error) {
			quetoo.Error(ERROR_FATAL, "Recursive error\n");
		} else {
			fputs("Recursive error\n", stderr);
			fflush(stderr);
			exit(error);
		}
	} else {
		quetoo.recursive_error = true;
	}

	char msg[MAX_PRINT_MSG];
	Com_Sprintfv(msg, sizeof(msg), func, fmt, args);

	Com_LogString(msg);

	if (quetoo.Error) {
		quetoo.Error(error, msg);
	} else {
		fputs(msg, stderr);
		fflush(stderr);
		exit(error);
	}

	quetoo.recursive_error = false;
}

/**
 * @brief
 */
void Com_Print(const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Printv(fmt, args);

	va_end(args);
}

/**
 * @brief
 */
void Com_Printv(const char *fmt, va_list args) {

	char msg[MAX_PRINT_MSG];
	Com_Sprintfv(msg, sizeof(msg), NULL, fmt, args);

	Com_LogString(msg);

	if (quetoo.Print) {
		quetoo.Print(msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/**
 * @brief Prints a warning message.
 */
void Com_Warn_(const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Warnv_(func, fmt, args);

	va_end(args);
}

/**
 * @brief Prints a warning message.
 */
void Com_Warnv_(const char *func, const char *fmt, va_list args) {

	char msg[MAX_PRINT_MSG];
	Com_Sprintfv(msg, sizeof(msg), func, fmt, args);

	Com_LogString(msg);

	if (quetoo.Warn) {
		quetoo.Warn(msg);
	} else {
		fprintf(stderr, "WARNING: %s", msg);
		fflush(stderr);
	}
}

/**
 * @brief
 */
void Com_Verbose(const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Verbosev(fmt, args);

	va_end(args);
}

/**
 * @brief
 */
void Com_Verbosev(const char *fmt, va_list args) {

	char msg[MAX_PRINT_MSG];
	Com_Sprintfv(msg, sizeof(msg), NULL, fmt, args);

	Com_LogString(msg);

	if (quetoo.Verbose) {
		quetoo.Verbose((const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/**
 * @brief Initializes the global arguments list and dispatches to an underlying
 * implementation, if provided. Should be called shortly after program
 * execution begins.
 */
void Com_Init(int32_t argc, char *argv[]) {

	quetoo.argc = argc;
	quetoo.argv = argv;

	// options that any Com_* implementation can use
	for (int32_t i = 1; i < Com_Argc(); i++) {

		// if we specified debug mode, quickly set it to all here
		// so that early systems prior to init can write stuff out
		if (!g_strcmp0(Com_Argv(i), "-debug") ||
		        !g_strcmp0(Com_Argv(i), "+debug")) {
			Com_SetDebug("all");
			continue;
		}

		if (!g_strcmp0(Com_Argv(i), "-log") ||
		        !g_strcmp0(Com_Argv(i), "+log")) {
			Com_InitLog(argc, argv);
			continue;
		}
	}

	if (quetoo.Init) {
		quetoo.Init();
	}
}

/**
 * @brief Program exit point under normal circumstances. Dispatches to a
 * specialized implementation, if provided, or simply prints the message and
 * exits. This function does not return.
 */
void Com_Shutdown(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	fmt = fmt ? : ""; // normal shutdown will actually pass NULL

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quetoo.Shutdown) {
		quetoo.Shutdown(msg);
	} else {
		Com_Print("%s", msg);
	}

	// close log file
	if (quetoo.log_file) {
		fclose(quetoo.log_file);
	}

	exit(0);
}

/**
 * @brief
 */
uint32_t Com_WasInit(uint32_t s) {
	return quetoo.subsystems & s;
}

/**
 * @brief
 */
void Com_InitSubsystem(uint32_t s) {
	quetoo.subsystems |= s;
}

/**
 * @brief
 */
void Com_QuitSubsystem(uint32_t s) {
	quetoo.subsystems &= ~s;
}

/**
 * @brief Returns the command line argument count.
 */
int32_t Com_Argc(void) {
	return quetoo.argc;
}

/**
 * @brief Returns the command line argument at the specified index.
 */
char *Com_Argv(int32_t arg) {
	if (arg < 0 || arg >= Com_Argc()) {
		return "";
	}
	return quetoo.argv[arg];
}

/**
 * @brief
 */
void Com_PrintInfo(const char *s) {
	char key[512];
	char value[512];
	char *o;
	intptr_t l;

	if (*s == '\\') {
		s++;
	}
	while (*s) {
		o = key;
		while (*s && *s != '\\') {
			*o++ = *s++;
		}

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else {
			*o = 0;
		}
		Com_Print("%s", key);

		if (!*s) {
			Com_Print("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\') {
			*o++ = *s++;
		}
		*o = 0;

		if (*s) {
			s++;
		}
		Com_Print("%s\n", value);
	}
}
