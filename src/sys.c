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

#include "sys.h"

/*
 * Sys_Milliseconds
 */
int Sys_Milliseconds(void) {
	static int base, time;

#ifdef _WIN32
	if(!base)
	base = timeGetTime() & 0xffff0000;

	time = timeGetTime() - base;
#else
	struct timeval tp;

	gettimeofday(&tp, NULL);

	if (!base)
		base = tp.tv_sec;

	time = (tp.tv_sec - base) * 1000 + tp.tv_usec / 1000;
#endif

	return time;
}

/*
 * Sys_GetCurrentUser
 */
const char *Sys_GetCurrentUser(void) {
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
void Sys_Mkdir(const char *path) {
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
const char *Sys_FindFirst(const char *path) {
	struct dirent *d;
	char *p;

	if (fdir) {
		Com_Debug("Sys_FindFirst without Sys_FindClose");
		Sys_FindClose();
	}

	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		strcpy(findpattern, p + 1);
	} else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");

	if ((fdir = opendir(findbase)) == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || GlobMatch(findpattern, d->d_name)) {
			sprintf(findpath, "%s/%s", findbase, d->d_name);
			return findpath;
		}
	}
	return NULL;
}

/*
 * Sys_FindNext
 */
const char *Sys_FindNext(void) {
	struct dirent *d;

	if (fdir == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || GlobMatch(findpattern, d->d_name)) {
			sprintf(findpath, "%s/%s", findbase, d->d_name);
			return findpath;
		}
	}
	return NULL;
}

/*
 * Sys_FindClose
 */
void Sys_FindClose(void) {
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}

/*
 * Sys_CloseLibrary
 *
 * Closes an open game module.
 */
void Sys_CloseLibrary(void **handle) {
	if (*handle)
		dlclose(*handle);
	*handle = NULL;
}

/*
 * Sys_OpenLibrary
 */
void Sys_OpenLibrary(const char *name, void **handle) {
	const char *path;

	*handle = NULL;

#ifdef _WIN32
	path = Fs_FindFirst(va("%s.dll", name), true);
#else
	path = Fs_FindFirst(va("%s.so", name), true);
#endif

	if (!path) {
		Com_Error(ERR_DROP, "Sys_OpenLibrary: Couldn't find %s\n", name);
	}

	Com_Print("Trying %s...\n", path);

	if ((*handle = dlopen(path, RTLD_NOW)))
		return;

	Com_Error(ERR_DROP, "Sys_OpenLibrary: %s\n", dlerror());
}

/*
 * Sys_LoadLibrary
 *
 * Opens and loads the specified shared library. The function identified by
 * entry_point is resolved and invoked with the specified parameters, its
 * return value returned by this function.
 */
void *Sys_LoadLibrary(const char *name, void **handle, const char *entry_point,
		void *params) {
	typedef void *entry_point_t(void *);
	entry_point_t *EntryPoint;

	if (*handle) {
		Com_Warn("Sys_LoadLibrary: %s: handle already open\n", name);
		Sys_CloseLibrary(handle);
	}

	Sys_OpenLibrary(name, handle);

	EntryPoint = (entry_point_t *) dlsym(*handle, entry_point);

	if (!EntryPoint) {
		Sys_CloseLibrary(handle);
		Com_Error(ERR_DROP, "Sys_LoadLibrary: %s: Failed to resolve %s\n",
				name, entry_point);
	}

	return EntryPoint(params);
}

/*
 * Sys_Quit
 *
 * The final exit point of the program under normal exit conditions.
 */
void Sys_Quit(void) {
	exit(0);
}

/*
 * Sys_Backtrace
 *
 * On platforms supporting it, print a backtrace.
 */
void Sys_Backtrace(void) {
#ifdef HAVE_EXECINFO
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	int i;

	i = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);
	backtrace_symbols_fd(symbols, i, STDERR_FILENO);

	fflush(stderr);
#endif
}

/*
 * The final exit point of the program under abnormal exit conditions.
 */
void Sys_Error(const char *error, ...) {
	va_list args;
	char string[MAX_STRING_CHARS];

	Sys_Backtrace();

	va_start(args, error);
	vsnprintf(string, sizeof(string), error, args);
	va_end(args);

	fprintf(stderr, "ERROR: %s\n", string);

	exit(1);
}

/*
 * Sys_Signal
 *
 * Catch kernel interrupts and dispatch the appropriate exit routine.
 */
void Sys_Signal(int s) {

	switch (s) {
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		Com_Print("Received signal %d, quitting...\n", s);
		Sys_Quit();
		break;
	default:
		Sys_Backtrace();
		Sys_Error("Received signal %d.\n", s);
		break;
	}
}
