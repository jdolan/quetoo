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

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include <setjmp.h>
#include <signal.h>

#include "config.h"
#ifdef BUILD_CLIENT
#include "client/client.h"
#endif
#include "server/server.h"

jmp_buf environment;

quake2world_t quake2world;

cvar_t *debug;
cvar_t *dedicated;
cvar_t *show_trace;
cvar_t *time_demo;
cvar_t *time_scale;
cvar_t *verbose;

static void Debug(const char *msg);
static void Error(err_t err, const char *msg) __attribute__((noreturn));
static void Print(const char *msg);
static void Shutdown(const char *msg);
static void Verbose(const char *msg);
static void Warn(const char *msg);


/*
 * Debug
 *
 * Filters debugging output to when the `debug` cvar is set.
 */
static void Debug(const char *msg){

	if(!debug->value)
		return;

	Print(msg);
}


/*
 * Error
 *
 * Callback for subsystem failures.  Depending on the code, we might simply
 * print a message, or shut the entire engine down and exit.
 */
static void Error(err_t err, const char *msg){

	Print(va("^1%s", msg));

	switch(err){
		case ERR_NONE:
		case ERR_DROP:

			Sv_Shutdown(msg);

#ifdef BUILD_CLIENT
			Cl_Disconnect();
			cls.key_state.dest = key_console;
#endif

			longjmp(environment, -1);
			break;

		case ERR_FATAL:
		default:

			Shutdown((const char *)msg);

			Sys_Error("%s", msg);
			break;
	}
}


/*
 * Print
 *
 * Delegates all printing to the console.
 */
static void Print(const char *msg){

	if(quake2world.time)
		Con_Print(msg);
	else
		printf("%s", msg);
}


/*
 * Verbose
 *
 * Filters verbose output to when the `verbose` cvar is set.
 */
static void Verbose(const char *msg){

	if(!verbose->value)
		return;

	Print(msg);
}


/*
 * Warn
 *
 * Prints the specified message with a colored accent.
 */
static void Warn(const char *msg){
	Print(va("^3%s", msg));
}


/*
 * Quit_f
 */
static void Quit_f(void){

	Shutdown("Server quit.\n");

	Sys_Quit();
}


/*
 * Init
 */
static void Init(int argc, char **argv){
	char *s;

	memset(&quake2world, 0, sizeof(quake2world));

	if(setjmp(environment))
		Sys_Error("Error during initialization.");

	quake2world.Debug = Debug;
	quake2world.Error = Error;
	quake2world.Print = Print;
	quake2world.Verbose = Verbose;
	quake2world.Warn = Warn;

	Z_Init();

	Com_InitArgv(argc, argv);

	Swap_Init();

	Cbuf_Init();

	Cmd_Init();

	Cvar_Init();

	/*
	 * We need to add the early commands twice, because a base directory needs
	 * to be set before executing configuration files, and we also want
	 * command line parameters to override configuration files.
	 */
	Cbuf_AddEarlyCommands(false);

	Fs_Init();

	Cbuf_AddEarlyCommands(true);

#ifndef BUILD_CLIENT
	dedicated = Cvar_Get("dedicated", "1", CVAR_NOSET, NULL);
#else
	dedicated = Cvar_Get("dedicated", "0", CVAR_NOSET, NULL);
#endif

	debug = Cvar_Get("debug", "0", 0, "Print debugging information");
	show_trace = Cvar_Get("showtrace", "0", 0, "Print trace counts per frame");
	time_demo = Cvar_Get("time_demo", "0", 0, "Benchmark and stress test");
	time_scale = Cvar_Get("time_scale", "1.0", 0, "Controls time lapse");
	verbose = Cvar_Get("verbose", "0", 0, "Print verbose information");

	Con_Init();
	quake2world.time = Sys_Milliseconds();

	s = va("Quake2World %s %s %s", VERSION, __DATE__, BUILDHOST);
	Cvar_Get("version", s, CVAR_SERVER_INFO | CVAR_NOSET, NULL);

	Cmd_AddCommand("quit", Quit_f, "Quit Quake2World");

	Net_Init();
	Netchan_Init();

	Sv_Init();

#ifdef BUILD_CLIENT
	Cl_Init();
#endif

	Com_Print("Quake2World initialized.\n");

	// add + commands from command line
	Cbuf_AddLateCommands();

	// dedicated server, nothing specified, use fractures.bsp
	if(dedicated->value && !Com_ServerState()){
		Cbuf_AddText("map torn\n");
		Cbuf_Execute();
	}
}


/*
 * Shutdown
 *
 * Cleans up all game engine subsystems.
 */
static void Shutdown(const char *msg){

	Sv_Shutdown(msg);

#ifdef BUILD_CLIENT
	Cl_Shutdown();
#endif

	Con_Shutdown();

	Z_Shutdown();
}


/*
 * Frame
 */
static void Frame(int msec){
	extern int c_traces, c_brush_traces;
	extern int c_pointcontents;

	if(setjmp(environment))
		return;  // an ERR_DROP or ERR_NONE was thrown

	if(show_trace->value){
		Com_Print("%4i traces (%4i clips), %4i points\n", c_traces,
				c_brush_traces, c_pointcontents);
		c_traces = c_brush_traces = c_pointcontents = 0;
	}

	Cbuf_Execute();

	Sv_Frame(msec);

#ifdef BUILD_CLIENT
	Cl_Frame(msec);
#endif
}


/*
 * main
 *
 * The entry point of the program.
 */
int main(int argc, char **argv){
	int oldtime, msec;

#ifdef _WIN32
	// here the magic happens
	int hCrt;
	FILE *hf;

	AllocConsole();

	hCrt = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
	hf = _fdopen(hCrt, "w");
	*stdout = *hf;
	setvbuf(stdout, NULL, _IONBF, 0);
#endif

	printf("Quake2World %s\n", VERSION);

	Init(argc, argv);

	signal(SIGHUP, Sys_Signal);
	signal(SIGINT, Sys_Signal);
	signal(SIGQUIT, Sys_Signal);
	signal(SIGILL, Sys_Signal);
	signal(SIGABRT, Sys_Signal);
	signal(SIGFPE, Sys_Signal);
	signal(SIGSEGV, Sys_Signal);
	signal(SIGTERM, Sys_Signal);

	oldtime = Sys_Milliseconds();

	while(true){  // this is our main loop

		quake2world.time = Sys_Milliseconds();

		if(time_scale->modified){
			if(time_scale->value < 0.1)
				time_scale->value = 0.1;
			else if(time_scale->value > 3.0)
				time_scale->value = 3.0;
		}

		msec = (quake2world.time - oldtime) * time_scale->value;

		if(msec < 1)  // 0ms frames are not okay
			continue;

		Frame(msec);

		oldtime = quake2world.time;
	}

	return 0;
}
