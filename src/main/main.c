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

#include <setjmp.h>
#include <signal.h>

#include <SDL2/SDL_assert.h>

#include "config.h"
#include "client/client.h"
#include "server/server.h"

static jmp_buf env;

quetoo_t quetoo;

static cvar_t *debug;
static cvar_t *verbose;

cvar_t *dedicated;
cvar_t *game;
cvar_t *time_demo;
cvar_t *time_scale;

static void Debug(const debug_mask_t mask, const char *msg);
static void Error(err_t err, const char *msg) __attribute__((noreturn));
static void Print(const char *msg);
static void Shutdown(const char *msg);
static void Verbose(const char *msg);
static void Warn(const char *msg);

static const debug_mask_t DEBUG_BREAKPOINT = (debug_mask_t) (1u << 31);

/**
 * @brief A mapping of built-in DEBUG_xxx masks to strings.
 */
static const char *debug_mask_strings[] = {
	"generic",
	"client",
	"server",
	"game",
	"cgame",
	"renderer",
	"pmove",
	"fs",
	"sound"
};

static const size_t debug_mask_strings_len = lengthof(debug_mask_strings);

/**
 * @brief Filters debugging output to the debug mask we have.
 */
static void Debug(const debug_mask_t mask, const char *msg) {

	if (!(quetoo.debug_mask & mask)) {
		return;
	}

	Print(msg);
}

static _Bool jmp_set = false;

/**
 * @brief Callback for subsystem failures. Depending on the severity, we may try to
 * recover, or we may shut the entire engine down and exit.
 */
static void Error(err_t err, const char *msg) {

	if (quetoo.debug_mask & DEBUG_BREAKPOINT) {
		SDL_TriggerBreakpoint();
	}

	Print(va("^1%s\n", msg));

	if (err == ERR_DROP && !jmp_set) {
		err = ERR_FATAL;
	}

	switch (err) {
		case ERR_DROP:
			Sv_ShutdownServer(msg);
			Cl_Disconnect();
			com_recursive = false;
			longjmp(env, err);
			break;

		case ERR_FATAL:
		default:
			Sys_Backtrace();
			Shutdown(msg);
			exit(err);
			break;
	}
}

/**
 * @brief Delegates all printing to the console.
 */
static void Print(const char *msg) {

	if (console_state.lock) {
		Con_Append(PRINT_HIGH, msg);
	} else {
		printf("%s", msg);
	}
}

/**
 * @brief Filters verbose output to when the `verbose` cvar is set.
 */
static void Verbose(const char *msg) {

	if (verbose->integer) {
		Print(msg);
	}
}

/**
 * @brief Prints the specified message with a colored accent.
 */
static void Warn(const char *msg) {

	if (quetoo.debug_mask & DEBUG_BREAKPOINT) {
		SDL_TriggerBreakpoint();
	}

	Print(va("^3%s", msg));
}

/**
 * @brief
 */
__attribute__((noreturn))
static void Quit_f(void) {

	Com_Shutdown("Server quit\n");
}

/**
 * @brief Parses a debug string and sets up the quetoo.debug value
 */
static void ParseDebugFlags(void) {
	
	// support old "debug 1/2" format
	if (debug->integer == 1) {
		quetoo.debug_mask = DEBUG_ANY;
	} else if (debug->integer == 2) {
		quetoo.debug_mask = DEBUG_ANY | DEBUG_BREAKPOINT;
	} else {
		const char *buf = debug->string, *c;
		_Bool first_token = true;

		while (true) {

			c = ParseToken(&buf);

			if (*c == '\0') {
				break;
			}

			char operation = '\0';

			// support adding/removing flags
			if (*c == '-' || *c == '+') {
				operation = *c;
				c++;
			}

			if (!operation) {

				// special case: if our first token isn't an explicit add/remove then
				// just reset the mask to 0 before setting the initial value.
				if (first_token) {
					quetoo.debug_mask = 0; // reset debug mask
				}

				operation = '+';
			}

			first_token = false;

			// figure out what the wanted flag is
			if (!g_ascii_strcasecmp(c, "none") ||
				c[0] == '0') {

				quetoo.debug_mask = 0;
				continue;
			}

			debug_mask_t wanted_flag = 0;

			// figure out what it is. Try special values first, then integral, then string.
			if (!g_ascii_strcasecmp(c, "breakpoint") || !g_ascii_strcasecmp(c, "bp")) {
				wanted_flag = DEBUG_BREAKPOINT;
			} else if (!g_ascii_strcasecmp(c, "any") || !g_ascii_strcasecmp(c, "all")) {
				wanted_flag = DEBUG_ANY;
			} else if (!(wanted_flag = (debug_mask_t) strtol(c, NULL, 10))) {

				for (uint32_t i = 0; i < debug_mask_strings_len; i++) {

					if (!g_ascii_strcasecmp(c, debug_mask_strings[i])) {
						wanted_flag = 1 << (i + 3);
						break;
					}
				}
			}

			// ignore invalid flag, who cares
			if (!wanted_flag) {
				continue;
			}

			if (operation == '+') {
				quetoo.debug_mask |= wanted_flag;
			} else {
				quetoo.debug_mask &= ~wanted_flag;
			}
		}
	}
}

