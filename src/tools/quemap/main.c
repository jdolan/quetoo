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

#include "qbsp.h"
#include "qlight.h"
#include "qmat.h"
#include "qzip.h"

#if defined(_WIN32)
	#if defined(__MINGW32__)
	 #define SDL_MAIN_HANDLED
	#endif

	#include <windows.h>
#endif

#include <SDL.h>

quetoo_t quetoo;

char map_base[MAX_QPATH]; // the base name (e.g. "edge")

char map_name[MAX_OS_PATH]; // the input map name (e.g. "maps/edge.map")
char bsp_name[MAX_OS_PATH]; // the input bsp name (e.g. "maps/edge.bsp")

_Bool verbose = false;
_Bool debug = false;
static _Bool is_monitor = false;

static void Print(const char *msg);

/**
 * @brief
 */
static void Debug(const debug_t debug, const char *msg) {

	if (!debug) {
		return;
	}

	Print(msg);
}

static void Shutdown(const char *msg);

/**
 * @brief
 */
static void Error(err_t err, const char *msg) __attribute__((noreturn));
static void Error(err_t err, const char *msg) {

	fprintf(stderr, "ERROR: Thread %lu: %s", SDL_ThreadID(), msg);

	fflush(stderr);

	if (SDL_ThreadID() == thread_main) {
		Shutdown(msg);
		exit(err);
	} else {
		raise(SIGINT);
		exit(err);
	}
}

/**
 * @brief Print to stdout and, if not escaped, to the monitor socket.
 */
static void Print(const char *msg) {

	if (msg) {
		if (*msg == '@') {
			fputs(msg + 1, stdout);
		} else {
			fputs(msg, stdout);
			Mon_SendMessage(MON_PRINT, msg);
		}

		fflush(stdout);
	}
}

/**
 * @brief Print a verbose message to stdout and, unless escaped, to the monitor
 * socket.
 */
static void Verbose(const char *msg) {

	if (!verbose) {
		return;
	}

	Print(msg);
}

/**
 * @brief Print a warning message to stdout and, if not escaped, to the monitor
 * socket.
 */
static void Warn(const char *msg) {

	if (msg) {
		if (*msg == '@') {
			fprintf(stderr, "WARNING: %s", msg + 1);
		} else {
			fprintf(stderr, "WARNING: %s", msg);
			Mon_SendMessage(MON_WARN, va("WARNING: %s", msg));
		}

		fflush(stderr);
	}
}

/**
 * @brief Initializes subsystems quemap relies on.
 */
static void Init(void) {

	SDL_Init(SDL_INIT_TIMER);

	Com_InitSubsystem(QUEMAP);

	Mem_Init();

	Mon_Init();

	Fs_Init(FS_AUTO_LOAD_ARCHIVES);

	Com_Print("Quemap %s %s %s initialized\n", VERSION, BUILD, REVISION);
}

/**
 * @brief Shuts down subsystems.
 */
static void Shutdown(const char *msg) {

	Com_QuitSubsystem(QUEMAP);

	Thread_Shutdown();

	Mon_Shutdown(msg);

	Fs_Shutdown();

	Mem_Shutdown();

	SDL_Quit();

#if defined(_WIN32)
	FreeConsole();
#endif
}

/**
 * @brief
 */
static void Check_BSP_Options(int32_t argc) {

	for (int32_t i = argc; i < Com_Argc(); i++) {
		if (!g_strcmp0(Com_Argv(i), "--micro-volume")) {
			micro_volume = atof(Com_Argv(i + 1));
			Com_Verbose("micro_volume = %f\n", micro_volume);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--no-csg")) {
			Com_Verbose("no_csg = true\n");
			no_csg = true;
		} else if (!g_strcmp0(Com_Argv(i), "--no-detail")) {
			Com_Verbose("no_detail = true\n");
			no_detail = true;
		} else if (!g_strcmp0(Com_Argv(i), "--no-liquid")) {
			Com_Verbose("no_liquid = true\n");
			no_liquid = true;
		} else if (!g_strcmp0(Com_Argv(i), "--no-phong")) {
			Com_Verbose("no_phong = true\n");
			no_phong = true;
		} else if (!g_strcmp0(Com_Argv(i), "--no-prune")) {
			Com_Verbose("no_prune = true\n");
			no_prune = true;
		} else if (!g_strcmp0(Com_Argv(i), "--no-tjunc")) {
			Com_Verbose("no_tjunc = true\n");
			no_tjunc = true;
		} else if (!g_strcmp0(Com_Argv(i), "--no-weld")) {
			Com_Verbose("no_weld = true\n");
			no_weld = true;
		} else if (!g_strcmp0(Com_Argv(i), "--only-ents")) {
			Com_Verbose("only_ents = true\n");
			only_ents = true;
		} else {
			break;
		}
	}
}

