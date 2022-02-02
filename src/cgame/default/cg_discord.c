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
#include <discord_register.h>
#include <discord_rpc.h>

#define DISCORD_APP_ID				378347526203637760

enum EDiscordResult {
	DiscordResult_Ok,
	DiscordResult_ServiceUnavailable,
	DiscordResult_InvalidVersion,
	DiscordResult_LockFailed,
	DiscordResult_InternalError,
	DiscordResult_InvalidPayload,
	DiscordResult_InvalidCommand,
	DiscordResult_InvalidPermissions,
	DiscordResult_NotFetched,
	DiscordResult_NotFound,
	DiscordResult_Conflict,
	DiscordResult_InvalidSecret,
	DiscordResult_InvalidJoinSecret,
	DiscordResult_NoEligibleActivity,
	DiscordResult_InvalidInvite,
	DiscordResult_NotAuthenticated,
	DiscordResult_InvalidAccessToken,
	DiscordResult_ApplicationMismatch,
	DiscordResult_InvalidDataUrl,
	DiscordResult_InvalidBase64,
	DiscordResult_NotFiltered,
	DiscordResult_LobbyFull,
	DiscordResult_InvalidLobbySecret,
	DiscordResult_InvalidFilename,
	DiscordResult_InvalidFileSize,
	DiscordResult_InvalidEntitlement,
	DiscordResult_NotInstalled,
	DiscordResult_NotRunning,
	DiscordResult_InsufficientBuffer,
	DiscordResult_PurchaseCanceled,
	DiscordResult_InvalidGuild,
	DiscordResult_InvalidEvent,
	DiscordResult_InvalidChannel,
	DiscordResult_InvalidOrigin,
	DiscordResult_RateLimited,
	DiscordResult_OAuth2Error,
	DiscordResult_SelectChannelTimeout,
	DiscordResult_GetGuildTimeout,
	DiscordResult_SelectVoiceForceRequired,
	DiscordResult_CaptureShortcutAlreadyListening,
	DiscordResult_UnauthorizedForAchievement,
	DiscordResult_InvalidGiftCode,
	DiscordResult_PurchaseError,
	DiscordResult_TransactionAborted
};

static const char *Cg_DiscordErrorString(enum EDiscordResult result) {

	switch (result) {
		case DiscordResult_Ok: return "Ok";
		case DiscordResult_ServiceUnavailable: return "Service Unavailable";
		case DiscordResult_InvalidVersion: return "Invalid Version";
		case DiscordResult_LockFailed: return "Lock Failed";
		case DiscordResult_InternalError: return "Internal Error";
		case DiscordResult_InvalidPayload: return "Invalid Payload";
		case DiscordResult_InvalidCommand: return "Invalid Command";
		case DiscordResult_InvalidPermissions: return "Invalid Permissions";
		case DiscordResult_NotFetched: return "Not Fetched";
		case DiscordResult_NotFound: return "Not Found";
		case DiscordResult_Conflict: return "Conflict";
		case DiscordResult_InvalidSecret: return "Invalid Secret";
		case DiscordResult_InvalidJoinSecret: return "Invalid Join Secret";
		case DiscordResult_NoEligibleActivity: return "No Eligible Activity";
		case DiscordResult_InvalidInvite: return "Invalid Invite";
		case DiscordResult_NotAuthenticated: return "Not Authenticated";
		case DiscordResult_InvalidAccessToken: return "Invalid Access Token";
		case DiscordResult_ApplicationMismatch: return "Application Mismatch";
		case DiscordResult_InvalidDataUrl: return "Invalid Data Url";
		case DiscordResult_InvalidBase64: return "Invalid Base64";
		case DiscordResult_NotFiltered: return "Not Filtered";
		case DiscordResult_LobbyFull: return "Lobby Full";
		case DiscordResult_InvalidLobbySecret: return "Invalid Lobby Secret";
		case DiscordResult_InvalidFilename: return "Invalid Filename";
		case DiscordResult_InvalidFileSize: return "Invalid File Size";
		case DiscordResult_InvalidEntitlement: return "Invalid Entitlement";
		case DiscordResult_NotInstalled: return "Not Installed";
		case DiscordResult_NotRunning: return "Not Running";
		case DiscordResult_InsufficientBuffer: return "Insufficient Buffer";
		case DiscordResult_PurchaseCanceled: return "Purchase Canceled";
		case DiscordResult_InvalidGuild: return "Invalid Guild";
		case DiscordResult_InvalidEvent: return "Invalid Event";
		case DiscordResult_InvalidChannel: return "Invalid Channel";
		case DiscordResult_InvalidOrigin: return "Invalid Origin";
		case DiscordResult_RateLimited: return "Rate Limited";
		case DiscordResult_OAuth2Error: return "OAuth2 Error";
		case DiscordResult_SelectChannelTimeout: return "Select Channel Timeout";
		case DiscordResult_GetGuildTimeout: return "Get Guild Timeout";
		case DiscordResult_SelectVoiceForceRequired: return "Select Voice Force Required";
		case DiscordResult_CaptureShortcutAlreadyListening: return "Capture Shortcut Already Listening";
		case DiscordResult_UnauthorizedForAchievement: return "Unauthorized For Achievement";
		case DiscordResult_InvalidGiftCode: return "Invalid Gift Code";
		case DiscordResult_PurchaseError: return "Purchase Error";
		case DiscordResult_TransactionAborted: return "Transaction Aborted";
	}

	return "Unknown Error ID";
}

