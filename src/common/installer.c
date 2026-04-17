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

#include <SDL3/SDL_misc.h>

#include "console.h"
#include "installer.h"
#include "filesystem.h"
#include "net/net_http.h"

#define QUETOO_REVISION_URL "https://quetoo.s3.amazonaws.com/revisions/" BUILD
#define QUETOO_RELEASES_URL "https://github.com/jdolan/quetoo/releases/latest"

/**
 * @brief Compares a local revision string against a remote revision URL.
 * @return 0 if they match, non-zero otherwise.
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
 * @brief Checks whether a newer engine build is available.
 * @return 0 if the engine is up to date, 1 if an update is available.
 */
int32_t Installer_CheckForUpdates(void) {

  if (revision->integer == -1) {
    Com_Debug(DEBUG_COMMON, "Skipping revision check\n");
    return 0;
  }

  if (Installer_CompareRevision(revision->string, QUETOO_REVISION_URL) == 0) {
    Com_Debug(DEBUG_COMMON, "Build revision %s is latest.\n", revision->string);
    return 0;
  }

  Com_Debug(DEBUG_COMMON, "Build revision %s is out of date.\n", revision->string);
  return 1;
}

/**
 * @brief Opens the GitHub releases page in the user's web browser.
 */
void Installer_OpenReleasesPage(void) {

  Com_Print("Opening %s\n", QUETOO_RELEASES_URL);

  if (!SDL_OpenURL(QUETOO_RELEASES_URL)) {
    Com_Warn("Failed to open browser: %s\n", SDL_GetError());
  }
}
