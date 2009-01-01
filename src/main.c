/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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
#include <dirent.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <pwd.h>
#endif

#ifndef __LIBSYS_LA__
#ifdef _WIN32

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

#endif
#endif

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"
#include "game/game.h"
#include "keys.h"


/*
 * Sys_Milliseconds
 */
int curtime;

int Sys_Milliseconds(void){
	static int base;
#ifdef _WIN32

	if(!base)
		base = timeGetTime() & 0xffff0000;

	curtime = timeGetTime() - base;
#else
	struct timeval tp;

	gettimeofday(&tp, NULL);

	if(!base)
		base = tp.tv_sec;

	curtime = (tp.tv_sec - base) * 1000 + tp.tv_usec / 1000;
#endif
	return curtime;
}

const char *Sys_GetCurrentUser(void){
	static char s_userName[64];
#ifdef _WIN32
	unsigned long size = sizeof(s_userName);

	if (!GetUserName(s_userName, &size))
		s_userName[0] = '\0';
#else
	struct passwd *p;

	if ((p = getpwuid(getuid())) == NULL)
		s_userName[0] = '\0';
	else {
		strncpy(s_userName, p->pw_name, sizeof(s_userName));
		s_userName[sizeof(s_userName) - 1] = '\0';
	}
#endif
	return s_userName;
}

/*
 * Sys_Mkdir
 * 
 * Create the specified directory path.
 */
void Sys_Mkdir(char *path){
#ifdef _WIN32
	mkdir(path);
#else
	mkdir(path, 0777);
#endif
}


static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static char findpattern[MAX_OSPATH];
static DIR *fdir;

/*
 * Sys_FindFirst
 * 
 * Returns the first full path name matched by the specified search path in
 * the Quake file system.  Wildcards are partially supported.
 */
const char *Sys_FindFirst(const char *path){
	struct dirent *d;
	char *p;

	if(fdir){
		Com_Dprintf("Sys_FindFirst without Sys_FindClose");
		Sys_FindClose();
	}

	strcpy(findbase, path);

	if((p = strrchr(findbase, '/')) != NULL){
		*p = 0;
		strcpy(findpattern, p + 1);
	} else
		strcpy(findpattern, "*");

	if(strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");

	if((fdir = opendir(findbase)) == NULL)
		return NULL;

	while((d = readdir(fdir)) != NULL){
		if(!*findpattern || Com_GlobMatch(findpattern, d->d_name)){
			sprintf(findpath, "%s/%s", findbase, d->d_name);
			return findpath;
		}
	}
	return NULL;
}


/*
 * Sys_FindNext
 */
const char *Sys_FindNext(void){
	struct dirent *d;

	if(fdir == NULL)
		return NULL;

	while((d = readdir(fdir)) != NULL){
		if(!*findpattern || Com_GlobMatch(findpattern, d->d_name)){
			sprintf(findpath, "%s/%s", findbase, d->d_name);
			return findpath;
		}
	}
	return NULL;
}


/*
 * Sys_FindClose
 */
void Sys_FindClose(void){
	if(fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}


/*
 * Sys_CloseLibrary
 * 
 * Closes an open game module.
 */
void Sys_CloseLibrary(void **handle){
	if(*handle)
		dlclose(*handle);
	*handle = NULL;
}


/*
 * Sys_OpenLibrary
 */
void Sys_OpenLibrary(const char *name, void **handle){
	const char *path;

	*handle = NULL;

#ifdef _WIN32
	path = Fs_FindFirst(va("%s.dll", name), true);
#else
	path = Fs_FindFirst(va("%s.so", name), true);
#endif

	if(!path){
		Com_Error(ERR_DROP, "Sys_OpenLibrary: Couldn't find %s", name);
	}

	Com_Printf("Trying %s..\n", path);

	if((*handle = dlopen(path, RTLD_NOW)))
		return;

	Com_Error(ERR_DROP, "Sys_OpenLibrary: %s\n", dlerror());
}


typedef game_export_t *loadgame_t(game_import_t *);
static void *game_handle;

/*
 * Sys_LoadGame
 * 
 * Attempts to open and load the game module.
 */
void *Sys_LoadGame(void *parms){
	loadgame_t *LoadGame;

	if(game_handle){
		Com_Warn("Sys_LoadGame: game already loaded, unloading..\n");
		Sys_UnloadGame();
	}

	Sys_OpenLibrary("game", &game_handle);

	LoadGame = (loadgame_t *)dlsym(game_handle, "LoadGame");

	if(!LoadGame){
		Sys_CloseLibrary(&game_handle);
		return NULL;
	}

	return LoadGame(parms);
}


/*
 * Sys_UnloadGame
 */
void Sys_UnloadGame(void){
	Sys_CloseLibrary(&game_handle);
}


/*
 * Sys_Quit
 * 
 * The final exit point of the program under normal exit conditions.
 */
void Sys_Quit(void){

	Sv_Shutdown(NULL, false);
	Cl_Shutdown();

	Com_Shutdown();

	exit(0);
}


/*
 * The final exit point of the program under abnormal exit conditions.
 */
void Sys_Error(const char *error, ...){
	va_list argptr;
	char string[MAX_STRING_CHARS];

	Sv_Shutdown(error, false);
	Cl_Shutdown();

	Com_Shutdown();

	va_start(argptr, error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	fprintf(stderr, "ERROR: %s\n", string);

	exit(1);
}


#ifndef __LIBSYS_LA__

#include "console.h"
#include "con_curses.h"

/*
 * Sys_Signal
 * 
 * Catch kernel interrupts and dispatch the appropriate exit routine.
 */
static void Sys_Signal(int s){

	switch(s){
		case SIGHUP:
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
			Com_Printf("Received signal %d, quitting..\n", s);
			Sys_Quit();
			break;
#ifndef _WIN32
#ifdef HAVE_CURSES
		case SIGWINCH:
			Curses_Resize();
			break;
#endif
#endif
		default:
			Sys_Error("Received signal %d.\n", s);
	}
}


/*
 * main
 * 
 * The entry point of the program.  This source file is actually also built
 * as libsys.la, so that other programs (e.g. q2wmap, pak) may link into
 * the platform abstracted functionality provided here.  The main function,
 * however, is the entry point of Quake2World execution -- omitted from the
 * libtool library for obvious reasons.
 */
int main(int argc, char **argv){
	int oldtime, msec;

#ifdef _WIN32
	// here the magic happens
	int hCrt;
	FILE *hf;

	AllocConsole();

	hCrt = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE),_O_TEXT);
	hf = _fdopen( hCrt, "w" );
	*stdout = *hf;
	setvbuf( stdout, NULL, _IONBF, 0 );
#endif

	printf("Quake2World %s\n", VERSION);
	Com_Init(argc, argv);

#ifndef _WIN32
#ifdef HAVE_CURSES
	signal(SIGWINCH, Sys_Signal);
#endif
#endif
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

		msec = curtime - oldtime;

		if(msec < 1)
			continue;

		Com_Frame(msec);

#ifdef HAVE_CURSES
		Curses_Frame(msec);
#endif
		oldtime = curtime;
	}
}

#endif /*__LIBSYS_LA__*/

