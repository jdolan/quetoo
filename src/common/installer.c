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

#include "console.h"
#include "installer.h"
#include "filesystem.h"
#include "net/net_http.h"

#define QUETOO_REVISION_URL "https://quetoo.s3.amazonaws.com/revisions/" BUILD
#define QUETOO_DATA_REVISION_URL "https://quetoo-data.s3.amazonaws.com/revision"

#define QUETOO_INSTALLER "quetoo-installer-small.jar"

/**
 * @brief
 */
static int32_t Installer_CompareRevision(const char *rev, const char *rev_url) {
	void *data;
	size_t length;

	int32_t status = Net_HttpGet(rev_url, &data, &length);
	if (status == 200) {
		char *remote_rev = g_strchomp(g_strndup(data, length));
		Com_Debug(DEBUG_COMMON, "%s == %s\n", rev, remote_rev);
		status = g_strcmp0(rev, remote_rev);
		g_free(remote_rev);
	} else {
		Com_Warn("%s: HTTP %d\n", rev_url, status);
		if (length) {
			Com_Debug(DEBUG_COMMON, "%s\n", (gchar *) data);
		}
	}

	Mem_Free(data);

	return status;
}

/**
 * @brief
 */
int32_t Installer_CheckForUpdates(void) {

	if (revision->integer == -1) {
		Com_Debug(DEBUG_COMMON, "Skipping revisions check\n");
		return 0;
	}

	if (Installer_CompareRevision(revision->string, QUETOO_REVISION_URL) == 0) {
		Com_Debug(DEBUG_COMMON, "Build revision %s is latest.\n", revision->string);

		void *buffer;
		if (Fs_Load("revision", &buffer) > 0) {

			char *data_revision = g_strstrip((gchar *) buffer);
			if (Installer_CompareRevision(data_revision, QUETOO_DATA_REVISION_URL) == 0) {
				Com_Debug(DEBUG_COMMON, "Data revision %s is latest.\n", data_revision);
				return 0;
			} else {
				Com_Debug(DEBUG_COMMON, "Data revision %s did not match.\n", data_revision);
			}

			Fs_Free(buffer);
		} else {
			Com_Debug(DEBUG_COMMON, "Data revision not found.\n");
		}
	} else {
		Com_Debug(DEBUG_COMMON, "Build revision %s is out of date.\n", revision->string);
	}

	return -1;
}

/**
 * @brief
 */
int32_t Installer_LaunchInstaller(void) {

	Com_Print("Quetoo is out of date, launching installer..\n");
	
	gchar *path = g_build_path(G_DIR_SEPARATOR_S, Fs_LibDir(), QUETOO_INSTALLER, NULL);
	GError *error = NULL;

	if (!g_spawn_async(NULL,
			(gchar *[]) { "java",
				"-jar", path,
				"--build", BUILD,
				"--dir", (gchar *) Fs_BaseDir(),
				"--prune", NULL },
			NULL,
			G_SPAWN_SEARCH_PATH |
			G_SPAWN_DO_NOT_REAP_CHILD |
			G_SPAWN_CHILD_INHERITS_STDIN |
			G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			NULL,
			NULL,
			NULL,
			&error)) {

		g_free(path);
		Com_Error(ERROR_DROP, "Failed: %d: %s\n", error->code, error->message);
	}
	
	g_free(path);
	Com_Shutdown("Installer launched successfully.\n");
}