static void Cg_DiscordDisconnected(int errorCode, const char* message) {
	cgi.Warn("Discord error %s: %s\n", Cg_DiscordErrorString(errorCode), message);
}

typedef enum {
	INVALID,
	IN_MENU,
	PLAYING
} cg_discord_status_t;

typedef struct {
	_Bool initialized;
	cg_discord_status_t status;
} cg_discord_state_t;

static cg_discord_state_t cg_discord_state;

static void Cg_DiscordReady(const DiscordUser *user) {

	cgi.Print("Discord Loaded (%s)\n", user->username);
	cg_discord_state.initialized = true;
}

static const char *Cg_GetGameMode(void) {
	const char *round_buffer = "";

	if (cg_state.num_rounds) {
		round_buffer = va(" (Round %i/%i)", cg_state.round, cg_state.num_rounds);
	}

	if (cg_state.ctf) {
		return va("%i-Team CTF%s", cg_state.num_teams, round_buffer);
	} else if (cg_state.num_teams) {
		return va("%i-Team Deathmatch%s", cg_state.num_teams, round_buffer);
	} else if (cg_state.match) {
		return va("Match Mode%s", round_buffer);
	} else switch (cg_state.gameplay) {
		case GAME_DEATHMATCH:
			return va("Deathmatch%s", round_buffer);
		case GAME_ARENA:
			return va("Arena%s", round_buffer);
		case GAME_DUEL:
			return va("Duel%s", round_buffer);
		case GAME_INSTAGIB:
			return va("Instagib%s", round_buffer);
	}

	return va("Deathmatch%s", round_buffer);
}

void Cg_UpdateDiscord(void) {

	if (cg_discord_state.initialized) {
		DiscordRichPresence presence = { 0 };
		_Bool needs_update = false;

		char details[128];
		char joinSecret[128];
		char spectateSecret[128];

		if (*cgi.state == CL_ACTIVE) {
			if (cg_discord_state.status != PLAYING) {
				needs_update = true;
			
				presence.largeImageKey = "default";
				presence.state = "Playing";

				g_snprintf(details, sizeof(details), "%s - %s", Cg_GetGameMode(), cgi.ConfigString(CS_NAME));
				presence.details = details;

				if (g_strcmp0(cgi.server_name, "localhost")) {
					presence.partyId = cgi.server_name;

					g_snprintf(joinSecret, sizeof(joinSecret), "JOIN_%s", presence.partyId);
					presence.joinSecret = joinSecret;

					g_snprintf(spectateSecret, sizeof(spectateSecret), "SPCT_%s", presence.partyId);
					presence.spectateSecret = spectateSecret;
				}

				presence.partySize = cg_state.num_clients;
				presence.partyMax = cg_state.max_clients;
				cg_discord_state.status = PLAYING;
				presence.instance = true;
			}
		} else {
			if (cg_discord_state.status != IN_MENU) {
				needs_update = true;
			
				presence.largeImageKey = "default";
				presence.state = "In Main Menu";

				cg_discord_state.status = IN_MENU;
			}
		}

		if (needs_update) {
			Discord_UpdatePresence(&presence);
		}
	}

	Discord_RunCallbacks();
}

static void Cg_DiscordJoinGame(const char *secret) {

	if (g_ascii_strncasecmp(secret, "JOIN_", 5)) {
		cgi.Warn("Invalid invitation\n");
		return;
	}

	// just to clear any pending spectator value
	cgi.SetCvarInteger("spectator", 0);

	cgi.Cbuf(va("connect %s\n", (secret + 5)));
}

static void Cg_DiscordJoinRequest(const DiscordUser *request) {

	// TODO: display UI for this
	Discord_Respond(request->userId, DISCORD_REPLY_YES);
}

static void Cg_DiscordSpectateGame(const char *secret) {

	if (g_ascii_strncasecmp(secret, "SPCT_", 5)) {
		cgi.Warn("Invalid invitation\n");
		return;
	}

	cgi.SetCvarInteger("spectator", 1);

	cgi.Cbuf(va("connect %s\n", (secret + 5)));
}

void Cg_InitDiscord(void) {

	static DiscordEventHandlers handlers = {
		.ready = Cg_DiscordReady,
		.disconnected = Cg_DiscordDisconnected,
		.joinGame = Cg_DiscordJoinGame,
		.joinRequest = Cg_DiscordJoinRequest,
		.errored = Cg_DiscordDisconnected,
		.spectateGame = Cg_DiscordSpectateGame
	};

	Discord_Initialize(G_STRINGIFY(DISCORD_APP_ID), &handlers, 1, NULL);
}

void Cg_ShutdownDiscord(void) {

	Discord_Shutdown();

	memset(&cg_discord_state, 0, sizeof(cg_discord_state));
}