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

#include "g_local.h"

/**
 * @brief Give items to a client
 */
static void G_Give_f(g_entity_t *ent) {
	const g_item_t *it;
	uint32_t quantity;
	uint32_t i;
	_Bool give_all;
	g_entity_t *it_ent;

	if (sv_max_clients->integer > 1 && !g_cheats->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Cheats are disabled\n");
		return;
	}

	const char *name = gi.Args();

	if (gi.Argc() == 3) {
		quantity = (uint32_t) strtol(gi.Argv(2), NULL, 10);

		if (quantity > 9999) {
			quantity = 9999;
		}
	} else {
		quantity = 9999;
	}

	if (g_ascii_strcasecmp(name, "all") == 0) {
		give_all = true;
	} else {
		give_all = false;
	}

	if (give_all || g_ascii_strcasecmp(gi.Argv(1), "health") == 0) {
		if (gi.Argc() == 3) {
			ent->locals.health = quantity;
		} else {
			ent->locals.health = ent->locals.max_health + 5;
		}
		if (!give_all) {
			return;
		}
	}

	if (give_all || g_ascii_strcasecmp(name, "armor") == 0) {
		for (i = 0; i < g_num_items; i++) {
			it = G_ItemByIndex(i);
			if (!it->Pickup) {
				continue;
			}
			if (it->type != ITEM_ARMOR) {
				continue;
			}
			if (!G_ArmorInfo(it)) {
				continue;
			}
			ent->client->locals.inventory[i] = it->max;
		}
		if (!give_all) {
			return;
		}
	}

	if (give_all || g_ascii_strcasecmp(name, "weapons") == 0) {
		for (i = 0; i < g_num_items; i++) {
			it = G_ItemByIndex(i);
			if (!it->Pickup) {
				continue;
			}
			if (it->type != ITEM_WEAPON) {
				continue;
			}
			ent->client->locals.inventory[i] += 1;
		}
		if (!give_all) {
			return;
		}
	}

	if (give_all || g_ascii_strcasecmp(name, "ammo") == 0) {
		for (i = 0; i < g_num_items; i++) {
			it = G_ItemByIndex(i);
			if (!it->Pickup) {
				continue;
			}
			if (it->type != ITEM_AMMO) {
				continue;
			}
			G_AddAmmo(ent, it, quantity);
		}
		if (!give_all) {
			return;
		}
	}

	if (give_all) { // we've given full health and inventory
		return;
	}

	it = G_FindItem(name);
	if (!it) {
		name = gi.Argv(1);
		it = G_FindItem(name);
		if (!it) {
			gi.ClientPrint(ent, PRINT_HIGH, "Unknown item: %s\n", name);
			return;
		}
	}

	if (!it->Pickup) {
		gi.ClientPrint(ent, PRINT_HIGH, "Non-pickup item: %s\n", name);
		return;
	}

	if (it->type == ITEM_AMMO) { // give the requested ammo quantity

		if (gi.Argc() == 3) {
			ent->client->locals.inventory[it->index] = quantity;
		} else {
			ent->client->locals.inventory[it->index] += it->quantity;
		}
	} else { // or spawn and touch whatever they asked for
		it_ent = G_AllocEntity_(it->class_name);

		G_SpawnItem(it_ent, it);
		G_TouchItem(it_ent, ent, NULL);

		if (it_ent->in_use) {
			G_FreeEntity(it_ent);
		}
	}
}

/**
 * @brief
 */
static void G_God_f(g_entity_t *ent) {
	char *msg;

	if (sv_max_clients->integer > 1 && !g_cheats->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Cheats are disabled\n");
		return;
	}

	ent->locals.flags ^= FL_GOD_MODE;
	if (!(ent->locals.flags & FL_GOD_MODE)) {
		msg = "god OFF\n";
	} else {
		msg = "god ON\n";
	}

	gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
}

/**
 * @brief
 */