/**
 * @brief
 */
static void Init(void) {

	SDL_Init(SDL_INIT_TIMER);

	Mem_Init();

	Cmd_Init();

	Cvar_Init();

	debug = Cvar_Add("debug", "0", 0, "Print debugging information");
	verbose = Cvar_Add("verbose", "0", 0, "Print verbose debugging information");

	dedicated = Cvar_Add("dedicated", "0", CVAR_NO_SET, "Run a dedicated server");
	if (strstr(Sys_ExecutablePath(), "-dedicated")) {
		Cvar_ForceSet("dedicated", "1");
	}

	if (dedicated->value) {
		Cvar_ForceSet("threads", "0");
	}

	game = Cvar_Add("game", DEFAULT_GAME, CVAR_LATCH | CVAR_SERVER_INFO, "The game module name");
	game->modified = g_strcmp0(game->string, DEFAULT_GAME);

	threads = Cvar_Add("threads", "0", CVAR_ARCHIVE, "Specifies the number of threads to create");
	threads->modified = false;

	time_demo = Cvar_Add("time_demo", "0", CVAR_LO_ONLY, "Benchmark and stress test");
	time_scale = Cvar_Add("time_scale", "1.0", CVAR_LO_ONLY, "Controls time lapse");

	const char *s = va("Quetoo %s %s %s", VERSION, __DATE__, BUILD_HOST);
	Cvar_Add("version", s, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

	quetoo.Debug = Debug;
	quetoo.Error = Error;
	quetoo.Print = Print;
	quetoo.Verbose = Verbose;
	quetoo.Warn = Warn;

	Fs_Init(true);

	Thread_Init(threads->integer);

	Con_Init();

	Cmd_Add("quit", Quit_f, CMD_SYSTEM, "Quit Quetoo");

	Netchan_Init();

	Sv_Init();

	Cl_Init();

	Com_Print("Quetoo %s %s %s initialized\n", VERSION, __DATE__, BUILD_HOST);

	// execute any +commands specified on the command line
	Cbuf_InsertFromDefer();
	Cbuf_Execute();

	// dedicated server, nothing specified, use Edge
	if (dedicated->value && !Com_WasInit(QUETOO_SERVER)) {
		Cbuf_AddText("map edge\n");
		Cbuf_Execute();
	}
}

/**
 * @brief Cleans up all game engine subsystems.
 */
static void Shutdown(const char *msg) {

	Com_Print("%s", msg);

	Sv_Shutdown(msg);

	Cl_Shutdown();

	Netchan_Shutdown();

	Thread_Shutdown();

	Con_Shutdown();

	Cvar_Shutdown();

	Cmd_Shutdown();

	Fs_Shutdown();

	Mem_Shutdown();

	SDL_Quit();
}

/**
 * @brief
 */
static void Frame(const uint32_t msec) {

	Cbuf_Execute();

	if (threads->modified) {
		threads->modified = false;

		Thread_Shutdown();
		Thread_Init(threads->integer);
	}

	if (game->modified) {
		game->modified = false;

		Fs_SetGame(game->string);

		if (Fs_Exists("autoexec.cfg")) {
			Cbuf_AddText("exec autoexec.cfg\n");
			Cbuf_Execute();
		}
	}

	Sv_Frame(msec);

	Cl_Frame(msec);
}

/**
 * @brief The entry point of the program.
 */
int32_t main(int32_t argc, char *argv[]) {
	static uint32_t old_time;
	uint32_t msec;

	printf("Quetoo %s %s %s\n", VERSION, __DATE__, BUILD_HOST);

	memset(&quetoo, 0, sizeof(quetoo));

	quetoo.Init = Init;
	quetoo.Shutdown = Shutdown;

	signal(SIGINT, Sys_Signal);
	signal(SIGILL, Sys_Signal);
	signal(SIGABRT, Sys_Signal);
	signal(SIGFPE, Sys_Signal);
	signal(SIGSEGV, Sys_Signal);
	signal(SIGTERM, Sys_Signal);
#ifndef _WIN32
	signal(SIGHUP, Sys_Signal);
	signal(SIGQUIT, Sys_Signal);
#endif

	Com_Init(argc, argv); // let's get it started in here

	jmp_set = true;

	while (true) { // this is our main loop

		if (setjmp(env)) { // an ERR_DROP was thrown
			Com_Debug(DEBUG_GENERIC, "Error detected, recovering..\n");
			continue;
		}

		if (time_scale->modified) {
			time_scale->modified = false;
			time_scale->value = Clamp(time_scale->value, 0.1, 3.0);
		}

		if (debug->modified) {
			debug->modified = false;
			ParseDebugFlags();
		}

		do {
			quetoo.ticks = SDL_GetTicks();
			msec = (quetoo.ticks - old_time) * time_scale->value;
		} while (msec < 1);

		Frame(msec);

		old_time = quetoo.ticks;
	}

	return 0;
}
