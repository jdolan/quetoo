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
#define QUETOO_DATA_REVISION_URL "https://quetoo-data.s3.amazonaws.com/default/revision"

#define QUETOO_INSTALLER "quetoo-installer-small.jar"

/**
 * @brief
 */
static int32_t Installer_CompareRevision(const char *rev, const char *rev_url) {
	void *data;
	size_t length;

	int32_t status = Net_HttpGet(rev_url, &data, &length);
	if (status == 200) {
		const char *remote_rev = g_strstrip((gchar *) data);
		Com_Debug(DEBUG_COMMON, "%s == %s\n", rev, remote_rev);
		status = g_strcmp0(rev, remote_rev);
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

	char *data_revision;
	Fs_Load("revision", (void **) &data_revision);

	int32_t res = Installer_CompareRevision(revision->string, QUETOO_REVISION_URL);
	if (res == 0) {
		Com_Debug(DEBUG_COMMON, "Build revision %s is latest, checking data..\n", REVISION);
		res = Installer_CompareRevision(data_revision, QUETOO_DATA_REVISION_URL);
		if (res == 0) {
			Com_Debug(DEBUG_COMMON, "Data revision %s is latest\n", data_revision);
		} else {
			Com_Debug(DEBUG_COMMON, "Data revision %s did not match\n", data_revision);
		}
	} else {
		Com_Debug(DEBUG_COMMON, "Build revision %s is out of date\n", REVISION);
	}

	return res;
}

/**
 * @brief
 */
int32_t Installer_LaunchInstaller(void) {

	Com_Print("Quetoo is out of date, launching installer..\n");

	GError *error;
	if (!g_spawn_async(Fs_LibDir(),
			(gchar *[]) { "java",
				"-jar", QUETOO_INSTALLER,
				"--build", BUILD,
				"--dir", (gchar *) Fs_BaseDir(),
				"--prune", NULL },
			NULL,
			G_SPAWN_SEARCH_PATH |
			G_SPAWN_DO_NOT_REAP_CHILD |
			G_SPAWN_STDOUT_TO_DEV_NULL |
			G_SPAWN_STDERR_TO_DEV_NULL,
			NULL,
			NULL,
			NULL,
			&error)) {

		Com_Error(ERROR_DROP, "Failed: %d: %s\n", error->code, error->message);
	}

	Com_Shutdown("Installer launched successfully.\n");
	return 0;
}
