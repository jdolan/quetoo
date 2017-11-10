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

static void Cg_DiscordReady(void) {
	cgi.Print("Discord Initialized\n");
}

static void Cg_DiscordError(int code, const char *message) {
	cgi.Print("Discord Error: %s (%i)\n", message, code);
}

static void Cg_DiscordDisconnected(int code, const char *message) {
	cgi.Print("Discord Disconnected\n");
}

static void Cg_DiscordJoinGame(const char *joinSecret) {
}

static void Cg_DiscordSpectateGame(const char *spectateSecret) {
}

static void Cg_DiscordJoinRequest(const DiscordJoinRequest *request) {
}

void Cg_DiscordUpdate(void) {

	if (*cgi.state == CL_ACTIVE) {
		DiscordRichPresence discordPresence;
		memset(&discordPresence, 0, sizeof(discordPresence));

		discordPresence.state = "Dying";
		discordPresence.details = "Being Cool";
		discordPresence.startTimestamp = discordPresence.endTimestamp = time(NULL);
		discordPresence.largeImageKey = "default";

		if (cgi.WorldModel()) {
			discordPresence.largeImageText = cgi.WorldModel()->media.name;
		}

		discordPresence.smallImageKey = "default";
		discordPresence.smallImageText = "Qforcer";

		discordPresence.instance = 0;

		Discord_UpdatePresence(&discordPresence);
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

	Discord_Initialize(DISCORD_APP_ID, &handlers, 1, NULL);
}

void Cg_ShutdownDiscord(void) {

	Discord_Shutdown();
}