static void G_NoClip_f(g_entity_t *ent) {
	char *msg;

	if (sv_max_clients->integer > 1 && !g_cheats->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Cheats are disabled\n");
		return;
	}

	if (ent->locals.move_type == MOVE_TYPE_NO_CLIP) {
		ent->locals.move_type = MOVE_TYPE_WALK;
		msg = "no_clip OFF\n";
	} else {
		ent->locals.move_type = MOVE_TYPE_NO_CLIP;
		msg = "no_clip ON\n";
	}

	gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
}

/**
 * @brief
 */
static void G_Wave_f(g_entity_t *ent) {

	if (ent->sv_flags & SVF_NO_CLIENT) {
		return;
	}

	if (ent->locals.dead) {
		return;
	}	

	G_SetAnimation(ent, ANIM_TORSO_GESTURE, true);
}

/**
 * @brief
 */
static void G_Use_f(g_entity_t *ent) {

	if (ent->locals.dead) {
		return;
	}

	const char *s = gi.Args();
	const g_item_t *it;
	
	if (s && *s) {
		it = G_FindItem(s);
	} else {
		it = ent->client->locals.last_pickup;
	}
	if (!it) {
		gi.ClientPrint(ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}
	if (!it->Use) {
		gi.ClientPrint(ent, PRINT_HIGH, "Item is not usable\n");
		return;
	}

	if (!ent->client->locals.inventory[it->index]) {
		gi.ClientPrint(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->Use(ent, it);
}

/**
 * @brief
 */
static void G_Drop_f(g_entity_t *ent) {
	const g_item_t *it;

	// we don't drop in instagib or arena
	if (g_level.gameplay) {
		return;
	}

	if (ent->locals.dead) {
		return;
	}

	const char *s = gi.Args();
	it = NULL;

	if (!g_strcmp0(s, "flag")) {
		G_TossFlag(ent);
		return;
	} else if (!g_strcmp0(s, "tech")) {
		G_TossTech(ent);
		return;
	} else { // or just look up the item
		it = G_FindItem(s);
	}

	if (!it) {
		gi.ClientPrint(ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}

	if (!it->Drop) {
		gi.ClientPrint(ent, PRINT_HIGH, "Item can not be dropped\n");
		return;
	}

	const uint16_t index = it->index;

	if (ent->client->locals.inventory[index] == 0) {
		gi.ClientPrint(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	int32_t drop_quantity;

	if (it->type == ITEM_AMMO) {
		drop_quantity = it->quantity;
	} else {
		drop_quantity = 1;
	}

	if (ent->client->locals.inventory[index] < drop_quantity) {
		gi.ClientPrint(ent, PRINT_HIGH, "Quantity too low: %s\n", s);
		return;
	}

	ent->client->locals.inventory[index] -= drop_quantity;
	ent->client->locals.last_dropped = it;

	it->Drop(ent, it);

	// adjust weapon if we need to
	if (it->type == ITEM_WEAPON) {
		if (ent->client->locals.weapon == it &&
			!ent->client->locals.next_weapon &&
			!ent->client->locals.inventory[index]) {
			G_UseBestWeapon(ent);
		}
	}
}

/**
 * @brief
 */
static void G_WeaponLast_f(g_entity_t *ent) {

	g_client_t *cl = ent->client;

	if (!cl->locals.weapon || !cl->locals.prev_weapon) {
		return;
	}

	const uint16_t index = cl->locals.prev_weapon->index;

	if (!cl->locals.inventory[index]) {
		return;
	}

	const g_item_t *it = G_ItemByIndex(index);

	if (!it->Use) {
		return;
	}

	if (it->type != ITEM_WEAPON) {
		return;
	}

	it->Use(ent, it);
}

/**
 * @brief
 */
static void G_Kill_f(g_entity_t *ent) {

	if ((g_level.time - ent->client->locals.respawn_time) < 1000) {
		return;
	}

	if (ent->client->locals.persistent.spectator) {
		return;
	}

	if (ent->locals.dead) {
		return;
	}

	ent->locals.flags &= ~FL_GOD_MODE;

	ent->locals.dead = true;
	ent->locals.health = 0;

	ent->locals.Die(ent, ent, MOD_SUICIDE);
}


/**
 * @brief Server console command for muting players by name (toggles)
 */
void G_Mute_f(void) {
	if (gi.Argc() < 2) {
		return;
	}

	g_client_t *cl = G_ClientByName(va("%s", gi.Argv(2)));

	if (!cl) {
		return;
	}

	if (cl->locals.persistent.muted) {
		cl->locals.persistent.muted = false;
		gi.Print(" %s is now unmuted\n", cl->locals.persistent.net_name);
	} else {
		cl->locals.persistent.muted = true;
		gi.Print(" %s is now muted\n", cl->locals.persistent.net_name);
	}
}

/**
 * @brief This is the client-specific sibling to Cvar_VariableString.
 */
static const char *G_ExpandVariable(g_entity_t *ent, char v) {
	int32_t i;

	switch (v) {

		case 'd': // last dropped item
			if (ent->client->locals.last_dropped) {
				return ent->client->locals.last_dropped->name;
			}
			return "";

		case 'h': // health
			i = ent->client->ps.stats[STAT_HEALTH];
			return va("%d", i);

		case 'a': // armor
			i = ent->client->ps.stats[STAT_ARMOR];
			return va("%d", i);

		default:
			return "";
	}
}

/**
 * @brief
 */
static char *G_ExpandVariables(g_entity_t *ent, const char *text) {
	static char expanded[MAX_STRING_CHARS];
	size_t i, j, len;

	if (!text || !text[0]) {
		return "";
	}

	memset(expanded, 0, sizeof(expanded));
	len = strlen(text);

	for (i = j = 0; i < len; i++) {
		if (text[i] == '%' && i < len - 1) { // expand %variables
			const char *c = G_ExpandVariable(ent, text[i + 1]);
			strcat(expanded, c);
			j += strlen(c);
			i++;
		} else
			// or just append normal chars
		{
			expanded[j++] = text[i];
		}
	}

	return expanded;
}

/**
 * @brief
 */
static void G_Say_f(g_entity_t *ent) {
	char text[MAX_STRING_CHARS];
	char temp[MAX_STRING_CHARS];
	int32_t i;

	g_client_t *cl = ent->client;
	if (cl->locals.persistent.muted) {
		gi.ClientPrint(ent, PRINT_HIGH, "You have been muted\n");
		return;
	}

	text[0] = '\0';

	_Bool team = false; // whether or not we're dealing with team chat
	_Bool arg0 = true; // whether or not we need to print arg0

	if (!g_strcmp0(gi.Argv(0), "say") || !g_strcmp0(gi.Argv(0), "say_team")) {
		arg0 = false;

		if (!g_strcmp0(gi.Argv(0), "say_team") && (g_level.teams || g_level.ctf)) {
			team = true;
		}
	}

	// if g_spectator_chat is off, spectators can only chat to other spectators
	// and so we force team-chat on them
	if (cl->locals.persistent.spectator && !g_spectator_chat->integer) {
		team = true;
	}

	char *s;
	if (arg0) { // not say or say_team, just arbitrary chat from the console
		s = G_ExpandVariables(ent, va("%s %s", gi.Argv(0), gi.Args()));
	} else { // say or say_team
		s = G_ExpandVariables(ent, va("%s", gi.Args()));
	}

	// strip quotes
	if (s[0] == '"' && s[strlen(s) - 1] == '"') {
		s[strlen(s) - 1] = '\0';
		s++;
	}

	// suppress empty messages
	StrStrip(s, temp);
	if (!strlen(temp)) {
		return;
	}

	if (!team) { // chat flood protection, does not pertain to teams

		if (g_level.time < cl->locals.chat_time) {
			return;
		}

		cl->locals.chat_time = g_level.time + 250;
	}

	const int32_t color = team ? ESC_COLOR_TEAMCHAT : ESC_COLOR_CHAT;
	g_snprintf(text, sizeof(text), "%s^%d: %s\n", cl->locals.persistent.net_name, color, s);

	for (i = 1; i <= sv_max_clients->integer; i++) { // print to clients
		const g_entity_t *other = &g_game.entities[i];

		if (!other->in_use) {
			continue;
		}

		if (team) {
			if (!G_OnSameTeam(ent, other)) {
				continue;
			}
			gi.ClientPrint(other, PRINT_TEAM_CHAT, "%s", text);
		} else {
			gi.ClientPrint(other, PRINT_CHAT, "%s", text);
		}
	}

	if (dedicated->value) { // print to the console
		gi.Print("%s", text);
	}
}

/**
 * @brief
 */
static void G_PlayerList_f(g_entity_t *ent) {
	int32_t i, seconds;
	char st[80];
	char text[MAX_PRINT_MSG];
	g_entity_t *e2;

	memset(text, 0, sizeof(text));

	// connect time, ping, score, name
	for (i = 0, e2 = g_game.entities + 1; i < sv_max_clients->integer; i++, e2++) {

		if (!e2->in_use) {
			continue;
		}

		seconds = (g_level.frame_num - e2->client->locals.persistent.first_frame) / QUETOO_TICK_RATE;

		g_snprintf(st, sizeof(st), "%02d:%02d %4d %3d %-16s %s\n", (seconds / 60), (seconds % 60),
		           e2->client->ping, e2->client->locals.persistent.score,
		           e2->client->locals.persistent.net_name,
		           e2->client->locals.persistent.skin);

		if (strlen(text) + strlen(st) > sizeof(text) - 200) {
			sprintf(text + strlen(text), "And more...\n");
			gi.ClientPrint(ent, PRINT_HIGH, "%s", text);
			return;
		}

		strcat(text, st);
	}
	gi.ClientPrint(ent, PRINT_HIGH, "%s", text);
}

/**
 * @brief Returns true if the client's team was changed, false otherwise.
 */
_Bool G_AddClientToTeam(g_entity_t *ent, const char *team_name) {
	g_team_t *team;

	if (g_level.match_time && g_level.match_time <= g_level.time) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match has already started\n");
		return false;
	}

	if (!(team = G_TeamByName(team_name))) { // resolve team
		gi.ClientPrint(ent, PRINT_HIGH, "Team \"%s\" doesn't exist\n", team_name);
		return false;
	}

	if (ent->client->locals.persistent.team == team) {
		return false;
	}

	if (g_level.gameplay == GAME_DUEL && G_TeamSize(team) > 0) {
		gi.ClientPrint(ent, PRINT_HIGH, "Only 1 player per team allowed in Duel mode\n");
		return false;
	}

	if (!ent->client->locals.persistent.spectator) { // changing teams
		G_TossQuadDamage(ent);
		G_TossFlag(ent);
		G_TossTech(ent);
		G_ClientHookDetach(ent);
	}

	ent->client->locals.persistent.team = team;
	ent->client->locals.persistent.spectator = false;
	ent->client->locals.persistent.ready = false;

	char *user_info = g_strdup(ent->client->locals.persistent.user_info);
	G_ClientUserInfoChanged(ent, user_info);
	g_free(user_info);

	return true;
}

/**
 * @brief
 */
static void G_AddClientToRound(g_entity_t *ent) {
	int32_t score; // keep score across rounds

	if (g_level.round_time && g_level.round_time <= g_level.time) {
		gi.ClientPrint(ent, PRINT_HIGH, "Round has already started\n");
		return;
	}

	score = ent->client->locals.persistent.score;

	if (g_level.teams) { // attempt to add client to team
		if (!G_AddClientToTeam(ent, gi.Argv(1))) {
			return;
		}
	} else { // simply allow them to join
		if (!ent->client->locals.persistent.spectator) {
			return;
		}
		ent->client->locals.persistent.spectator = false;
	}

	G_ClientRespawn(ent, true);
	ent->client->locals.persistent.score = score; // lastly restore score
}

/**
 * @brief
 */
static void G_Team_f(g_entity_t *ent) {

	if ((g_level.teams || g_level.ctf) && gi.Argc() != 2) {
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <team name>\n", gi.Argv(0));
		return;
	}

	if (g_level.rounds) { // special case for rounds play
		G_AddClientToRound(ent);
		return;
	}

	if (!g_level.teams && !g_level.ctf) {
		gi.ClientPrint(ent, PRINT_HIGH, "Teams are disabled\n");
		return;
	}

	if (!G_AddClientToTeam(ent, gi.Argv(1))) {
		return;
	}

	G_ClientRespawn(ent, true);
}

/**
 * @brief
 */
static void G_Unready_f(g_entity_t *ent) {

	if (!g_level.match) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match is disabled\n");
		return;
	}

	if (ent->client->locals.persistent.spectator) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're a spectator\n");
		return;
	}

	if (!g_level.warmup && g_level.match_time <= g_level.time) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match has started\n");
		return;
	}

	if (!ent->client->locals.persistent.ready) {
		gi.ClientPrint(ent, PRINT_HIGH, "You are not ready\n");
		return;
	}

	ent->client->locals.persistent.ready = false;
	gi.BroadcastPrint(PRINT_HIGH, "^7%s ^4is no longer ready%s\n",
	                  ent->client->locals.persistent.net_name,
	                  (g_level.start_match) ? ", countdown aborted" : ""
	                 );

	g_level.start_match = false;
	g_level.match_time = 0;
	g_level.match_status = MSTAT_WARMUP;
}

/**
 * @brief If match is enabled, all clients must issue ready for game to start.
 */
static void G_Ready_f(g_entity_t *ent) {
	int32_t i, clients;
	g_client_t *cl;

	if (!g_level.match) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match is disabled\n");
		return;
	}

	if (ent->client->locals.persistent.spectator) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're a spectator\n");
		return;
	}

	if (ent->client->locals.persistent.ready) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're already ready, dumbass\n");
		return;
	}

	ent->client->locals.persistent.ready = true;
	gi.BroadcastPrint(PRINT_CHAT, "^7%s ^4is ready\n", ent->client->locals.persistent.net_name);

	clients = 0;

	uint8_t teams_ready[MAX_TEAMS];
	memset(teams_ready, 0, sizeof(teams_ready));

	for (i = 0; i < sv_max_clients->integer; i++) { // is everyone ready?
		cl = g_game.clients + i;

		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		if (cl->locals.persistent.spectator) {
			continue;
		}

		if (!cl->locals.persistent.ready) {
			break;
		}

		clients++;

		if (g_level.teams || g_level.ctf) {
			teams_ready[cl->locals.persistent.team->id]++;
		}
	}

	if (i != (int32_t) sv_max_clients->integer) { // someone isn't ready
		return;
	}

	if (clients < 2) { // need at least 2 clients to trigger match
		return;
	}

	if (g_level.teams || g_level.ctf) { // need at least 1 player per team

		for (int32_t i = 0; i < g_level.num_teams; i++) {

			if (!teams_ready[i]) {
				return;
			}
		}
	}

	if ((int32_t) g_level.teams == 2 || (int32_t) g_level.ctf == 2) { // balanced teams required

		for (int32_t i = 1; i < g_level.num_teams; i++) {
			if (teams_ready[i] != teams_ready[0]) {
				gi.BroadcastPrint(PRINT_HIGH, "Teams must be balanced for match to start\n");
				return;
			}
		}
	}

	g_warmup_time->integer = Clampf(g_warmup_time->integer, 0, 30);

	gi.BroadcastPrint(PRINT_HIGH, "Match starting in %d seconds...\n", g_warmup_time->integer);
	g_level.match_time = g_level.time + (g_warmup_time->integer * 1000);

	g_level.start_match = true;
	g_level.match_status = MSTAT_COUNTDOWN;
}

