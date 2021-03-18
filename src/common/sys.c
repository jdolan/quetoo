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


#include "sys.h"
#include "filesystem.h"

#include <signal.h>

#if defined(_WIN32)
	#include <windows.h>
	#include <shlobj.h>
#if !defined(__MINGW32__)
		#include <DbgHelp.h>
#endif
#endif

#if defined(__APPLE__)
	#include <mach-o/dyld.h>
#endif

#if HAVE_DLFCN_H
	#include <dlfcn.h>
#endif

#if HAVE_EXECINFO
	#include <execinfo.h>
	#define MAX_BACKTRACE_SYMBOLS 50
#endif

#if HAVE_SYS_TIME_H
	#include <sys/time.h>
#endif

#include <SDL.h>

/**
 * @return The current executable path (argv[0]).
 */
const char *Sys_ExecutablePath(void) {
	static char path[MAX_OS_PATH];

#if defined(__APPLE__)
	uint32_t i = sizeof(path);

	if (_NSGetExecutablePath(path, &i) > -1) {
		return path;
	}

#elif defined(__linux__)

	if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path)) > -1) {
		return path;
	}

#elif defined(_WIN32)

	if (GetModuleFileName(0, path, sizeof(path))) {
		return path;
	}

#endif

	Com_Warn("Failed to resolve executable path\n");
	return NULL;
}

/**
 * @return The current user's name.
 */
const char *Sys_Username(void) {
	return g_get_user_name();
}

/**
 * @brief Returns the current user's Quetoo directory.
 *
 * @remarks On Windows, this is `\My Documents\My Games\Quetoo`. On POSIX
 * platforms, it's `~/.quetoo`.
 */
const char *Sys_UserDir(void) {
	static char user_dir[MAX_OS_PATH];

#if defined(_WIN32)
	const char *my_documents = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
	g_snprintf(user_dir, sizeof(user_dir), "%s\\My Games\\Quetoo", my_documents);
#else
	const char *home = g_get_home_dir();
	g_snprintf(user_dir, sizeof(user_dir), "%s/.quetoo", home);
#endif

	return user_dir;
}

/**
 * @brief
 */
void *Sys_OpenLibrary(const char *name, _Bool global) {

#if defined(_WIN32)
	const char *so_name = va("%s.dll", name);
#else
	const char *so_name = va("%s.so", name);
#endif

	if (Fs_Exists(so_name)) {
		char path[MAX_OS_PATH];

		g_snprintf(path, sizeof(path), "%s%c%s", Fs_RealDir(so_name), G_DIR_SEPARATOR, so_name);
		Com_Print("  Loading %s...\n", path);

		void *handle = dlopen(path, RTLD_LAZY | (global ? RTLD_GLOBAL : RTLD_LOCAL));
		if (handle) {
			return handle;
		}

		Com_Error(ERROR_DROP, "%s\n", dlerror());
	}

	Com_Error(ERROR_DROP, "Couldn't find %s\n", so_name);
}

/**
 * @brief Closes an open game module.
 */
void Sys_CloseLibrary(void *handle) {
	dlclose(handle);
}

/**
 * @brief Opens and loads the specified shared library. The function identified by
 * entry_point is resolved and invoked with the specified parameters, its
 * return value returned by this function.
 */
void *Sys_LoadLibrary(void *handle, const char *entry_point, void *params) {
	typedef void *EntryPointFunc(void *);
	EntryPointFunc *EntryPoint;

	assert(handle);
	assert(entry_point);

	EntryPoint = (EntryPointFunc *) dlsym(handle, entry_point);
	if (!EntryPoint) {
		Com_Error(ERROR_DROP, "Failed to resolve entry point: %s\n", entry_point);
	}

	return EntryPoint(params);
}

/**
 * @brief On platforms supporting it, capture a backtrace. Returns
 * allocated memory; use `g_string_free` on it.
 * @param start How many frames to skip
 * @param count How many frames total to include
 */
