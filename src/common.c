/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#include "common.h"

// console redirection (e.g. rcon)
typedef struct redirect_s {
	int32_t target;

	char *buffer;
	size_t size;

	RedirectFlush Flush;
} redirect_t;

static redirect_t redirect;

/*
 * @brief
 */
void Com_BeginRedirect(int32_t target, char *buffer, size_t size, RedirectFlush Flush) {

	if (!target || !buffer || !size || !Flush) {
		Com_Error(ERR_FATAL, "Invalid redirect\n");
	}

	redirect.target = target;
	redirect.buffer = buffer;
	redirect.size = size;
	redirect.Flush = Flush;

	*redirect.buffer = '\0';
}

/*
 * @brief
 */
void Com_EndRedirect(void) {

	if (!redirect.target || !redirect.buffer || !redirect.size || !redirect.Flush) {
		Com_Error(ERR_FATAL, "Invalid redirect\n");
	}

	redirect.Flush(redirect.target, redirect.buffer);

	memset(&redirect, 0, sizeof(redirect));
}

/*
 * @brief Print a debug statement. If the format begins with '!', the function
 * name is omitted.
 */
void Com_Debug_(const char *func, const char *fmt, ...) {
	char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
		fmt++;
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (quake2world.Debug) {
		quake2world.Debug((const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/*
 * @brief An error condition has occurred. This function does not return.
 */
void Com_Error_(const char *func, err_t err, const char *fmt, ...) {
	char msg[MAX_PRINT_MSG];

	if (err == ERR_FATAL) {
		static _Bool recursive;
		if (recursive) {
			fprintf(stderr, "Recursive error\n");
			exit(err);
		}
		recursive = true;
	}

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
		fmt++;
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (quake2world.Error) {
		quake2world.Error(err, (const char *) msg);
	} else {
		fprintf(stderr, "%s", msg);
		fflush(stderr);
		exit(err);
	}
}

/*
 * @brief
 */
void Com_Print(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (redirect.target) { // handle redirection (rcon)
		if ((strlen(msg) + strlen(redirect.buffer)) > (redirect.size - 1)) {
			redirect.Flush(redirect.target, redirect.buffer);
			*redirect.buffer = '\0';
		}
		g_strlcat(redirect.buffer, msg, redirect.size);
		return;
	}

	if (quake2world.Print) {
		quake2world.Print((const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/*
 * @brief Prints a warning message.
 */
void Com_Warn_(const char *func, const char *fmt, ...) {
	static char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
		fmt++;
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (quake2world.Warn) {
		quake2world.Warn((const char *) msg);
	} else {
		fprintf(stderr, "WARNING: %s", msg);
		fflush(stderr);
	}
}

/*
 * @brief
 */
void Com_Verbose(const char *fmt, ...) {
	va_list args;
	static char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Verbose) {
		quake2world.Verbose((const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/*
 * @brief Initializes the global arguments list and dispatches to an underlying
 * implementation, if provided. Should be called shortly after program
 * execution begins.
 */
void Com_Init(int32_t argc, char **argv) {

	quake2world.argc = argc;
	quake2world.argv = argv;

	//putenv("G_SLICE=always-malloc");

	if (quake2world.Init) {
		quake2world.Init();
	}
}

/*
 * @brief Program exit point under normal circumstances. Dispatches to a
 * specialized implementation, if provided, or simply prints the message and
 * exits. This function does not return.
 */
void Com_Shutdown(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	fmt = fmt ? fmt : ""; // normal shutdown will actually pass NULL

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Shutdown) {
		quake2world.Shutdown(msg);
	} else {
		Com_Print("%s", msg);
	}

	exit(0);
}

/*
 * @brief
 */
uint32_t Com_WasInit(uint32_t s) {
	return quake2world.subsystems & s;
}

/*
 * @brief
 */
void Com_InitSubsystem(uint32_t s) {
	quake2world.subsystems |= s;
}

/*
 * @brief
 */
void Com_QuitSubsystem(uint32_t s) {
	quake2world.subsystems &= ~s;
}



/*
 * @brief Returns the command line argument count.
 */
int32_t Com_Argc(void) {
	return quake2world.argc;
}

/*
 * @brief Returns the command line argument at the specified index.
 */
char *Com_Argv(int32_t arg) {
	if (arg < 0 || arg >= Com_Argc())
		return "";
	return quake2world.argv[arg];
}

/*
 * @brief
 */
void Com_PrintInfo(const char *s) {
	char key[512];
	char value[512];
	char *o;
	int32_t l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Print("%s", key);

		if (!*s) {
			Com_Print("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Print("%s\n", value);
	}
}
