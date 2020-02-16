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

#include <SDL_assert.h>

#include "config.h"
#include "client/client.h"
#include "server/server.h"

static jmp_buf env;

quetoo_t quetoo;

static cvar_t *verbose;
static cvar_t *version;

cvar_t *dedicated;
cvar_t *game;
cvar_t *ai;
cvar_t *threads;
cvar_t *time_demo;
cvar_t *time_scale;

static void Debug(const debug_t debug, const char *msg);
static void Error(err_t err, const char *msg) __attribute__((noreturn));
static void Print(const char *msg);
static void Shutdown(const char *msg);
static void Verbose(const char *msg);
static void Warn(const char *msg);

/**
 * @brief
 */
static void Debug_f(void) {

	if (Cmd_Argc() == 1) {
		Com_Print("Set or toggle debug categories.\nUsage: debug [category] ..\n");
		Com_Print("Categories:\n");
		const char *categories[] = {
			"ai",
			"cgame",
			"client",
			"collision",
			"console",
			"filesystem",
			"game",
			"net",
			"pmove_client",
			"pmove_server",
			"renderer",
			"server",
			"sound",
			"ui",
		};

		Com_Print("  none\n");

		for (size_t i = 0; i < lengthof(categories); i++) {
			int32_t color = ESC_COLOR_WHITE;
			switch (1 << i) {
				case DEBUG_AI:
				case DEBUG_GAME:
				case DEBUG_SERVER:
					color = ESC_COLOR_CYAN;
					break;
				case DEBUG_CGAME:
				case DEBUG_CLIENT:
				case DEBUG_RENDERER:
				case DEBUG_SOUND:
				case DEBUG_UI:
					color = ESC_COLOR_MAGENTA;
					break;
				case DEBUG_COLLISION:
				case DEBUG_CONSOLE:
				case DEBUG_FILESYSTEM:
				case DEBUG_NET:
					color = ESC_COLOR_YELLOW;
					break;
				case DEBUG_PMOVE_CLIENT:
				case DEBUG_PMOVE_SERVER:
					color = ESC_COLOR_BLUE;
					break;
				default:
					break;
			}
			Com_Print("  ^%d%s^7\n", color, categories[i]);
		}

		Com_Print("  ^2all^7\n");
		Com_Print("  ^1breakpoint^7\n");

		return;
	}

	Com_SetDebug(Cmd_Args());

	Com_Print("Debug mask: %s\n", Com_GetDebug());
}

/**
 * @brief Prints debug output using colored escapes based on the debug category.
 */