GString *Sys_Backtrace(uint32_t start, uint32_t max_count)
{
	GString *backtrace_str = g_string_new(NULL);

#if HAVE_EXECINFO
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	const int32_t symbol_count = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);

	char **strings = backtrace_symbols(symbols, symbol_count);

	for (uint32_t i = start, s = 0; s < max_count && i < (uint32_t) symbol_count; i++, s++) {

		g_string_append(backtrace_str, strings[i]);
		g_string_append(backtrace_str, "\n");
	}

	free(strings);
#elif defined(_WIN32) && !defined(__MINGW32__)
	static bool symbols_initialized = false;
	void *symbols[32];
	const int name_length = 256;
	
    HANDLE process = GetCurrentProcess();

	if (!symbols_initialized)
	{
		SymSetOptions(SYMOPT_LOAD_LINES);
		SymInitialize(process, NULL, TRUE);
		symbols_initialized = true;
	}

	const int16_t symbol_count = RtlCaptureStackBackTrace(1, lengthof(symbols), symbols, NULL);

	PSYMBOL_INFO symbol = calloc(sizeof(*symbol) + name_length, 1);
	symbol->MaxNameLen = name_length - 1;
	symbol->SizeOfStruct = sizeof(*symbol);
	
	IMAGEHLP_LINE line;
	line.SizeOfStruct = sizeof(line);
	DWORD dwDisplacement;
	
	for (uint32_t i = start, s = 0; s < max_count && i < (uint32_t) symbol_count; i++, s++) {
		BOOL result = SymFromAddr(process, (DWORD64) symbols[i], 0, symbol);

		if (!result) {
			g_string_append(backtrace_str, "> ???\n");
			continue;
		}

		// we don't care about UCRT/Windows SDK stuff
		if (!g_strcmp0(symbol->Name, "invoke_main") ||
			!strncmp(symbol->Name, "__scrt_", 7)) {
			break;
		}

		// check for line number support
#ifdef _WIN64
		if (SymGetLineFromAddr(process, (DWORD64) symbols[i], &dwDisplacement, &line)) {
#else
		if (SymGetLineFromAddr(process, (DWORD) symbols[i], &dwDisplacement, &line)) {
#endif
			char *last_slash = strrchr(line.FileName, '\\');

			if (!last_slash)
				last_slash = strrchr(line.FileName, '/');

			if (!last_slash)
				last_slash = line.FileName;
			else
				last_slash++;

			g_string_append_printf(backtrace_str, "> %s (%s:%i)\n", symbol->Name, last_slash, line.LineNumber);
		}
		else
			g_string_append_printf(backtrace_str, "> %s (unknown:unknown)\n", symbol->Name);
	}

	SymCleanup(process);
	free(symbol);
#else
	g_string_append(backtrace_str, "Backtrace not supported.\n");
#endif

	// cut off the last \n
	if (backtrace_str->len) {
		g_string_truncate(backtrace_str, backtrace_str->len - 1);
	}

	return backtrace_str;
}

/**
 * @brief Raise a fatal error dialog/exception.
 */
void Sys_Raise(const char *msg) {

	GString *backtrace = Sys_Backtrace(0, (uint32_t) -1);
	
	g_string_prepend(backtrace, "\n");
	g_string_prepend(backtrace, msg);

	if (!Com_WasInit(QUETOO_CLIENT)) {
		fprintf(stderr, "%s", backtrace->str);
		g_string_free(backtrace, true);
		return;
	}

	const SDL_MessageBoxData data = {
		.flags = SDL_MESSAGEBOX_ERROR,
		.title = "Fatal Error",
		.message = backtrace->str,
		.numbuttons = 1,
		.buttons = &(const SDL_MessageBoxButtonData) {
			.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
			.buttonid = 1,
			.text = "OK"
		}
	};

	int32_t button;
	SDL_ShowMessageBox(&data, &button);

	g_string_free(backtrace, true);

#if defined(_MSC_VER)

	RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);

#endif
}

/**
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
void Sys_Signal(int32_t s) {

	switch (s) {
		case SIGINT:
		case SIGTERM:
#ifndef _WIN32
		case SIGHUP:
		case SIGQUIT:
#endif
			Com_Shutdown("Received signal %d, quitting...\n", s);
		default:
			Com_Error(ERROR_FATAL, "Received signal %d\n", s);
	}
}
