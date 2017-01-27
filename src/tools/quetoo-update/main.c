/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006-2011 Quetoo.
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <shellapi.h>

/**
 * @brief Windows must use CreateProcess because _spawn/_exec on Windows keeps the calling process 
 * running, which is definitely not what we want.
 */
static intptr_t execvp(const char *filename, const char *const *args) {
	gchar *cmdline = g_strjoinv(" ", (gchar **) (args + 1));
	
	const int result = (int) ShellExecute(NULL, "open", filename, cmdline, NULL, SW_HIDE);

	if (result <= 32) {
		g_free(cmdline);
		return GetLastError();
	}

	g_free(cmdline);
	return 0;
}

int main(int argc, char **argv);

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	return main(__argc, __argv);
}

	#ifndef realpath
		#define realpath(rel, abs) _fullpath(abs, rel, MAX_PATH)
	#endif

#else
	#include <unistd.h>
#endif

#if defined(__APPLE__)
	#define ARCH "x86_64"
	#define HOST "apple_darwin"
#elif defined(__linux__)
	#if defined(__x86_64__)
		#define ARCH "x86_64"
	#else
		#define ARCH "i686"
	#endif
	#define HOST "pc_linux"
#elif defined __MINGW32__
	#if defined(__MINGW64__)
		#define ARCH "x86_64"
	#else
		#define ARCH "i686"
	#endif
	#define HOST "w64_mingw32"
#elif defined(_MSC_VER)
	#if defined(_WIN64)
		#define ARCH "x86_64"
	#else
		#define ARCH "i686"
	#endif
	#define HOST "pc_windows"
#else
	#error Unknown architecture or host
#endif

#define QUETOO_INSTALLER_JAR "quetoo-installer-small.jar"

/**
 * @return The quetoo-installer jar file.
 */
static gchar *get_quetoo_installer_jar(const char *argv0, const char *jar) {

	gchar *update_jar = NULL;

	char *path = realpath(argv0, NULL);
	if (path) {

		gchar *dir = g_path_get_dirname(path);
		if (dir) {
#if defined(__APPLE__)
			update_jar = g_build_path(G_DIR_SEPARATOR_S, dir, "lib", jar, NULL);
#else
			update_jar = g_build_path(G_DIR_SEPARATOR_S, dir, "..", "lib", jar, NULL);
#endif
			g_free(dir);
		}

		free(path);
	}

	return update_jar;
}

/**
 * @brief Program entry point.
 */
int main(int argc, char **argv) {
	int status = 1;

	const char *jar = g_getenv("QUETOO_INSTALLER_JAR") ?: QUETOO_INSTALLER_JAR;

	gchar *quetoo_installer_jar = get_quetoo_installer_jar(argv[0], jar);
	if (quetoo_installer_jar) {

		GPtrArray *args = g_ptr_array_new();
		g_ptr_array_add(args, "java");
		g_ptr_array_add(args, "-jar");
		g_ptr_array_add(args, quetoo_installer_jar);
		g_ptr_array_add(args, "--arch");
		g_ptr_array_add(args, ARCH);
		g_ptr_array_add(args, "--host");
		g_ptr_array_add(args, HOST);

		for (int i = 0; i < argc - 1; i++) {
			g_ptr_array_add(args, argv + i);
		}

		g_ptr_array_add(args, NULL);

		char *const *argptr = (char *const *) &g_ptr_array_index(args, 0);

		if (execvp(argptr[0], argptr) != -1) {
			status = 0;
		} else {
			status = errno;
			fprintf(stderr, "Failed spawn process: %d\n", status);
		}

		g_ptr_array_free(args, true);
		g_free(quetoo_installer_jar);
	} else {
		fprintf(stderr, "Failed to resolve %s\n", jar);
	}

	return status;
}
