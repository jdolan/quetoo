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

#include "cg_local.h"
#include "discord-rpc.h"

#define DISCORD_APP_ID				"378347526203637760"

typedef enum {
	INVALID,
	IN_MENU,
	PLAYING
} cg_discord_status_t;

typedef struct {
	_Bool initialized;
	cg_discord_status_t status;
	DiscordRichPresence presence;
} cg_discord_state_t;

static cg_discord_state_t cg_discord_state;

static void Cg_DiscordReady(void) {

	cgi.Print("Discord Initialized\n");
	cg_discord_state.initialized = true;
}

static void Cg_DiscordError(int code, const char *message) {

	cgi.Warn("Discord Error: %s (%i)\n", message, code);
}

static void Cg_DiscordDisconnected(int code, const char *message) {

	cgi.Print("Discord Disconnected\n");
}

static void Cg_DiscordJoinGame(const char *joinSecret) {

	cgi.Cbuf(va("connect %s\n", joinSecret));
}

static void Cg_DiscordSpectateGame(const char *spectateSecret) {
}

static void Cg_DiscordJoinRequest(const DiscordJoinRequest *request) {
}

static const char *Cg_GetGameMode(void) {
	if (cg_state.ctf) {
		return va("%i-Team CTF", cg_state.teams);
	} else if (cg_state.teams) {
		return va("%i-Team Deathmatch", cg_state.teams);
	} else if (cg_state.match) {
		return "Match Mode";
	}

	return "Deathmatch";
}

void Cg_UpdateDiscord(void) {

	_Bool needs_update = false;
	static char descBuffer[128];

	if (*cgi.state == CL_ACTIVE) {
		if (cg_discord_state.status != PLAYING) {
			needs_update = true;

			memset(&cg_discord_state.presence, 0, sizeof(cg_discord_state.presence));
			
			cg_discord_state.presence.largeImageKey = "default";
			cg_discord_state.presence.state = "Playing";

			g_snprintf(descBuffer, sizeof(descBuffer), "%s - %s", Cg_GetGameMode(), cgi.ConfigString(CS_NAME));

			cg_discord_state.presence.joinSecret = cgi.server_name;
			cg_discord_state.presence.details = descBuffer;
			cg_discord_state.presence.partySize = cg_state.numclients;
			cg_discord_state.presence.partyMax = cg_state.maxclients;
			cg_discord_state.status = PLAYING;
			cg_discord_state.presence.instance = 1;
		}
	} else {
		if (cg_discord_state.status != IN_MENU) {
			needs_update = true;

			memset(&cg_discord_state.presence, 0, sizeof(cg_discord_state.presence));
			
			cg_discord_state.presence.largeImageKey = "default";
			cg_discord_state.presence.state = "In Main Menu";

			cg_discord_state.status = IN_MENU;
		}
	}

	if (needs_update) {
		Discord_UpdatePresence(&cg_discord_state.presence);
	}

#ifdef DISCORD_DISABLE_IO_THREAD
	Discord_UpdateConnection();
#endif

	Discord_RunCallbacks();
}

void Cg_InitDiscord(void) {
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = Cg_DiscordReady;
	handlers.errored = Cg_DiscordError;
	handlers.disconnected = Cg_DiscordDisconnected;
	handlers.joinGame = Cg_DiscordJoinGame;
	handlers.spectateGame = Cg_DiscordSpectateGame;
	handlers.joinRequest = Cg_DiscordJoinRequest;

	memset(&cg_discord_state, 0, sizeof(cg_discord_state));
	Discord_Initialize(DISCORD_APP_ID, &handlers, 1, "");
}

void Cg_ShutdownDiscord(void) {

	Discord_Shutdown();
	memset(&cg_discord_state, 0, sizeof(cg_discord_state));
}