/**
 * @brief
 */
static void G_ToggleReady_f(g_entity_t *ent) {
	if (ent->client->locals.persistent.ready) {
		G_Unready_f(ent);
	} else {
		G_Ready_f(ent);
	}
}

/**
 * @brief
 */
static void G_Spectate_f(g_entity_t *ent) {

	// prevent spectator spamming
	if (g_level.time - ent->client->locals.respawn_time < 1000) {
		return;
	}

	if (!g_strcmp0(gi.Argv(0), "spectate")) {

		if (ent->client->locals.persistent.spectator) {
			gi.ClientPrint(ent, PRINT_HIGH, "You are already spectating\n");
			return;
		}

		if (g_level.gameplay == GAME_DEATHMATCH || g_level.gameplay == GAME_DUEL) {
			G_TossQuadDamage(ent);
		}

		G_TossFlag(ent);
		G_TossTech(ent);
		G_ClientHookDetach(ent);

		gi.WriteByte(SV_CMD_MUZZLE_FLASH);
		gi.WriteShort(ent->s.number);
		gi.WriteByte(MZ_LOGOUT);
		gi.Multicast(ent->s.origin, MULTICAST_PHS, NULL);

	} else if (!g_strcmp0(gi.Argv(0), "join")) {

		if (!ent->client->locals.persistent.spectator) {
			gi.ClientPrint(ent, PRINT_HIGH, "You have already joined\n");
			return;
		}

		// prevent spectators from joining matches
		if (!g_level.warmup && g_level.match_time && ent->client->locals.persistent.spectator) {
			gi.ClientPrint(ent, PRINT_HIGH, "Match has already started\n");
			return;
		}

		// prevent spectators from joining rounds
		if (g_level.round_time && ent->client->locals.persistent.spectator) {
			gi.ClientPrint(ent, PRINT_HIGH, "Round has already started\n");
			return;
		}

		if (g_level.teams || g_level.ctf) {
			if (g_auto_join->value) { // assign them to a team
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
			} else { // or ask them to pick
				gi.ClientPrint(ent, PRINT_HIGH, "Use team <team name> to join the game\n");
				return;
			}
		}
	}

	ent->client->locals.persistent.spectator = !ent->client->locals.persistent.spectator;
	G_ClientRespawn(ent, true);
}

