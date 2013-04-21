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
#include "filesystem.h"

#include <signal.h>
#include <sys/time.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <pwd.h>
#else
#include <windows.h>
#define CSIDL_APPDATA  0x001a
#define CSIDL_PERSONAL 0x0005
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#define MAX_BACKTRACE_SYMBOLS 50
#endif

/*
 * @return Milliseconds since Quake execution began.
 */
uint32_t Sys_Milliseconds(void) {
	static uint32_t base, time;

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
 * @return The current executable path (argv[0]).
 */
const char *Sys_ExecutablePath(void) {
	static char path[MAX_OSPATH];

#ifdef __APPLE__
	uint32_t i = sizeof(path);

	if (_NSGetExecutablePath(path, &i) > -1) {
		return path;
	}

#elif __LINUX__

	if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path)) > -1) {
		return path;
	}

#endif

	Com_Warn("Sys_ExecutablePath: Failed to resolve executable path\n");
	return NULL;
}

/*
 * @return The current user's name.
 */
const char *Sys_Username(void) {
	static char user[64];
#ifdef _WIN32
	size_t size = sizeof(user);

	if (!GetUserName(user, &size))
	user[0] = '\0';
#else
	struct passwd *p;

	if ((p = getpwuid(getuid())) == NULL)
		user[0] = '\0';
	else {
		g_strlcpy(user, p->pw_name, sizeof(user));
	}
#endif
	return user;
}

/*
 * @brief Returns the current user's home directory.
 */
const char *Sys_UserDir(void) {
	static char user_dir[MAX_OSPATH];
#ifdef _WIN32
	void *handle;
	FARPROC GetFolderPath;

	memset(user_dir, 0, sizeof(user_dir));

	if ((handle = dlopen("shfolder.dll", 0))) {
		if ((GetFolderPath = dlsym(handle, "SHGetFolderPathA"))) {
			GetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, user_dir);
		}
		dlclose(handle);
	}

	if (*user_dir != '\0') // append our directory name
	strcat(user_dir, "/My Games/Quake2World");
	else // or simply use ./
	strcat(user_dir, PKGDATADIR);
#else
	g_snprintf(user_dir, sizeof(user_dir), "%s/.quake2world", getenv("HOME"));
#endif
	return user_dir;
}

/*
 * @brief Closes an open game module.
 */
void Sys_CloseLibrary(void **handle) {
	if (*handle)
		dlclose(*handle);
	*handle = NULL;
}

/*
 * @brief
 */
void Sys_OpenLibrary(const char *name, void **handle) {
	*handle = NULL;

#ifdef _WIN32
	char *so_name = va("%s.dll", name);
#else
	char *so_name = va("%s.so", name);
#endif

	if (Fs_Exists(so_name)) {
		char path[MAX_QPATH];

		g_snprintf(path, sizeof(path), "%s/%s", Fs_RealDir(so_name), so_name);
		Com_Print("Trying %s...\n", path);

		if ((*handle = dlopen(path, RTLD_NOW)))
			return;

		Com_Error(ERR_DROP, "Sys_OpenLibrary: %s\n", dlerror());
	}

	Com_Error(ERR_DROP, "Sys_OpenLibrary: Couldn't find %s\n", so_name);
}

/*
 * @brief Opens and loads the specified shared library. The function identified by
 * entry_point is resolved and invoked with the specified parameters, its
 * return value returned by this function.
 */
void *Sys_LoadLibrary(const char *name, void **handle, const char *entry_point, void *params) {
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
		Com_Error(ERR_DROP, "Sys_LoadLibrary: %s: Failed to resolve %s\n", name, entry_point);
	}

	return EntryPoint(params);
}

/*
 * @brief The final exit point of the program under normal exit conditions.
 */
void Sys_Quit(void) {
	exit(0);
}

/*
 * @brief On platforms supporting it, print32_t a backtrace.
 */
void Sys_Backtrace(void) {
#ifdef HAVE_EXECINFO
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	int32_t i;

	i = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);
	backtrace_symbols_fd(symbols, i, STDERR_FILENO);

	fflush(stderr);
#endif
}

/*
 * @brief The final exit point of the program under abnormal exit conditions.
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
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
void Sys_Signal(int32_t s) {

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