static void Debug(const debug_t debug, const char *msg) {

	int32_t color = ESC_COLOR_WHITE;
	switch (debug) {
		case DEBUG_AI:
		case DEBUG_GAME:
		case DEBUG_SERVER:
			color = ESC_COLOR_CYAN;
			break;
		case DEBUG_CGAME:
		case DEBUG_CLIENT:
		case DEBUG_RENDERER:
		case DEBUG_SOUND:
		case DEBUG_UI:
			color = ESC_COLOR_MAGENTA;
			break;
		case DEBUG_COLLISION:
		case DEBUG_CONSOLE:
		case DEBUG_FILESYSTEM:
		case DEBUG_NET:
			color = ESC_COLOR_YELLOW;
			break;
		case DEBUG_PMOVE_CLIENT:
		case DEBUG_PMOVE_SERVER:
			color = ESC_COLOR_BLUE;
			break;
		default:
			break;
	}

	Print(va("^%d%s", color, msg));
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

	if (err == ERROR_DROP && !jmp_set) {
		err = ERROR_FATAL;
	}

	switch (err) {
		case ERROR_DROP:
			Sv_ShutdownServer(msg);
			Cl_Disconnect();
			Cl_Drop(msg);
			quetoo.recursive_error = false;
			longjmp(env, err);
			break;

		case ERROR_FATAL:
		default:
			Sys_Backtrace(msg);
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
static void Quit_f(void) __attribute__((noreturn));
static void Quit_f(void) {
	Com_Shutdown("Server quit\n");
}

static const char *mem_tag_names[MEM_TAG_TOTAL] = {
	"default",
	"server",
	"ai",
	"game",
	"game_level",
	"client",
	"renderer",
	"sound",
	"ui",
	"cgame",
	"cgame_level",
	"material",
	"cmd",
	"cvar",
	"collision",
	"bsp",
	"fs"
};

/**
 * @brief
 */
static void MemStats_f(void) {

	GArray *stats = Mem_Stats();

	Com_Print("Memory stats:\n");

	size_t sum = 0, reported_total = 0;

	for (size_t i = 0; i < stats->len; i++) {

		mem_stat_t *stat_i = &g_array_index(stats, mem_stat_t, i);
		const char *tag_name;

		if (stat_i->tag == -1) {
			Com_Print("total: %" PRIuPTR " bytes\n", stat_i->size);
			reported_total = stat_i->size;
			continue;
		} else if (stat_i->tag < MEM_TAG_TOTAL) {
			tag_name = mem_tag_names[stat_i->tag];
		} else {
			tag_name = va("#%d", stat_i->tag);
		}

		Com_Print(" [%s] %" PRIuPTR " bytes - %" PRIuPTR " blocks\n", tag_name, stat_i->size, stat_i->count);
		sum += stat_i->size;
	}

	if (sum != reported_total) {
		Com_Print("WARNING: %" PRIuPTR " bytes summed vs %" PRIuPTR " bytes reported!\n", sum, reported_total);
	}

	Com_Print(" [console] approx. %" PRIuPTR " bytes - approx. %" PRIu32 " blocks\n", console_state.size, console_state.strings.length);

	g_array_free(stats, true);
}

/**
 * @brief
 */
static void Init(void) {

	SDL_Init(SDL_INIT_TIMER);

	Mem_Init();

	Cmd_Init();

	Cvar_Init();

	char *s = va("%s %s %s", VERSION, BUILD_HOST, REVISION);
	version = Cvar_Add("version", s, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

	verbose = Cvar_Add("verbose", "0", 0, "Print verbose debugging information");

	dedicated = Cvar_Add("dedicated", "0", CVAR_NO_SET, "Run a dedicated server");
	if (strstr(Sys_ExecutablePath(), "-dedicated")) {
		Cvar_ForceSetInteger(dedicated->name, 1);
	}

	game = Cvar_Add("game", DEFAULT_GAME, CVAR_LATCH | CVAR_SERVER_INFO, "The game module name");
	game->modified = g_strcmp0(game->string, DEFAULT_GAME);

	ai = Cvar_Add("ai", DEFAULT_AI, CVAR_LATCH | CVAR_SERVER_INFO, "The AI module name");
	ai->modified = g_strcmp0(ai->string, DEFAULT_AI);

	threads = Cvar_Add("threads", "0", CVAR_ARCHIVE, "Specifies the number of threads to create");
	threads->modified = false;

	time_demo = Cvar_Add("time_demo", "0", CVAR_DEVELOPER, "Benchmark and stress test");
	time_scale = Cvar_Add("time_scale", "1.0", CVAR_DEVELOPER, "Controls time lapse");

	quetoo.Debug = Debug;
	quetoo.Error = Error;
	quetoo.Print = Print;
	quetoo.Verbose = Verbose;
	quetoo.Warn = Warn;

	Fs_Init(FS_AUTO_LOAD_ARCHIVES);

	Thread_Init(threads->integer);

	Con_Init();

	Cmd_Add("mem_stats", MemStats_f, CMD_SYSTEM, "Print memory stats");
	Cmd_Add("debug", Debug_f, CMD_SYSTEM, "Control debugging output");
	Cmd_Add("quit", Quit_f, CMD_SYSTEM, "Quit Quetoo");

	Netchan_Init();

	Sv_Init();

	Cl_Init();

	Com_Print("Quetoo %s initialized", version->string);

	// reset debug value since Cbuf may change it from Com's "all" init
	Com_SetDebug("0");

	// execute any +commands specified on the command line
	Cbuf_InsertFromDefer();
	Cbuf_Execute();

	// if we don't have console fonts, the user should run the updater
	if (!Fs_Exists("fonts/small.tga")) {
		Com_Error(ERROR_FATAL, "Please run quetoo-update.\n");
	}

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

		if (setjmp(env)) { // an ERROR_DROP was thrown
			Com_Warn("Error detected, recovering..\n");
			continue;
		}

		if (time_scale->modified) {
			time_scale->modified = false;
			time_scale->value = Clampf(time_scale->value, 0.1, 3.0);
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