/**
 * @brief
 */
static void Check_LIGHT_Options(int32_t argc) {

	for (int32_t i = argc; i < Com_Argc(); i++) {
		if (!g_strcmp0(Com_Argv(i), "--no-indirect")) {
			indirect = false;
			Com_Verbose("indirect: false\n");
		} else if (!g_strcmp0(Com_Argv(i), "--antialias")) {
			antialias = true;
			Com_Verbose("antialias: true\n");
		} else if (!g_strcmp0(Com_Argv(i), "--radiosity")) {
			radiosity = atof(Com_Argv(i + 1));
			Com_Verbose("radiosity: %g\n", radiosity);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--bounce")) {
			num_bounces = (int32_t) CLAMP(strtol(Com_Argv(i + 1), NULL, 10), 1, MAX_BOUNCES);
			Com_Verbose("bounces: %d\n", num_bounces);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--brightness")) {
			brightness = atof(Com_Argv(i + 1));
			Com_Verbose("brightness: %g\n", brightness);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--saturation")) {
			saturation = atof(Com_Argv(i + 1));
			Com_Verbose("saturation: %g\n", saturation);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--contrast")) {
			contrast = atof(Com_Argv(i + 1));
			Com_Verbose("contrast: %g\n", contrast);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--luxel-size")) {
			luxel_size = (int32_t) strtol(Com_Argv(i + 1), NULL, 10);
			Com_Verbose("luxel size: %d\n", luxel_size);
			i++;
		} else if (!g_strcmp0(Com_Argv(i), "--patch-size")) {
			patch_size = (int32_t) strtol(Com_Argv(i + 1), NULL, 10);
			Com_Verbose("patch size: %d\n", patch_size);
			i++;
		} else {
			break;
		}
	}

	patch_size = Maxf(patch_size, luxel_size);
}

/**
 * @brief
 */
static void Check_ZIP_Options(int32_t argc) {

	for (int32_t i = argc; i < Com_Argc(); i++) {

		if (!g_strcmp0(Com_Argv(i), "--include-shared")) {
			include_shared = true;
			Com_Verbose("Including shared assets\n");
		} else if (!g_strcmp0(Com_Argv(i), "--update")) {
			update_zip = true;
			Com_Verbose("Updating existing zip archive\n");
		} else {
			break;
		}
	}
}

/**
 * @brief
 */
static void Check_MAT_Options(int32_t argc) {
}

/**
 * @brief
 */
static void PrintHelpMessage(void) {
	Com_Print("General options\n");
	Com_Print("-v --verbose\n");
	Com_Print("-d --debug\n");
	Com_Print("-t --threads <int> - Specify the number of worker threads (default auto)\n");
	Com_Print("-p --path <game directory> - add the path to the search directory\n");
	Com_Print("-w --wpath <game directory> - add the write path to the search directory\n");
	Com_Print("-connect <host> - use GtkRadiant's BSP monitoring server\n");
	Com_Print("\n");

	Com_Print("-mat               MAT stage options:\n");
	Com_Print("\n");

	Com_Print("-bsp               BSP stage options:\n");
	Com_Print(" --micro_volume <float>\n");
	Com_Print(" --no-csg - don't subtract brushes\n");
	Com_Print(" --no-detail - skip detail brushes\n");
	Com_Print(" --no-liquid - skip liquid brushes\n");
	Com_Print(" --no-phong - don't apply Phong shading\n");
	Com_Print(" --no-prune - don't prune unused nodes\n");
	Com_Print(" --no-tjunc - don't fix T-junctions\n");
	Com_Print(" --no-weld - don't weld vertices\n");
	Com_Print(" --only-ents - only update the entity string from the .map\n");
	Com_Print("\n");

	Com_Print("-light             LIGHT stage options:\n");
	Com_Print(" --antialias - calculate extra lighting samples and average them\n");
	Com_Print(" --indirect - calculate indirect lighting\n");
	Com_Print(" --bounce <integer> - indirect lighting bounces (default 1)\n");
	Com_Print(" --radiosity <float> - radiosity level (default 0.125)\n");
	Com_Print(" --brightness <float> - brightness (default 1.0)\n");
	Com_Print(" --contrast <float> - contrast (default 1.0)\n");
	Com_Print(" --saturation <float> - saturation (default 1.0)\n");
	Com_Print(" --luxel-size <float> - luxel size (default 4)\n");
	Com_Print(" --patch-size <float> - patch size (default 16)\n");
	Com_Print("\n");

	Com_Print("-zip               ZIP stage options:\n");
	Com_Print(" --include-shared - include assets from shared archives\n");
	Com_Print(" --update - Update the existing archive instead of authoring a new one\n");
	Com_Print("\n");

	Com_Print("Examples:\n");
	Com_Print("Materials file generation:\n"
			  " quemap -mat maps/my.map\n");
	Com_Print("Development compile with rough lighting:\n"
			  " quemap -bsp -light maps/my.map\n");
	Com_Print("Final compile with expensive lighting:\n"
	          " quemap -bsp -light --antialias maps/my.map\n");
	Com_Print("Zip file generation:\n"
			  " quemap -zip maps/my.bsp\n");
	Com_Print("\n");
}