/**
 * @brief resumes the current match
 */
static void G_Timein_f(g_entity_t *ent) {

	gi.BroadcastPrint(PRINT_HIGH, "%s resumed the game, get ready\n",
	                  ent->client->locals.persistent.net_name);

	// assuming there is more than 10 seconds left in TO, move clock to 10 sec
	if (g_level.timeout_time - g_level.time > 10000) {
		g_level.timeout_time = g_level.time + 10000;
	}
}

/**
 * @brief pause the current match, allow for limited commands during
 */
static void G_Timeout_f(g_entity_t *ent) {

	if (!G_MatchIsPlaying()) {
		gi.ClientPrint(ent, PRINT_HIGH, "No match in progress...\n");
		return;
	}

	if (!ent->client->locals.persistent.team && !ent->client->locals.persistent.admin) {
		gi.ClientPrint(ent, PRINT_HIGH, "Only players or admins can pause the match...\n");
		return;
	}

	if (!G_MatchIsTimeout()) {
		G_CallTimeOut(ent);
		return;
	}

	if ((g_level.timeout_caller == ent || ent->client->locals.persistent.admin)) {
		G_Timein_f(ent);
		return;
	}

	gi.ClientPrint(ent, PRINT_HIGH, "You cannot cancel timeout\n");
}

