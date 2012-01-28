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

#include <SDL/SDL.h>
#include "q2wmap.h"

quake2world_t quake2world;

char map_name[MAX_OSPATH];
char bsp_name[MAX_OSPATH];
char outbase[MAX_OSPATH];

boolean_t verbose;
boolean_t debug;
boolean_t legacy;

/* BSP */
extern boolean_t noprune;
extern boolean_t nodetail;
extern boolean_t fulldetail;
extern boolean_t onlyents;
extern boolean_t nomerge;
extern boolean_t nowater;
extern boolean_t nofill;
extern boolean_t nocsg;
extern boolean_t noweld;
extern boolean_t noshare;
extern boolean_t nosubdivide;
extern boolean_t notjunc;
extern boolean_t noopt;
extern boolean_t leaktest;
extern boolean_t verboseentities;

extern int block_xl, block_xh, block_yl, block_yh;
extern vec_t microvolume;
extern int subdivide_size;

/* VIS */
extern boolean_t fastvis;
extern boolean_t nosort;

/* LIGHT */
extern boolean_t extra_samples;
extern float brightness;
extern float saturation;
extern float contrast;
extern float surface_scale;
extern float entity_scale;

#ifdef _WIN32

static HANDLE Console; //windows console
static FILE *output_file; //file output
#define HORIZONTAL	45							//console size and buffer
#define VERTICAL		70						//console size and buffer
static int input_index_h, input_index_v; //pdcurses print location
static char title[64]; //window bar title (updates to show status)

#ifdef HAVE_CURSES
#include <curses.h>

/*
 * PDCursesInit
 */
static void PDCursesInit(void) {
	stdscr = initscr(); // initialize the ncurses window
	resize_term(HORIZONTAL, VERTICAL); // resize the console
	cbreak(); // disable input line buffering
	noecho(); // don't show type characters
	keypad(stdscr, TRUE); // enable special keys
	nodelay(stdscr, TRUE); // non-blocking input
	curs_set(1); // enable the cursor

	if(has_colors() == TRUE) {
		start_color();
		// this is ncurses-specific
		use_default_colors();
		// COLOR_PAIR(0) is terminal default
		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_GREEN, -1);
		init_pair(3, COLOR_YELLOW, -1);
		init_pair(4, COLOR_BLUE, -1);
		init_pair(5, COLOR_CYAN, -1);
		init_pair(6, COLOR_MAGENTA, -1);
		init_pair(7, -1, -1);
	}
}
#endif

/*
 * OpenWin32Console
 */
static void OpenWin32Console(void) {
	AllocConsole();
	Console = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0,0,CONSOLE_TEXTMODE_BUFFER,0);
	SetConsoleActiveScreenBuffer(Console);

	snprintf(title, sizeof(title), "Q2W Map Compiler");
	SetConsoleTitle(title);

	freopen("CON", "wt", stdout);
	freopen("CON", "wt", stderr);
	freopen("CON", "rt", stdin);

	if((output_file = fopen ("bsp_output.txt","w")) == NULL)
	Com_Error(ERR_FATAL, "Could not open bsp_compiler.txt\n");

	input_index_h = 0; // zero the index counters
	input_index_v = 0; // zero the index counters
#ifdef HAVE_CURSES
	PDCursesInit(); // initialize the pdcurses window
#endif
}

/*
 * CloseWin32Console
 */
static void CloseWin32Console(void) {
	Fs_CloseFile(output_file); // close the open file stream
	CloseHandle(Console);
	FreeConsole();
}

/*
 * Debug
 */
static void Debug(const char *msg) {
	unsigned long cChars;

	if(!debug)
	return;

	fprintf(output_file, "%s", msg);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}

/*
 * Error
 */
static void Error(err_t err, const char *msg) {
	const char *e = "************ ERROR ************\n";
	unsigned long cChars;

	fprintf(output_file, "%s", e);
	fprintf(output_file, "%s", msg); // output to a file

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), e, lstrlen(e), &cChars, NULL);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);

#ifdef HAVE_CURSES
	do { // don't quit, leave compiler error on screen until a key is pressed
	}while(getch() == ERR);

	endwin(); // shutdown pdcurses
#endif
	CloseWin32Console(); // close the console

	Z_Shutdown();
	exit(1);
}

/*
 * Print
 */
