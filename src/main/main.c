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

#include <setjmp.h>
#include <signal.h>

#include "config.h"
#ifdef BUILD_CLIENT
#include "client/client.h"
extern cl_static_t cls;
#endif
#include "server/server.h"

jmp_buf environment;

quake2world_t quake2world;

static cvar_t *debug;
cvar_t *dedicated;
cvar_t *game;
static cvar_t *threads;
cvar_t *time_demo;
cvar_t *time_scale;
static cvar_t *verbose;

static void Debug(const char *msg);
static void Error(err_t err, const char *msg) __attribute__((noreturn));
static void Print(const char *msg);
static void Shutdown(const char *msg);
static void Verbose(const char *msg);
static void Warn(const char *msg);

/*
 * @brief Filters debugging output to when the `debug` cvar is set.
 */
static void Debug(const char *msg) {

	if (debug && !debug->integer)
		return;

	Print(msg);
}

/*
 * @brief Callback for subsystem failures. Depending on the severity, we may try to
 * recover, or we may shut the entire engine down and exit.
 */
static void Error(err_t err, const char *msg) {

	Com_Print("^1%s\n", msg);

	switch (err) {
		case ERR_NONE:
		case ERR_PRINT:
		case ERR_DROP:
			Sv_ShutdownServer(msg);

#ifdef BUILD_CLIENT
			Cl_Disconnect();
			if (err == ERR_NONE) {
				cls.key_state.dest = KEY_UI;
			} else {
				cls.key_state.dest = KEY_CONSOLE;
			}
#endif

			longjmp(environment, 0);
			break;

		case ERR_FATAL:
		default:
			Sys_Backtrace();
			Shutdown(msg);
			exit(err);
			break;
	}
}

/*
 * @brief Delegates all printing to the console.
 */
static void Print(const char *msg) {

	if (quake2world.time) {
		Con_Print(msg);
	} else {
		printf("%s", msg);
	}
}

/*
 * @brief Filters verbose output to when the `verbose` cvar is set.
 */
static void Verbose(const char *msg) {

	if (verbose && !verbose->integer) {
		return;
	}

	Print(msg);
}

/*
 * @brief Prints the specified message with a colored accent.
 */
static void Warn(const char *msg) {
	Print(va("^3%s", msg));
}

/*
 * @brief
 */
static void Quit_f(void) {
	Com_Shutdown("Server quit\n");
}

/*
 * @brief
 */
static void Init(void) {

	Mem_Init();

	Cmd_Init();

	Cvar_Init();

	Fs_Init(true);

	debug = Cvar_Get("debug", "0", 0, "Print debugging information");

#ifdef BUILD_CLIENT
	dedicated = Cvar_Get("dedicated", "0", CVAR_NO_SET, NULL);
#else
	dedicated = Cvar_Get("dedicated", "1", CVAR_NO_SET, NULL);
#endif

	game = Cvar_Get("game", DEFAULT_GAME, CVAR_LATCH | CVAR_SERVER_INFO, "The game module name");
	game->modified = g_strcmp0(game->string, DEFAULT_GAME);

	threads = Cvar_Get("threads", "4", CVAR_ARCHIVE, "Enable or disable multicore processing.");
	time_demo = Cvar_Get("time_demo", "0", CVAR_LO_ONLY, "Benchmark and stress test");
	time_scale = Cvar_Get("time_scale", "1.0", CVAR_LO_ONLY, "Controls time lapse");
	verbose = Cvar_Get("verbose", "0", 0, "Print verbose debugging information");

	const char *s = va("Quake2World %s %s %s", VERSION, __DATE__, BUILD_HOST);
	Cvar_Get("version", s, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

	Thread_Init(threads->integer);
	threads->modified = false;

	Con_Init();
	quake2world.time = Sys_Milliseconds();

	Cmd_Add("quit", Quit_f, CMD_SYSTEM, "Quit Quake2World");

	Netchan_Init();

	Sv_Init();

#ifdef BUILD_CLIENT
	Cl_Init();
#endif

	Com_Print("Quake2World %s %s %s initialized\n", VERSION, __DATE__, BUILD_HOST);

	// execute any +commands specified on the command line
	Cbuf_InsertFromDefer();

	// dedicated server, nothing specified, use Edge
	if (dedicated->value && !Com_WasInit(Q2W_SERVER)) {
		Cbuf_AddText("map edge\n");
		Cbuf_Execute();
	}
}

/*
 * @brief Cleans up all game engine subsystems.
 */
static void Shutdown(const char *msg) {

	Com_Print("%s", msg);

	Sv_Shutdown(msg);

#ifdef BUILD_CLIENT
	Cl_Shutdown();
#endif

	Netchan_Shutdown();

	Thread_Shutdown();

	Con_Shutdown();
	quake2world.time = 0; // short-circuit Print

	Cvar_Shutdown();

	Cmd_Shutdown();

	Fs_Shutdown();

	Mem_Shutdown();
}

/*
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

#ifdef BUILD_CLIENT
	Cl_Frame(msec);
#endif
}

/*
 * @brief The entry point of the program.
 */
int32_t main(int32_t argc, char **argv) {
	static uint32_t old_time;
	uint32_t msec;

	printf("Quake2World %s %s %s\n", VERSION, __DATE__, BUILD_HOST);

	memset(&quake2world, 0, sizeof(quake2world));

	if (setjmp(environment))
		Com_Error(ERR_FATAL, "Error during initialization.");

	quake2world.Debug = Debug;
	quake2world.Error = Error;
	quake2world.Print = Print;
	quake2world.Verbose = Verbose;
	quake2world.Warn = Warn;

	quake2world.Init = Init;
	quake2world.Shutdown = Shutdown;

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

	while (true) { // this is our main loop

		if (setjmp(environment)) { // an ERR_DROP or ERR_NONE was thrown
			Com_Debug("Error detected, recovering..\n");
			continue;
		}

		if (time_scale->modified) {
			if (time_scale->value < 0.1)
				time_scale->value = 0.1;
			else if (time_scale->value > 3.0)
				time_scale->value = 3.0;
		}

		do {
			quake2world.time = Sys_Milliseconds();
			msec = (quake2world.time - old_time) * time_scale->value;
		} while (msec < 1);

		Frame(msec);

		old_time = quake2world.time;
	}

	return 0;
}