/**
 * @brief
 */
static void G_Admin_f(g_entity_t *ent) {

	if (strlen(g_admin_password->string) == 0) { // blank password (default) disabled
		gi.ClientPrint(ent, PRINT_HIGH, "Admin features disabled\n");
		return;
	}

	if (gi.Argc() < 2) {	// no arguments supplied, show help
		if (!ent->client->locals.persistent.admin) {
			gi.ClientPrint(ent, PRINT_HIGH, "Usage: admin <password>\n");
		} else {
			gi.ClientPrint(ent, PRINT_HIGH, "Admin commands:\n");
			gi.ClientPrint(ent, PRINT_HIGH, "kick, remove, mute, unmute, timeout, timein\n");
		}
		return;
	}

	if (!ent->client->locals.persistent.admin) { // not yet an admin, assuming auth
		if (g_strcmp0(gi.Argv(1), g_admin_password->string) == 0) {
			ent->client->locals.persistent.admin = true;
			gi.BroadcastPrint(PRINT_HIGH, "%s became an admin\n", ent->client->locals.persistent.net_name);
		} else {
			gi.ClientPrint(ent, PRINT_HIGH, "Invalid admin password\n");
		}
		return;
	}

	if (gi.Argc() > 2) {
		if (g_strcmp0(gi.Argv(2), "mute") == 0) {
			G_MuteClient(va("%s", gi.Argv(3)), true);
		}
	}
}

