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
#include <discord_game_sdk.h>

#define DISCORD_APP_ID				378347526203637760

static struct IDiscordCore *cg_discord_core;
static struct IDiscordUserManager *cg_discord_user_manager;
static struct IDiscordActivityManager *cg_discord_activity_manager;
static enum EDiscordResult cg_discord_error;

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

/**
 * @brief Convenience function to run a Discord function & set global error.
 */
#define Cg_DiscordCall(...) \
	(cg_discord_error = __VA_ARGS__)

/**
 * @brief Convenience function to run a Discord function, assert
 * & print warnings.
 */
#define Cg_DiscordAssert(...) \
	({\
		if (Cg_DiscordCall(__VA_ARGS__)) { \
			cgi.Warn("Discord error: %s\n", Cg_DiscordErrorString(cg_discord_error)); \
		} \
		cg_discord_error; \
	})

/**
 * @brief Convenience function to run a Discord function, assert
 * & print warnings, disable Discord integration & return on failure.
 */
#define Cg_DiscordRequire(...) \
	if (Cg_DiscordAssert(__VA_ARGS__)) { \
		Cg_ShutdownDiscord(); \
		return; \
	}

typedef enum {
	INVALID,
	IN_MENU,
	PLAYING
} cg_discord_status_t;

typedef struct {
	_Bool initialized;
	cg_discord_status_t status;
	struct DiscordUser user;
	struct DiscordActivity presence;
} cg_discord_state_t;

static cg_discord_state_t cg_discord_state;

static void Cg_DiscordCurrentUserUpdated(void *event_data) {
	Cg_DiscordRequire(cg_discord_user_manager->get_current_user(cg_discord_user_manager, &cg_discord_state.user));

	cgi.Print("Discord Loaded (%s)\n", cg_discord_state.user.username);
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

static void Cg_UpdateDiscordCallback(void *callback_data, enum EDiscordResult result) {
	Cg_DiscordRequire(result);
}

void Cg_UpdateDiscord(void) {
	
	if (!cg_discord_core) {
		return;
	}

	if (cg_discord_state.initialized) {
		_Bool needs_update = false;

		if (*cgi.state == CL_ACTIVE) {
			if (cg_discord_state.status != PLAYING) {
				needs_update = true;

				memset(&cg_discord_state.presence, 0, sizeof(cg_discord_state.presence));
				cg_discord_state.presence.application_id = DISCORD_APP_ID;
			
				g_strlcpy(cg_discord_state.presence.assets.large_image, "default", sizeof(cg_discord_state.presence.assets.large_image));
				g_strlcpy(cg_discord_state.presence.state, "Playing", sizeof(cg_discord_state.presence.state));

				g_snprintf(cg_discord_state.presence.details, sizeof(cg_discord_state.presence.details), "%s - %s", Cg_GetGameMode(), cgi.ConfigString(CS_NAME));

				g_strlcpy(cg_discord_state.presence.secrets.join, cgi.server_name, sizeof(cg_discord_state.presence.secrets.join));
				cg_discord_state.presence.party.size.current_size = cg_state.num_clients;
				cg_discord_state.presence.party.size.max_size = cg_state.max_clients;
				cg_discord_state.status = PLAYING;
				cg_discord_state.presence.instance = true;
			}
		} else {
			if (cg_discord_state.status != IN_MENU) {
				needs_update = true;

				memset(&cg_discord_state.presence, 0, sizeof(cg_discord_state.presence));
				cg_discord_state.presence.application_id = DISCORD_APP_ID;
			
				g_strlcpy(cg_discord_state.presence.assets.large_image, "default", sizeof(cg_discord_state.presence.assets.large_image));
				g_strlcpy(cg_discord_state.presence.state, "In Main Menu", sizeof(cg_discord_state.presence.state));

				cg_discord_state.status = IN_MENU;
			}
		}

		if (needs_update) {
			cg_discord_activity_manager->update_activity(cg_discord_activity_manager, &cg_discord_state.presence, NULL, Cg_UpdateDiscordCallback);
		}
	}

	Cg_DiscordRequire(cg_discord_core->run_callbacks(cg_discord_core));
}

static void Cg_DiscordLogHook(void *hook_data, enum EDiscordLogLevel level, const char *message) {
	cgi.Print("Discord: %s\n", message);
}

void Cg_InitDiscord(void) {

	static struct DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);
	params.client_id = DISCORD_APP_ID;
	params.flags = DiscordCreateFlags_NoRequireDiscord;
	
	static struct IDiscordUserEvents users_events;
    users_events.on_current_user_update = Cg_DiscordCurrentUserUpdated;
	params.user_events = &users_events;

	if (Cg_DiscordAssert(DiscordCreate(DISCORD_VERSION, &params, &cg_discord_core))) {
		return;
	}

	cg_discord_core->set_log_hook(cg_discord_core, DiscordLogLevel_Error, NULL, Cg_DiscordLogHook);

	cg_discord_user_manager = cg_discord_core->get_user_manager(cg_discord_core);
	cg_discord_activity_manager = cg_discord_core->get_activity_manager(cg_discord_core);
}

void Cg_ShutdownDiscord(void) {

	if (cg_discord_core) {
		cg_discord_core->destroy(cg_discord_core);
		cg_discord_core = NULL;
	}

	memset(&cg_discord_state, 0, sizeof(cg_discord_state));
}