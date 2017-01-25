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

		gchar *args[argc + 3];
		args[0] = "java", args[1] = "-jar", args[2] = quetoo_update_jar;
		memcpy(args[3], argv[1], (argc - 1) * sizeof(char *));
		args[argc + 2] = NULL;

		if (execvp(args[0], (const char *const *) args) != -1) {
			status = 0;
		} else {
			status = errno;
			fprintf(stderr, "Failed spawn process: %d\n", status);
		}

		g_free(quetoo_update_jar);
	} else {
		fprintf(stderr, "Failed to resolve %s\n", jar);
	}

	return status;
}
