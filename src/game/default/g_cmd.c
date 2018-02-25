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

	if (g_max_clients->integer > 1 && !g_cheats->value) {
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
		G_TouchItem(it_ent, ent, NULL, NULL);

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

	if (g_max_clients->integer > 1 && !g_cheats->value) {
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

	if (g_max_clients->integer > 1 && !g_cheats->value) {
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
	const g_item_t *it = G_FindItem(s);
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

	uint16_t drop_quantity;

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
 *  * @brief Server console command - force a command on a particular player
 *   */
void G_Stuff_Sv_f(void) {

	char cmd[MAX_STRING_CHARS];

	if (gi.Argc() < 3) {
		gi.Print(" Usage: stuff <clientname> <command to execute>\n");
		return;
	}

	const g_entity_t *ent = G_EntityByName(va("%s", gi.Argv(2)));

	if (!ent) {
		return;
	}

	// build the command 1 is the target
	for (uint8_t i = 2; i < gi.Argc(); i++) {
		g_strlcat(cmd, va("%s ", gi.Argv(i)), sizeof(cmd));
	}


	G_ClientStuff(ent, va("%s", cmd));
}


/**
 * @brief Server console command - force a command on all connected clients
 */
void G_StuffAll_Sv_f(void) {

	if (gi.Argc() < 2) {
		gi.Print(" Usage: stuff_all <command>\n");
		return;
	}

	const char *cmd = gi.Args();

	for (int32_t i = 0; i < g_max_clients->integer; i++) {
		const g_entity_t *ent = &g_game.entities[i + 1];
		if (!ent->in_use) {
			continue;
		}

		G_ClientStuff(ent, cmd);
	}
}

/**
 * @brief Server console command for muting players by name (toggles)
 */
void G_Mute_Sv_f(void) {
	if (gi.Argc() < 2) {
		return;
	}

	g_client_t *cl = G_ClientByName(va("%s", gi.Argv(2)));

	if (!cl) {
		return;
	}

	if (cl->locals.muted) {
		cl->locals.muted = false;
		gi.Print(" %s is now unmuted\n", cl->locals.persistent.net_name);
	} else {
		cl->locals.muted = true;
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
	if (cl->locals.muted) {
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
	StripColors(s, temp);
	if (!strlen(temp)) {
		return;
	}

	if (!team) { // chat flood protection, does not pertain to teams

		if (g_level.time < cl->locals.chat_time) {
			return;
		}

		cl->locals.chat_time = g_level.time + 250;
	}

	const int32_t color = team ? CON_COLOR_TEAMCHAT : CON_COLOR_CHAT;
	g_snprintf(text, sizeof(text), "%s^%d: %s\n", cl->locals.persistent.net_name, color, s);

	for (i = 1; i <= g_max_clients->integer; i++) { // print to clients
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

	if (g_dedicated->value) { // print to the console
		gi.Print("%s", text);
	}
}

/**
 * @brief
 */
static void G_PlayerList_f(g_entity_t *ent) {
	int32_t i, seconds;
	char st[80];
	char text[1400];
	g_entity_t *e2;

	memset(text, 0, sizeof(text));

	// connect time, ping, score, name
	for (i = 0, e2 = g_game.entities + 1; i < g_max_clients->integer; i++, e2++) {

		if (!e2->in_use) {
			continue;
		}

		seconds = (g_level.frame_num - e2->client->locals.persistent.first_frame) / QUETOO_TICK_RATE;

		g_snprintf(st, sizeof(st), "%02d:%02d %4d %3d %-16s %s %s\n", (seconds / 60), (seconds % 60),
		           e2->client->ping, e2->client->locals.persistent.score,
		           e2->client->locals.persistent.net_name,
		           (e2->client->locals.persistent.admin) ? "(admin)" : "",
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

static const char *vote_cmds[] = {
	"g_capture_limit",
	"g_ctf",
	"g_techs",
	"g_frag_limit",
	"g_friendly_fire",
	"g_gameplay",
	"g_match",
	"g_round_limit",
	"g_rounds",
	"g_spawn_farthest",
	"g_weapon_stay",
	"g_teams",
	"g_num_teams",
	"g_time_limit",
	"g_hook",
	"g_hook_style",
	"g_hook_speed",
	"g_hook_pull_speed",
	"kick",
	"map",
	"mute",
	"next_map",
	"restart",
	"unmute",
	NULL
};

static GList *vote_cvars; // temp number so it doesn't have to be dynamically allocated

/**
 * @brief Create g_vote_allow_<vote> CVars
 */
void G_InitVote(void) {
	size_t i = 0;

	while (vote_cmds[i]) {
		const char *name = va("g_vote_allow_%s", vote_cmds[i]);
		const char *desc = va("Allows voting on %s", vote_cmds[i]);

		vote_cvars = g_list_append(vote_cvars, gi.AddCvar(name, "1", CVAR_SERVER_INFO, desc));

		i++;
	}
}

/**
 * @brief Deinitialize the vote CVar GList
 */
void G_ShutdownVote(void) {
	g_list_free(vote_cvars);
}

/**
 * @brief Returns if the g_vote_allow_<vote> CVar is enabled
 */
static _Bool G_VoteAllowed(const char *vote) {
	const char *name = va("g_vote_allow_%s", vote);

	const GList *list = vote_cvars;

	while (list) {
		const cvar_t *var = list->data;
		if ((!g_strcmp0(name, var->name)) && var->integer) {
			return true;
		}

		list = list->next;
	}

	return false;
}

/**
 * @brief Inspects the vote command and issues help if applicable. Returns
 * true if the command received help and may therefore be ignored, false
 * otherwise.
 */
static _Bool G_VoteHelp(g_entity_t *ent) {
	char msg[MAX_STRING_CHARS];
	size_t i;

	if (!g_level.vote_time) { // check for yes/no
		if (gi.Argc() == 1 && (!g_strcmp0(gi.Argv(0), "yes") || !g_strcmp0(gi.Argv(0), "no"))) {
			gi.ClientPrint(ent, PRINT_HIGH, "There is not a vote in progress\n"); // shorthand
			return true;
		}
		if (gi.Argc() == 2 && (!g_strcmp0(gi.Argv(1), "yes") || !g_strcmp0(gi.Argv(1), "no"))) {
			gi.ClientPrint(ent, PRINT_HIGH, "There is not a vote in progress\n"); // explicit
			return true;
		}
	}

	memset(msg, 0, sizeof(msg));

	i = 0;
	if (gi.Argc() == 1) { // no command specified, list them
		strcat(msg, "\nAvailable vote commands:\n\n");

		while (vote_cmds[i]) {
			if (G_VoteAllowed(vote_cmds[i])) {
				strcat(msg, "  ");
				strcat(msg, vote_cmds[i]);
				strcat(msg, "\n");
			}

			i++;
		}
		gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
		return true;
	}

	i = 0;
	while (vote_cmds[i]) { // verify that command is supported
		if (!g_strcmp0(gi.Argv(1), vote_cmds[i])) {
			break;
		}
		i++;
	}

	if (!vote_cmds[i]) { // inform client if it is not
		gi.ClientPrint(ent, PRINT_HIGH, "Voting on \"%s\" is not supported\n", gi.Argv(1));
		return true;
	}

	if (!G_VoteAllowed(vote_cmds[i])) {
		gi.ClientPrint(ent, PRINT_HIGH, "Voting on \"%s\" is disabled\n", gi.Argv(1));
		return true;
	}

	if (!g_strcmp0(gi.Argv(1), "restart") || !g_strcmp0(gi.Argv(1), "next_map")) {
		return false; // takes no args, this is okay
	}

	// command-specific help for some commands
	if (gi.Argc() == 2 && !g_strcmp0(gi.Argv(1), "map")) { // list available maps

		if (!g_map_list) { // no maps in maplist
			gi.ClientPrint(ent, PRINT_HIGH, "Map voting is not available\n");
			return true;
		}

		strcat(msg, "\nAvailable maps:\n\n");

		for (GList *list = g_map_list; list; list = list->next) {
			const g_map_list_map_t *elt = list->data;
			g_strlcat(msg, va("  ^2%s^7 %s\n", elt->name, elt->message), sizeof(msg));
		}

		gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
		return true;
	}

	if (gi.Argc() == 2 && !g_strcmp0(gi.Argv(1), "g_gameplay")) { // list gameplay modes
		gi.ClientPrint(ent, PRINT_HIGH, "\nAvailable gameplay modes:\n\n"
		               "  DEATHMATCH\n  INSTAGIB\n  ARENA\n  DUEL\n");
		return true;
	}

	if (gi.Argc() == 2 && !g_strcmp0(gi.Argv(1), "g_hook")) { // list hook modes
		gi.ClientPrint(ent, PRINT_HIGH, "\nAvailable hook modes:\n\n"
		               "  default\n  1\n  0\n");
		return true;
	}

	if (gi.Argc() == 2 && !g_strcmp0(gi.Argv(1), "g_hook_style")) { // list hook modes
		gi.ClientPrint(ent, PRINT_HIGH, "\nAvailable force hook styles:\n\n"
		               "  default\n  pull\n  swing\n");
		return true;
	}

	// matches are required for duel mode
	if (g_level.gameplay == GAME_DUEL && !g_strcmp0(gi.Argv(1), "g_match")) {
		gi.ClientPrint(ent, PRINT_HIGH,
		               "Match mode is required for DUEL gameplay, setting cannot be changed\n");
		return true;
	}

	// teams are required for duel mode
	if (g_level.gameplay == GAME_DUEL && !g_strcmp0(gi.Argv(1), "g_teams")) {
		gi.ClientPrint(ent, PRINT_HIGH,
		               "Teams are required for DUEL gameplay, setting cannot be changed\n");
		return true;
	}

	if (gi.Argc() == 2) { // general catch for invalid commands
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <command args>\n", gi.Argv(0));
		return true;
	}

	return false;
}

/**
 * @brief
 */
static void G_Vote_f(g_entity_t *ent) {
	char vote[64];

	if (!g_voting->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Voting is not allowed");
		return;
	}

	if (!g_strcmp0(gi.Argv(0), "yes") || !g_strcmp0(gi.Argv(0), "no")) { // allow shorthand voting
		g_strlcpy(vote, gi.Argv(0), sizeof(vote));
	} else { // or the explicit syntax
		g_strlcpy(vote, gi.Args(), sizeof(vote));
	}

	if (g_level.vote_time) { // check for vote from client
		if (ent->client->locals.persistent.vote) {
			gi.ClientPrint(ent, PRINT_HIGH, "You've already voted\n");
			return;
		}
		if (g_strcmp0(vote, "yes") == 0) {
			if (ent->client->locals.persistent.admin) {
				g_level.votes[VOTE_YES] = g_max_clients->integer; // admin vote wins immediately
				return;
			}
			ent->client->locals.persistent.vote = VOTE_YES;
		} else if (g_strcmp0(vote, "no") == 0) {
			if (ent->client->locals.persistent.admin) {
				gi.BroadcastPrint(PRINT_HIGH, "An admin voted 'no', vote failed.\n");
				G_ResetVote();
				return;
			}
			ent->client->locals.persistent.vote = VOTE_NO;
		} else { // only yes and no are valid during a vote
			gi.ClientPrint(ent, PRINT_HIGH, "A vote \"%s\" is already in progress\n",
			               g_level.vote_cmd);
			return;
		}

		g_level.votes[ent->client->locals.persistent.vote]++;
		gi.BroadcastPrint(PRINT_HIGH, "Voting results \"%s\":\n  %d Yes     %d No\n",
		                  g_level.vote_cmd, g_level.votes[VOTE_YES], g_level.votes[VOTE_NO]);
		return;
	}

	if (G_VoteHelp(ent)) { // vote command got help, ignore it
		return;
	}

	if (!g_strcmp0(gi.Argv(1), "map")) { // ensure map is in map list

		if (G_MapList_Find(NULL, gi.Argv(2)) == NULL) { // inform client if it is not
			gi.ClientPrint(ent, PRINT_HIGH, "Map \"%s\" is not available\n", gi.Argv(2));
			return;
		}
	}

	g_strlcpy(g_level.vote_cmd, vote, sizeof(g_level.vote_cmd));
	g_level.vote_time = g_level.time;

	if (!ent->client->locals.persistent.admin) {	// wait to cast vote if admin
		ent->client->locals.persistent.vote = VOTE_YES; // client has implicitly voted
		g_level.votes[VOTE_YES] = 1;
	}

	gi.SetConfigString(CS_VOTE, g_level.vote_cmd); // send to layout

	gi.BroadcastPrint(PRINT_HIGH, "%s has called a vote:\n"
	                  "  %s\n"
	                  "Type vote yes or vote no in the console\n",
	                  ent->client->locals.persistent.net_name, g_level.vote_cmd
	                 );
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
static void G_Teamname_f(g_entity_t *ent) {
	g_team_t *t;

	if (gi.Argc() != 2) {
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <name>\n", gi.Argv(0));
		return;
	}

	if (!ent->client->locals.persistent.team) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're not on a team\n");
		return;
	}

	t = ent->client->locals.persistent.team;

	if (g_level.time - t->name_time < TEAM_CHANGE_TIME) {
		return; // prevent change spamming
	}

	const char *s = gi.Argv(1);

	if (*s != '\0' &&
		strlen(s) < MAX_TEAM_NAME &&
		!strchr(s, '|')) { // something valid-ish was provided
		g_strlcpy(t->name, s, sizeof(t->name));
	} else {
		strcpy(t->name, G_TeamDefaults(t)->name);
	}

	t->name_time = g_level.time;

	G_SetTeamNames();

	gi.BroadcastPrint(PRINT_HIGH, "%s changed team_name to %s\n",
	                  ent->client->locals.persistent.net_name, t->name);
}

/**
 * @brief
 */
static void G_Teamskin_f(g_entity_t *ent) {
	int32_t i;
	g_client_t *cl;
	g_team_t *t;

	if (gi.Argc() != 2) {
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <skin>\n", gi.Argv(0));
		return;
	}

	if (!ent->client->locals.persistent.team) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're not on a team\n");
		return;
	}

	t = ent->client->locals.persistent.team;

	if (g_level.time - t->skin_time < TEAM_CHANGE_TIME) {
		return; // prevent change spamming
	}

	const char *s = gi.Argv(1);

	if (s != '\0' &&
		!strchr(s, '/')) { // something valid-ish was provided
		g_strlcpy(t->skin, s, sizeof(t->skin));
	} else {
		strcpy(t->skin, G_TeamDefaults(t)->skin);
	}

	s = t->skin;

	t->skin_time = g_level.time;

	for (i = 0; i < g_max_clients->integer; i++) { // update skins
		cl = g_game.clients + i;

		if (!cl->locals.persistent.team || cl->locals.persistent.team != t) {
			continue;
		}

		char *skin_start = strchr(cl->locals.persistent.skin, '/');
		*skin_start = 0;

		const char *new_model = va("%s/%s", cl->locals.persistent.skin, s);

		g_strlcpy(cl->locals.persistent.skin, new_model, sizeof(cl->locals.persistent.skin));

		gi.SetConfigString(CS_CLIENTS + i,
		                   va("%s\\%s", cl->locals.persistent.net_name, cl->locals.persistent.skin));
	}

	gi.BroadcastPrint(PRINT_HIGH, "%s changed team_skin to %s\n",
	                  ent->client->locals.persistent.net_name, t->skin);
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

	for (i = 0; i < g_max_clients->integer; i++) { // is everyone ready?
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

	if (i != (int32_t) g_max_clients->integer) { // someone isn't ready
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

	g_warmup_time->integer = Clamp(g_warmup_time->integer, 0, 30);

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
	_Bool spectator;

	// prevent spectator spamming
	if (g_level.time - ent->client->locals.respawn_time < 1000) {
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

	spectator = ent->client->locals.persistent.spectator;

	if (ent->client->locals.persistent.spectator) { // they wish to join
		if (g_level.teams || g_level.ctf) {
			if (g_auto_join->value) { // assign them to a team
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
			} else { // or ask them to pick
				gi.ClientPrint(ent, PRINT_HIGH, "Use team <team name> to join the game\n");
			}
		}
	} else { // they wish to spectate

		if (g_level.gameplay == GAME_DEATHMATCH || g_level.gameplay == GAME_DUEL) {
			G_TossQuadDamage(ent);
		}

		G_TossFlag(ent);
		G_TossTech(ent);
		G_ClientHookDetach(ent);
	}

	ent->client->locals.persistent.spectator = !spectator;
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
		gi.ClientPrint(ent, PRINT_HIGH, "Only players can pause the match...\n");
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

static void G_Admin_f(g_entity_t *ent) {

	if (strlen(g_admin_password->string) == 0) {	// blank password (default) disabled
		gi.ClientPrint(ent, PRINT_HIGH, "Admin features disabled\n");
		return;
	}

	if (gi.Argc() < 2) {	// no arguments supplied, show help
		if (!ent->client->locals.persistent.admin) {
			gi.ClientPrint(ent, PRINT_HIGH, "Usage: admin <password>\n");
		} else {
			gi.ClientPrint(ent, PRINT_HIGH, "Commands with admin overrides:\n");
			gi.ClientPrint(ent, PRINT_HIGH, "vote, time/timeout/timein, kick, remove, mute, timelimit \n");
		}
		return;
	}

	if (!ent->client->locals.persistent.admin) {	// not yet an admin, assuming auth
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
	if (g_strcmp0(cmd, "vote") == 0) { // maybe
		G_Vote_f(ent);
		return;

	} else if (g_strcmp0(cmd, "yes") == 0 || g_strcmp0(cmd, "no") == 0) {
		G_Vote_f(ent);
		return;

	} else if (g_strcmp0(cmd, "timeout") == 0 || g_strcmp0(cmd, "timein") == 0 || g_strcmp0(cmd, "time") == 0) {
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
	if (g_strcmp0(cmd, "spectate") == 0) {
		G_Spectate_f(ent);
	} else if (g_strcmp0(cmd, "team") == 0 || g_strcmp0(cmd, "join") == 0) {
		G_Team_f(ent);
	} else if (g_strcmp0(cmd, "team_name") == 0) {
		G_Teamname_f(ent);
	} else if (g_strcmp0(cmd, "team_skin") == 0) {
		G_Teamskin_f(ent);
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

	else
		// anything that doesn't match a command will be a chat
	{
		G_Say_f(ent);
	}
}
