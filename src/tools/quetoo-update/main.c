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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <process.h>

	#ifndef execvp
		#define execvp _execvp
	#endif

	#ifndef realpath
		#define realpath(rel, abs) _fullpath(abs, rel, MAX_PATH)
	#endif
#else
	#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__x86_64__) || defined(_WIN64) || defined(__MINGW64__) || defined(__LP64__)
	#define ARCH "x86_64"
#else
	#define ARCH "i686"
#endif

#if defined(__APPLE__)
	#define HOST "apple"
#elif defined(__linux__)
	#define HOST "linux"
#elif defined(_WIN32)
	#if defined(__MINGW32__) || defined(__CYGWIN__)
		#define HOST "mingw"
	#elif defined(_MSC_VER)
		#define HOST "pc-windows"
	#endif
#endif

#if !defined(HOST) || !defined(ARCH)
#error Unknown OS.
#endif

#define DEFAULT_UPDATE_JAR "quetoo-update-small.jar"

/**
 * @return The quetoo-update jar file.
 */
static gchar *get_quetoo_update_jar(const char *argv0, const char *jar) {

	gchar *update_jar = NULL;

	char *path = realpath(argv0, NULL);
	if (path) {

		gchar *dir = g_path_get_dirname(path);
		if (dir) {
#ifdef __APPLE__
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

	const char *jar = g_getenv("QUETOO_UPDATE_JAR") ?: DEFAULT_UPDATE_JAR;

	gchar *quetoo_update_jar = get_quetoo_update_jar(argv[0], jar);
	if (quetoo_update_jar) {

		GPtrArray *args = g_ptr_array_new();
		g_ptr_array_add(args, "java");
		g_ptr_array_add(args, "-jar");
		g_ptr_array_add(args, quetoo_update_jar);
		g_ptr_array_add(args, "--arch");
		g_ptr_array_add(args, ARCH);
		g_ptr_array_add(args, "--host");
		g_ptr_array_add(args, HOST);

		for (int i = 0; i < argc - 1; i++) {
			g_ptr_array_add(args, argv + i);
		}

		g_ptr_array_add(args, NULL);

		const gchar *const *argptr = (const gchar *const *) &g_ptr_array_index(args, 0);

		if (execvp(argptr[0], argptr) != -1) {
			status = 0;
		} else {
			status = errno;
			fprintf(stderr, "Failed spawn process: %d\n", status);
		}

		g_ptr_array_free(args, true);
		g_free(quetoo_update_jar);
	} else {
		fprintf(stderr, "Failed to resolve %s\n", jar);
	}

	return status;
}
