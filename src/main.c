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

#include "common.h"

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

#ifdef __APPLE__
	// a hack for Mac to open Cocoa before we try to initialize SDL
	void *cocoa_lib;
	void (*nsappload)(void);

	cocoa_lib = dlopen("/System/Library/Frameworks/Cocoa.framework/Cocoa", RTLD_LAZY);

	nsappload = (void(*))dlsym(cocoa_lib, "NSApplicationLoad");
	nsappload();

	dlclose(cocoa_lib);
#endif

	printf("Quake2World %s\n", VERSION);

	Com_Init(argc, argv);

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

		Sys_Milliseconds();

		if(timescale->modified){
			if(timescale->value < 0.1)
				timescale->value = 0.1;
			else if(timescale->value > 3.0)
				timescale->value = 3.0;
		}

		msec = (curtime - oldtime) * timescale->value;

		if(msec < 1)  // 0ms frames are not okay
			continue;

		Com_Frame(msec);

		oldtime = curtime;
	}
}