static void Print(const char *msg) {
	unsigned long cChars;

	fprintf(output_file, "%s", msg); // output to a file

#ifdef HAVE_CURSES
	if(verbose || debug) { // verbose and debug output doesn't need fancy output - use WriteConsole() instead
		WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
	}
	else { // fancy colored pdcurses output for normal compiling
		char copymsg[2];
		int n;

		for(n = 0; n < lstrlen(msg); n++) { // process the entire string (msg)
			if (msg[n] == '\n') {
				input_index_h++; // start a new line
				input_index_v = 0; // start at the beginning of the new line
			}
			else { // otherwise continue processing the current line
				copymsg[0] = msg[n]; // copy a character
				if(input_index_h == 0) { // highlight the first line (q2wmap version, date, mingw32 build)
					attron(COLOR_PAIR(3)); // bright yellow
					attron(A_BOLD);
				}
				// highlight compiler progression (1... 2... 3... 4... 5... 6... 7... 8... 9... 10...)
				else if(input_index_h == 4 || input_index_h == 5 || input_index_h == 11 || input_index_h == 12 ||
						input_index_h == 21 || input_index_h == 22 || input_index_h == 23) {
					attron(COLOR_PAIR(1)); // bright red
					attron(A_BOLD);
				}
				else
				attroff(COLOR_PAIR(3)); // turn off attributes

				// finally print our processed character on the console
				mvwprintw(stdscr, input_index_h, input_index_v, copymsg);
				refresh();

				input_index_v++; // advance to the next character position
			}
		}
	}
#else
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
#endif /* HAVE_CURSES */
}

/*
 * Verbose
 */
static void Verbose(const char *msg) {
	unsigned long cChars;

	if(!verbose)
	return;

	fprintf(output_file, "%s", msg);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}

/*
 * Warn
 */
static void Warn(const char *msg) {
	unsigned long cChars;
	const char *w = "WARNING: ";

	fprintf(output_file, "%s", w);
	fprintf(output_file, "%s", msg);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), w, lstrlen(w), &cChars, NULL);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}

#else /* _WIN32 */

/*
 * Debug
 */
static void Debug(const char *msg) {

	if (!debug)
		return;

	printf("%s", msg);
}

/*
 * Error
 */
static void Error(err_t err, const char *msg) __attribute__((noreturn));
static void Error(err_t err, const char *msg) {

	fprintf(stderr, "************ ERROR ************\n");
	fprintf(stderr, "%s", msg);

	Z_Shutdown();
	exit(1);
}

/*
 * Print
 */
static void Print(const char *msg) {
	printf("%s", msg);
}

/*
 * Verbose
 */
static void Verbose(const char *msg) {

	if (!verbose)
		return;

	printf("%s", msg);
}

/*
 * Warn
 */
static void Warn(const char *msg) {

	fprintf(stderr, "WARNING: ");
	fprintf(stderr, "%s", msg);
}

#endif /* _WIN32 */

/*
 * Check_BSP_Options
 */
static int Check_BSP_Options(int argc, char **argv) {
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-noweld")) {
			Com_Verbose("noweld = true\n");
			noweld = true;
		} else if (!strcmp(argv[i], "-nocsg")) {
			Com_Verbose("nocsg = true\n");
			nocsg = true;
		} else if (!strcmp(argv[i], "-noshare")) {
			Com_Verbose("noshare = true\n");
			noshare = true;
		} else if (!strcmp(argv[i], "-notjunc")) {
			Com_Verbose("notjunc = true\n");
			notjunc = true;
		} else if (!strcmp(argv[i], "-nowater")) {
			Com_Verbose("nowater = true\n");
			nowater = true;
		} else if (!strcmp(argv[i], "-noopt")) {
			Com_Verbose("noopt = true\n");
			noopt = true;
		} else if (!strcmp(argv[i], "-noprune")) {
			Com_Verbose("noprune = true\n");
			noprune = true;
		} else if (!strcmp(argv[i], "-nofill")) {
			Com_Verbose("nofill = true\n");
			nofill = true;
		} else if (!strcmp(argv[i], "-nomerge")) {
			Com_Verbose("nomerge = true\n");
			nomerge = true;
		} else if (!strcmp(argv[i], "-nosubdivide")) {
			Com_Verbose("nosubdivide = true\n");
			nosubdivide = true;
		} else if (!strcmp(argv[i], "-nodetail")) {
			Com_Verbose("nodetail = true\n");
			nodetail = true;
		} else if (!strcmp(argv[i], "-fulldetail")) {
			Com_Verbose("fulldetail = true\n");
			fulldetail = true;
		} else if (!strcmp(argv[i], "-onlyents")) {
			Com_Verbose("onlyents = true\n");
			onlyents = true;
		} else if (!strcmp(argv[i], "-micro")) {
			microvolume = atof(argv[i + 1]);
			Com_Verbose("microvolume = %f\n", microvolume);
			i++;
		} else if (!strcmp(argv[i], "-leaktest")) {
			Com_Verbose("leaktest = true\n");
			leaktest = true;
		} else if (!strcmp(argv[i], "-verboseentities")) {
			Com_Verbose("verboseentities = true\n");
			verboseentities = true;
		} else if (!strcmp(argv[i], "-subdivide")) {
			subdivide_size = atoi(argv[i + 1]);
			Com_Verbose("subdivide_size = %d\n", subdivide_size);
			i++;
		} else if (!strcmp(argv[i], "-block")) {
			block_xl = block_xh = atoi(argv[i + 1]);
			block_yl = block_yh = atoi(argv[i + 2]);
			Com_Verbose("block: %i,%i\n", block_xl, block_yl);
			i += 2;
		} else if (!strcmp(argv[i], "-blocks")) {
			block_xl = atoi(argv[i + 1]);
			block_yl = atoi(argv[i + 2]);
			block_xh = atoi(argv[i + 3]);
			block_yh = atoi(argv[i + 4]);
			Com_Verbose("blocks: %i,%i to %i,%i\n", block_xl, block_yl,
					block_xh, block_yh);
			i += 4;
		} else if (!strcmp(argv[i], "-tmpout")) {
			strcpy(outbase, "/tmp");
		} else
			break;
	}
	return 0;
}