/**
 * @brief
 */
int32_t main(int32_t argc, char **argv) {
	int32_t num_threads = 0;
	_Bool do_mat = false;
	_Bool do_bsp = false;
	_Bool do_light = false;
	_Bool do_zip = false;

	printf("Quemap %s %s %s\n", VERSION, BUILD, REVISION);

	memset(&quetoo, 0, sizeof(quetoo));

	quetoo.Debug = Debug;
	quetoo.Error = Error;
	quetoo.Print = Print;
	quetoo.Verbose = Verbose;
	quetoo.Warn = Warn;

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

	Com_Init(argc, argv);

	// general options
	for (int32_t i = 1; i < Com_Argc(); i++) {

		if (!g_strcmp0(Com_Argv(i), "-h") || !g_strcmp0(Com_Argv(i), "--help")) {
			PrintHelpMessage();
			Com_Shutdown(NULL);
		}

		if (!g_strcmp0(Com_Argv(i), "-v") || !g_strcmp0(Com_Argv(i), "--verbose")) {
			verbose = true;
			continue;
		}

		if (!g_strcmp0(Com_Argv(i), "-d") || !g_strcmp0(Com_Argv(i), "--debug")) {
			Com_SetDebug("all");
			debug = true;
			continue;
		}

		if (!g_strcmp0(Com_Argv(i), "-t") || !g_strcmp0(Com_Argv(i), "--threads")) {
			num_threads = atoi(Com_Argv(i + 1));
			continue;
		}

		if (!g_strcmp0(Com_Argv(i), "-connect")) { // GtkRadiant hard-codes this option
			is_monitor = Mon_Connect(Com_Argv(i + 1));
			continue;
		}
	}

	// read compiling options
	for (int32_t i = 1; i < Com_Argc(); i++) {

		if (!g_strcmp0(Com_Argv(i), "-mat")) {
			do_mat = true;
			Check_MAT_Options(i + 1);
		}

		if (!g_strcmp0(Com_Argv(i), "-bsp")) {
			do_bsp = true;
			Check_BSP_Options(i + 1);
		}

		if (!g_strcmp0(Com_Argv(i), "-light")) {
			do_light = true;
			Check_LIGHT_Options(i + 1);
		}

		if (!g_strcmp0(Com_Argv(i), "-zip")) {
			do_zip = true;
			Check_ZIP_Options(i + 1);
		}
	}

	if (!do_bsp && !do_light && !do_mat && !do_zip) {
		Com_Error(ERROR_FATAL, "No action specified. Try %s --help\n", Com_Argv(0));
	}

	Thread_Init(num_threads);
	Com_Print("Using %d threads\n", Thread_Count());

	const char *filename = Com_Argv(Com_Argc() - 1);
	if (g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		gchar *dirname = g_path_get_dirname(filename);
		if (dirname) {
			Fs_AddToSearchPath(dirname);
			filename += strlen(dirname);
			g_free(dirname);
		}
	}

	// resolve the base name, used for all output files
	gchar *basename = g_path_get_basename(filename);
	StripExtension(basename, map_base);
	g_free(basename);

	StripExtension(filename, map_name);
	g_strlcat(map_name, ".map", sizeof(map_name));

	if (!Fs_Exists(map_name)) {
		g_snprintf(map_name, sizeof(map_name), "maps/%s.map", map_base);
	}

	StripExtension(filename, bsp_name);
	g_strlcat(bsp_name, ".bsp", sizeof(bsp_name));

	if (!Fs_Exists(bsp_name)) {
		g_snprintf(bsp_name, sizeof(bsp_name), "maps/%s.bsp", map_base);
	}

	// start timer
	const uint32_t start = SDL_GetTicks();

	if (do_mat) {
		MAT_Main();
	}
	if (do_bsp) {
		BSP_Main();
	}
	if (do_light) {
		LIGHT_Main();
	}
	if (do_zip) {
		ZIP_Main();
	}

	// emit time
	const uint32_t end = SDL_GetTicks();
	Com_Print("\n%s finished in %dms\n", Com_Argv(0), end - start);

	Com_Shutdown(NULL);
}
