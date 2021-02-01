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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

#include <Objectively/URLSession.h>

#define QUETOO_REVISION_URL "https://quetoo.s3.amazonaws.com/revisions/" BUILD

#define QUETOO_INSTALLER "quetoo-installer-small.jar"

#if defined(_WIN32)
	#include "windows.c"
	#define QUETOO_EXECUTABLE "quetoo.exe"
#else
	#include <unistd.h>
	#define QUETOO_EXECUTABLE "quetoo"
#endif

/**
 * @brief
 */
static Data *get_url(const char *url_string) {
	Data *data = NULL;

	URLSession *session = $$(URLSession, sharedInstance);
	URL *url = $(alloc(URL), initWithCharacters, url_string);

	URLSessionDataTask *task = $(session, dataTaskWithURL, url, NULL);
	$((URLSessionTask *) task, execute);

	const int32_t statusCode = task->urlSessionTask.response->httpStatusCode;
	if (statusCode == 200) {
		data = retain(task->data);
	} else {
		fprintf(stderr, "Failed to get %s: HTTP %d\n", url_string, statusCode);
	}

	release(task);
	release(url);

	return data;
}

/**
 * @brief Program entry point.
 */
int main(int argc, char **argv) {
	char path[PATH_MAX];

	printf("Quetoo Launcher %s %s %s\n", VERSION, BUILD, REVISION);

	int status = 1;

	realpath(argv[0], path);
	if (strlen(path)) {

		gchar *bin_dir = g_path_get_dirname(path);
		assert(bin_dir);

		GPtrArray *args = g_ptr_array_new();
		char *executable = NULL;

		Data *data = get_url(QUETOO_REVISION_URL);
		if (data) {

			char *revision = strtrim((char *) data->bytes);
			if (g_strcmp0(revision, REVISION)) {

				printf("Revision %s != %s, executing installer..", REVISION, revision);

#if defined(__APPLE__)
				executable = g_build_path(G_DIR_SEPARATOR_S, bin_dir, "lib", QUETOO_INSTALLER, NULL);
#else
				executable = g_build_path(G_DIR_SEPARATOR_S, bin_dir, "..", "lib", QUETOO_INSTALLER, NULL);
#endif

				g_ptr_array_add(args, "java");
				g_ptr_array_add(args, "-jar");
				g_ptr_array_add(args, executable);
				g_ptr_array_add(args, "--build");
				g_ptr_array_add(args, BUILD);
				g_ptr_array_add(args, "--prune");
			} else {
				printf("Revision %s is current", revision);
			}

			release(data);
		} else {
			fprintf(stderr, "Failed to fetch %s\n", QUETOO_REVISION_URL);
		}

		if (args->len == 0) {

			executable = g_build_path(G_DIR_SEPARATOR_S, bin_dir, QUETOO_EXECUTABLE, NULL);
			g_ptr_array_add(args, executable);

			for (int i = 1; i < argc; i++) {
				g_ptr_array_add(args, argv[i]);
			}
		}

		g_ptr_array_add(args, NULL);

		char *const *argptr = (char *const *) &g_ptr_array_index(args, 0);

		if (execvp(argptr[0], argptr) != -1) {
			status = 0;
		} else {
			status = errno;
			fprintf(stderr, "Failed to execute %s: %d\n", (char *) g_ptr_array_index(args, 0), status);
		}

		g_ptr_array_free(args, true);
		g_free(executable);

	} else {
		fprintf(stderr, "Failed to resolve realpath of %s\n", argv[0]);
	}

	return status;
}