/*
 * Check_VIS_Options
 */
static int Check_VIS_Options(int argc, char **argv) {
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-fast")) {
			Com_Verbose("fastvis = true\n");
			fastvis = true;
		} else if (!strcmp(argv[i], "-nosort")) {
			Com_Verbose("nosort = true\n");
			nosort = true;
		} else
			break;
	}

	return 0;
}

/*
 * Check_LIGHT_Options
 */
static int Check_LIGHT_Options(int argc, char **argv) {
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-extra")) {
			extra_samples = true;
			Com_Verbose("extra samples = true\n");
		} else if (!strcmp(argv[i], "-brightness")) {
			brightness = atof(argv[i + 1]);
			Com_Verbose("brightness at %f\n", brightness);
			i++;
		} else if (!strcmp(argv[i], "-saturation")) {
			saturation = atof(argv[i + 1]);
			Com_Verbose("saturation at %f\n", saturation);
			i++;
		} else if (!strcmp(argv[i], "-contrast")) {
			contrast = atof(argv[i + 1]);
			Com_Verbose("contrast at %f\n", contrast);
			i++;
		} else if (!strcmp(argv[i], "-surface")) {
			surface_scale *= atof(argv[i + 1]);
			Com_Verbose("surface light scale at %f\n", surface_scale);
			i++;
		} else if (!strcmp(argv[i], "-entity")) {
			entity_scale *= atof(argv[i + 1]);
			Com_Verbose("entity light scale at %f\n", entity_scale);
			i++;
		} else
			break;
	}

	return 0;
}

/*
 * Check_PAK_Options
 */
static int Check_PAK_Options(int argc, char **argv) {
	return 0;
}

/*
 * Check_MAT_Options
 */
static int Check_MAT_Options(int argc, char **argv) {
	return 0;
}

/*
 * PrintHelpMessage
 */
static void PrintHelpMessage(void) {
	Print("General options\n");
	Print("-v -verbose\n");
	Print("-l -legacy            Compile a legacy Quake2 map\n");
	Print("-d -debug\n");
	Print("-t -threads <int>\n");

	Print("\n");
	Print("-bsp               Binary space partitioning (BSPing) options:\n");
	Print(" -block <int> <int>\n");
	Print(" -blocks <int> <int> <int> <int>\n");
	Print(
			" -fulldetail - don't treat details (and trans surfaces) as details\n");
	Print(" -leaktest\n");
	Print(" -micro <float>\n");
	Print(" -nocsg\n");
	Print(" -nodetail - skip detail brushes\n");
	Print(" -nofill\n");
	Print(" -nomerge - skip node face merging\n");
	Print(" -noopt\n");
	Print(" -noprune - don't prune (or cut) nodes\n");
	Print(" -noshare\n");
	Print(" -nosubdivide\n");
	Print(" -notjunc\n");
	Print(" -nowater - skip water brushes in compilation\n");
	Print(" -noweld\n");
	Print(" -onlyents - modify existing bsp file with entities from map file\n");
	Print(
			" -subdivide <int> -subdivide brushes for better light effects (but higher polycount)\n");
	Print(" -tmpout\n");
	Print(" -verboseentities - also be verbose about submodels (entities)\n");
	Print("\n");
	Print("-vis               VIS stage options:\n");
	Print(" -fast\n");
	Print(" -level\n");
	Print(" -nosort\n");
	Print("\n");
	Print("-light             Lighting stage options:\n");
	Print(" -contrast <float> - contrast factor\n");
	Print(" -entity <float> - entity light scaling\n");
	Print(" -extra - extra light samples\n");
	Print(" -brightness <float> - brightness factor\n");
	Print(" -saturation <float> - saturation factor\n");
	Print(" -surface <float> - surface light scaling\n");
	Print("\n");
	Print("-pak               PAK file options:\n");
	Print("\n");
	Print("Examples:\n");
	Print("Standard full compile:\n q2wmap -bsp -vis -light maps/my.map\n");
	Print(
			"Fast vis, extra light, two threads:\n q2wmap -t 2 -bsp -vis -fast -light -extra maps/my.map\n");
	Print("\n");
}