#ifdef _DEBUG
void G_RecordPmove(void);
void G_PlayPmove(void);
#endif

/**
 * @brief
 */
void G_ClientCommand(g_entity_t *ent) {

	if (!ent->client) {
		return; // not fully in game yet
	}

	const char *cmd = gi.Argv(0);

	if (g_strcmp0(cmd, "say") == 0) {
		G_Say_f(ent);
		return;
	}
	if (g_strcmp0(cmd, "say_team") == 0) {
		G_Say_f(ent);
		return;
	}

	// most commands can not be executed during intermission
	if (g_level.intermission_time) {
		return;
	}

	// these commands are allowed in a timeout
	if (g_strcmp0(cmd, "timeout") == 0 || g_strcmp0(cmd, "timein") == 0) {
		G_Timeout_f(ent);
		return;

	} else if (g_strcmp0(cmd, "admin") == 0) {
		G_Admin_f(ent);
		return;
	}

	if (G_MatchIsTimeout()) {
		gi.ClientPrint(ent, PRINT_HIGH, "'%s' not allowed during a timeout\n", cmd);
		return;
	}

	// these commands are not allowed during intermission or timeout
	if (g_strcmp0(cmd, "spectate") == 0 || g_strcmp0(cmd, "join") == 0) {
		G_Spectate_f(ent);
	} else if (g_strcmp0(cmd, "team") == 0) {
		G_Team_f(ent);
	} else if (g_strcmp0(cmd, "ready") == 0) {
		G_Ready_f(ent);
	} else if (g_strcmp0(cmd, "unready") == 0) {
		G_Unready_f(ent);
	} else if (g_strcmp0(cmd, "toggle_ready") == 0) {
		G_ToggleReady_f(ent);
	} else if (g_strcmp0(cmd, "use") == 0) {
		G_Use_f(ent);
	} else if (g_strcmp0(cmd, "drop") == 0) {
		G_Drop_f(ent);
	} else if (g_strcmp0(cmd, "give") == 0) {
		G_Give_f(ent);
	} else if (g_strcmp0(cmd, "god") == 0) {
		G_God_f(ent);
	} else if (g_strcmp0(cmd, "no_clip") == 0) {
		G_NoClip_f(ent);
	} else if (g_strcmp0(cmd, "wave") == 0) {
		G_Wave_f(ent);
	} else if (g_strcmp0(cmd, "weapon_last") == 0) {
		G_WeaponLast_f(ent);
	} else if (g_strcmp0(cmd, "kill") == 0) {
		G_Kill_f(ent);
	} else if (g_strcmp0(cmd, "player_list") == 0) {
		G_PlayerList_f(ent);
	} else if (g_strcmp0(cmd, "chase_previous") == 0) {
		G_ClientChasePrevious(ent);
	} else if (g_strcmp0(cmd, "chase_next") == 0) {
		G_ClientChaseNext(ent);
	}
#ifdef _DEBUG
	else if (g_strcmp0(cmd, "pmove_record") == 0) {
		G_RecordPmove();
	} else if (g_strcmp0(cmd, "pmove_play") == 0) {
		G_PlayPmove();
	}
#endif

	else
		// anything that doesn't match a command will be a chat
	{
		G_Say_f(ent);
	}
}
