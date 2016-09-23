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

#include "../../common.h"
#include "../../sys.h"

#if defined(__APPLE__)
 #include <sys/wait.h>
 #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-apple/x86_64/"
#elif defined(__linux__)
 #include <sys/wait.h>
 #if defined(__x86_64__)
  #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-linux/x86_64/"
 #else
  #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-linux/i686/"
 #endif
#elif defined(__MINGW32__)
 #include <ctype.h>
 #if defined(__MINGW64__)
  #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-mingw/x86_64/"
 #else
  #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-mingw/i686/"
 #endif
#elif defined(__CYGWIN__)
 #if defined(__LP64__)
  #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-mingw/x86_64/"
 #else
  #define RSYNC_REPOSITORY "rsync://quetoo.org/quetoo-mingw/i686/"
 #endif
#endif

quetoo_t quetoo;

/**
 * @return The default rsync destination, assuming the game is installed via the official packages.
 */
static char *get_default_dest(void) {
	char *dest = NULL;

	const char *exe = Sys_ExecutablePath();
	if (exe) {
#if defined(__APPLE__)
		char *c = strstr(exe, "Quetoo.app/Contents");
		if (c) {
			const size_t len = (c - exe) + strlen("Quetoo.app/Contents");
			dest = calloc(len + 1, sizeof(char));
			strncpy(dest, exe, len);
		}
#elif defined(__linux__)
		char *c = strstr(exe, "quetoo/bin");
		if (c) {
			const size_t len = (c - exe) + strlen("quetoo");
			dest = calloc(len + 1, sizeof(char));
			strncpy(dest, exe, len);
		}
#elif defined(_WIN32)
		char *c = strstr(exe, "\\bin\\quetoo-update.exe");
		if (c) {
			const size_t len = (c - exe);
			dest = calloc(len + 1, sizeof(char));
			strncpy(dest, exe, len);
		}
#endif
	}

	return dest;
}

#if defined(_WIN32)
/**
 * @return The Cygwin path corresponding to `path`. Ideally, we would use the Cygwin API for this,
 * but cross-compiling for Cygwin is somehow not very popular.
 *
 * @see <sys/cygwin.h>
 * @see cygwin_conv_path
 */
static char *convert_cygwin_path(const char *path) {

	const size_t len = strlen(path) + strlen("/cgydrive/") + 1;
	char *cygpath = calloc(len, sizeof(char));

	if (strstr(path, ":\\")) {
		strcpy(cygpath, "/cygdrive/");
	}

	const char *in = path;
	char *out = cygpath + strlen(cygpath);
	while (*in) {
		if (*in == ':') {
			in++;
		} else if (*in == '\\') {
			*out++ = '/'; in++;
		} else {
			*out++ = tolower(*in++);
		}
	}

	return cygpath;
}
#endif

/**
 * @brief Program entry point.
 */
int main(int argc, char **argv) {
	int status = 1;

	char *dest;

	if (argc == 2) {
		dest = strdup(argv[1]);
	} else {
		dest = get_default_dest();
	}

	if (dest) {
		printf("Syncing "RSYNC_REPOSITORY" to %s..\n", dest);

#if defined(_WIN32)
		char *cygdest = convert_cygwin_path(dest);
		if (cygdest) {
			printf("Using cygwin path %s\n", cygdest);
			status = _spawnlp(_P_WAIT, "rsync.exe", "rsync.exe", "-rkzhP", "--delete", "--stats",
							  "--skip-compress=pk3",
							  "--perms",
							  "--chmod=a=rwx,Da+x",
							  "--exclude=bin/cygwin1.dll",
							  "--exclude=bin/rsync.exe",
							  "--exclude=bin/update.exe",
							  RSYNC_REPOSITORY, cygdest, NULL);

			free(cygdest);
		} else {
			fprintf(stderr, "Failed to convert %s to cygwin path\n", dest);
		}
#else
		const pid_t child = fork();
		if (child == 0) {
			const int err = execlp("rsync", "rsync", "-rLzhP", "--delete", "--stats", RSYNC_REPOSITORY, dest, NULL);
			if (err == -1) {
				perror(NULL);
				exit(err);
			}
		} else {
			wait(&status);
		}
#endif

		if (status == 0) {
			printf("\n\nFinished successfully\n");
		} else {
			fprintf(stderr, "\n\nrsync exited with error status %d\n", status);
		}

		free(dest);
	} else {
		fprintf(stderr, "Failed to resolve rsync destination\n");
		fprintf(stderr, "Usage: %s [destination]\n", argv[0]);
	}

	return status;
}