/*
 * main
 */
int main(int argc, char **argv) {
	int i;
	int r = 0;
	int total_time;
	time_t start, end;
	int alt_argc;
	char *c, **alt_argv;
	boolean_t do_bsp = false;
	boolean_t do_vis = false;
	boolean_t do_light = false;
	boolean_t do_mat = false;
	boolean_t do_pak = false;

	memset(&quake2world, 0, sizeof(quake2world));

	quake2world.Debug = Debug;
	quake2world.Error = Error;
	quake2world.Print = Print;
	quake2world.Verbose = Verbose;
	quake2world.Warn = Warn;

#ifdef _WIN32
	OpenWin32Console(); //	initialize the windows console
#endif

	Com_Print("Quake2World Map %s %s %s\n", VERSION, __DATE__, BUILD_HOST);

	if (argc < 2) { // print help and exit
		PrintHelpMessage();
		return 0;
	}

	// init core facilities
	Z_Init();

	Cvar_Init();

	Cmd_Init();

	Fs_Init();

	// general options
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "-verbose")) {
			verbose = true;
			continue;
		}

		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "-debug")) {
			debug = true;
			continue;
		}

		if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "-threads")) {
			Cvar_Set("threads", argv[i + 1]);
			continue;
		}

		if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "-legacy")) {
			legacy = true;
			continue;
		}
	}

	// read compiling options
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help")) {
			PrintHelpMessage();
			return 0;
		}

		if (!strcmp(argv[i], "-bsp")) {
			do_bsp = true;
			alt_argc = argc - i;
			alt_argv = (char **) (argv + i);
			Check_BSP_Options(alt_argc, alt_argv);
		}

		if (!strcmp(argv[i], "-vis")) {
			do_vis = true;
			alt_argc = argc - i;
			alt_argv = (char **) (argv + i);
			Check_VIS_Options(alt_argc, alt_argv);
		}

		if (!strcmp(argv[i], "-light")) {
			do_light = true;
			alt_argc = argc - i;
			alt_argv = (char **) (argv + i);
			Check_LIGHT_Options(alt_argc, alt_argv);
		}

		if (!strcmp(argv[i], "-mat")) {
			do_mat = true;
			alt_argc = argc - i;
			alt_argv = (char **) (argv + i);
			Check_MAT_Options(alt_argc, alt_argv);
		}

		if (!strcmp(argv[i], "-pak")) {
			do_pak = true;
			alt_argc = argc - i;
			alt_argv = (char **) (argv + i);
			Check_PAK_Options(alt_argc, alt_argv);
		}
	}

	if (!do_bsp && !do_vis && !do_light && !do_mat && !do_pak) {
		Com_Error(ERR_FATAL, "No action specified.\n"
			"Please specify at least one of -bsp -vis -light -mat -pak.\n");
	}

	Thread_Init();

	// ugly little hack to localize global paths to game paths
	// for e.g. GtkRadiant
	c = strstr(argv[argc - 1], "/maps/");
	c = c ? c + 1 : argv[argc - 1];

	StripExtension(c, map_name);
	strcpy(bsp_name, map_name);
	strcat(map_name, ".map");
	strcat(bsp_name, ".bsp");

	// start timer
	start = time(NULL);
	srand(0);

	if (do_bsp)
		BSP_Main();
	if (do_vis)
		VIS_Main();
	if (do_light)
		LIGHT_Main();
	if (do_mat)
		MAT_Main();
	if (do_pak)
		PAK_Main();

	Thread_Shutdown();

	Z_Shutdown();

	// emit time
	end = time(NULL);
	total_time = (int) (end - start);
	Com_Print("\nTotal Time: ");
	if (total_time > 59)
		Com_Print("%d Minutes ", total_time / 60);
	Com_Print("%d Seconds\n", total_time % 60);

#ifdef _WIN32
	snprintf(title, sizeof(title), "Q2WMap [Finished]");
	SetConsoleTitle(title);

	Com_Print("\n-----------------------------------------------------------------\n");
	Com_Print("%s has been compiled sucessfully! \n\nPress any key to quit\n", bsp_name);
	Com_Print("-----------------------------------------------------------------");

#ifdef HAVE_CURSES
	do { // don't quit, leave output on screen until a key is pressed
	}while(getch() == ERR);

	endwin(); // shutdown pdcurses
#endif

	CloseWin32Console(); // close the console
#endif

	// exit with error code
	return r;
}
