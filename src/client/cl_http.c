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

#include <unistd.h>
#include <curl/curl.h>

#include "cl_local.h"

static CURLM *curlm;
static CURL *curl;

// generic encapsulation for common http response codes
typedef struct response_s {
	int code;
	char *text;
} response_t;

static response_t responses[] = {
	{ 400, "Bad request" },
	{ 401, "Unauthorized" },
	{ 403, "Forbidden" },
	{ 404, "Not found" },
	{ 500, "Internal server error" },
	{ 0, NULL }
};

static char curlerr[MAX_STRING_CHARS]; // curl's error buffer

static char url[MAX_OSPATH]; // remote url to fetch from
static char file[MAX_OSPATH]; // local path to save to

static long status, length; // for current transfer
static boolean_t gzip, success;

/*
 * Cl_HttpDownloadRecv
 */
static size_t Cl_HttpDownloadRecv(void *buffer, size_t size, size_t nmemb,
		void *p) {
	return Fs_Write(buffer, size, nmemb, cls.download.file);
}

/*
 * Cl_HttpDownload
 *
 * Queue up an http download.  The url is resolved from cls.download_url and
 * the current game.  We use cURL's multi interface, even tho we only ever
 * perform one download at a time, because it is non-blocking.
 */
boolean_t Cl_HttpDownload(void) {

	if (!curlm)
		return false;

	if (!curl)
		return false;

	memset(file, 0, sizeof(file)); // resolve local file name
	if (gzip)
		snprintf(file, sizeof(file), "%s/%s.gz", Fs_Gamedir(), cls.download.name);
	else
		snprintf(file, sizeof(file), "%s/%s", Fs_Gamedir(), cls.download.name);

	Fs_CreatePath(file); // create the directory

	if (!(cls.download.file = fopen(file, "wb"))) {
		Com_Warn("Failed to open %s.\n", file);
		return false; // and open the file
	}

	cls.download.http = true;

	memset(url, 0, sizeof(url)); // construct url

	if (gzip)
		snprintf(url, sizeof(url), "%s/%s/%s.gz", cls.download_url, Fs_Gamedir(), cls.download.name);
	else
		snprintf(url, sizeof(url), "%s/%s/%s", cls.download_url, Fs_Gamedir(), cls.download.name);

	// set handle to default state
	curl_easy_reset(curl);

	// set url from which to retrieve the file
	curl_easy_setopt(curl, CURLOPT_URL, url);

	// set error buffer so we may print meaningful messages
	memset(curlerr, 0, sizeof(curlerr));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);

	// set the callback for when data is received
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Cl_HttpDownloadRecv);

	curl_multi_add_handle(curlm, curl);
	return true;
}

/*
 * Cl_HttpResponseCode
 */
static char *Cl_HttpResponseCode(long code) {
	int i = 0;
	while (responses[i].code) {
		if (responses[i].code == code)
			return responses[i].text;
		i++;
	}
	return "Unknown";
}

/*
 * Cl_HttpDownloadCleanup
 *
 * If a download is currently taking place, clean it up.  This is called
 * both to finalize completed downloads as well as abort incomplete ones.
 */
void Cl_HttpDownloadCleanup() {
	char *c;
	char f[MAX_OSPATH];

	if (!cls.download.file || !cls.download.http)
		return;

	curl_multi_remove_handle(curlm, curl); // cleanup curl

	Fs_CloseFile(cls.download.file); // always close the file
	cls.download.file = NULL;

	if (success) {
		cls.download.name[0] = 0;

		strncpy(f, file, sizeof(f));

		if ((c = strstr(f, ".gz"))) { // deflate compressed files
			Fs_GunzipFile(f);
			*c = 0;
		}

		if (strstr(f, ".pak")) // add new paks to search paths
			Fs_AddPakfile(f);

		gzip = true;

		// we were disconnected while downloading, so reconnect
		Cl_Reconnect_f();
	} else {

		c = strlen(curlerr) ? curlerr : Cl_HttpResponseCode(status);

		if (gzip) { // retry uncompressed file

			Com_Print("Failed to download %s via HTTP (compressed): %s.\n"
				"Trying uncompressed...\n", cls.download.name, c);

			gzip = false;
		} else { // or via legacy udp download

			Com_Print("Failed to download %s via HTTP: %s.\n"
				"Trying UDP...\n", cls.download.name, c);

			Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
			Msg_WriteString(&cls.netchan.message,
					va("download %s", cls.download.name));

			gzip = true;
		}

		unlink(file); // delete partial or empty file
	}

	cls.download.http = false;

	status = length = 0;
	success = false;

	if (!gzip) // a gzip download failed, retry normal file
		Cl_HttpDownload();
}

/*
 * Cl_HttpDownloadThink
 *
 * Process the pending download by giving cURL some time to think.
 * Poll it for feedback on the transfer to determine what action to take.
 * If a transfer fails, stuff a stringcmd to download it via UDP.  Since
 * we leave cls.download.tempname in tact during our cleanup, when the
 * download is parsed back in the client, it will fopen and begin.
 */
void Cl_HttpDownloadThink(void) {
	CURLMsg *msg;
	int i;

	if (!cls.download_url[0] || !cls.download.file)
		return; // nothing to do

	// process the download as long as data is avaialble
	while (curl_multi_perform(curlm, &i) == CURLM_CALL_MULTI_PERFORM) {
	}

	// fail fast on any curl error
	if (*curlerr != '\0') {
		Cl_HttpDownloadCleanup();
		return;
	}

	// check for http status code
	if (!status) {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

		if (status == 200) {
			// disconnect while we download, this could take some time
			Cl_SendDisconnect();
		} else if (status > 0) { // 404, 403, etc..
			Cl_HttpDownloadCleanup();
			return;
		}
	}

	// check for completion
	while ((msg = curl_multi_info_read(curlm, &i))) {
		if (msg->msg == CURLMSG_DONE) {
			success = true;
			Cl_HttpDownloadCleanup();
			Cl_RequestNextDownload();
			return;
		}
	}
}

/*
 * Cl_InitHttpDownload
 */
void Cl_InitHttpDownload(void) {

	if (!(curlm = curl_multi_init()))
		return;

	if (!(curl = curl_easy_init()))
		return;

	gzip = true;
}

/*
 * Cl_ShutdownHttpDownload
 */
void Cl_ShutdownHttpDownload(void) {

	Cl_HttpDownloadCleanup();

	curl_easy_cleanup(curl);
	curl = NULL;

	curl_multi_cleanup(curlm);
	curlm = NULL;
}
