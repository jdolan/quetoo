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

#include "g_local.h"

/*
 * G_Give_f
 *
 * Give items to a client
 */
static void G_Give_f(g_edict_t *ent) {
	char *name;
	g_item_t *it;
	int index;
	int i;
	boolean_t give_all;
	g_edict_t *it_ent;

	if (sv_max_clients->integer > 1 && !g_cheats->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Cheats are disabled.\n");
		return;
	}

	name = gi.Args();

	if (strcasecmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || strcasecmp(gi.Argv(1), "health") == 0) {
		if (gi.Argc() == 3)
			ent->health = atoi(gi.Argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || strcasecmp(name, "weapons") == 0) {
		for (i = 0; i < g_game.num_items; i++) {
			it = g_items + i;
			if (!it->pickup)
				continue;
			if (it->type != ITEM_WEAPON)
				continue;
			ent->client->persistent.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || strcasecmp(name, "ammo") == 0) {
		for (i = 0; i < g_game.num_items; i++) {
			it = g_items + i;
			if (!it->pickup)
				continue;
			if (it->type != ITEM_AMMO)
				continue;
			G_AddAmmo(ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || strcasecmp(name, "armor") == 0) {
		if (gi.Argc() == 3)
			ent->client->persistent.armor = atoi(gi.Argv(2));
		else
			ent->client->persistent.armor = ent->client->persistent.max_armor;

		if (!give_all)
			return;
	}

	if (give_all) // we've given full health and inventory
		return;

	it = G_FindItem(name);
	if (!it) {
		name = gi.Argv(1);
		it = G_FindItem(name);
		if (!it) {
			gi.ClientPrint(ent, PRINT_HIGH, "Unknown item: %s\n", name);
			return;
		}
	}

	if (!it->pickup) {
		gi.ClientPrint(ent, PRINT_HIGH, "Non-pickup item: %s\n", name);
		return;
	}

	if (it->type == ITEM_AMMO) { // give the requested ammo quantity
		index = ITEM_INDEX(it);

		if (gi.Argc() == 3)
			ent->client->persistent.inventory[index] = atoi(gi.Argv(2));
		else
			ent->client->persistent.inventory[index] += it->quantity;
	} else { // or spawn and touch whatever they asked for
		it_ent = G_Spawn();
		it_ent->class_name = it->class_name;

		G_SpawnItem(it_ent, it);
		G_TouchItem(it_ent, ent, NULL, NULL);

		if (it_ent->in_use)
			G_FreeEdict(it_ent);
	}
}

/*
 * G_God_f
 */
static void G_God_f(g_edict_t *ent) {
	char *msg;

	if (sv_max_clients->integer > 1 && !g_cheats->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Cheats are disabled.\n");
		return;
	}

	ent->flags ^= FL_GOD_MODE;
	if (!(ent->flags & FL_GOD_MODE))
		msg = "god OFF\n";
	else
		msg = "god ON\n";

	gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
}

/*
 * G_NoClip_f
 */
static void G_NoClip_f(g_edict_t *ent) {
	char *msg;

	if (sv_max_clients->integer > 1 && !g_cheats->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Cheats are disabled.\n");
		return;
	}

	if (ent->move_type == MOVE_TYPE_NO_CLIP) {
		ent->move_type = MOVE_TYPE_WALK;
		msg = "no_clip OFF\n";
	} else {
		ent->move_type = MOVE_TYPE_NO_CLIP;
		msg = "no_clip ON\n";
	}

	gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
}

/*
 * G_Wave_f
 */
static void G_Wave_f(g_edict_t *ent) {

	if (ent->sv_flags & SVF_NO_CLIENT)
		return;

	G_SetAnimation(ent, ANIM_TORSO_GESTURE, true);
}

/*
 * G_Use_f
 */
static void G_Use_f(g_edict_t *ent) {
	int index;
	g_item_t *it;
	char *s;

	s = gi.Args();
	it = G_FindItem(s);
	if (!it) {
		gi.ClientPrint(ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}
	if (!it->use) {
		gi.ClientPrint(ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->persistent.inventory[index]) {
		gi.ClientPrint(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use(ent, it);
}

/*
 * G_Drop_f
 */
static void G_Drop_f(g_edict_t *ent) {
	int index;
	g_edict_t *f;
	g_item_t *it;
	char *s;

	// we dont drop in instagib or arena
	if (g_level.gameplay)
		return;

	s = gi.Args();
	it = NULL;

	if (!strcmp(s, "flag")) { // find the correct flag

		f = G_FlagForTeam(G_OtherTeam(ent->client->persistent.team));
		if (f)
			it = f->item;
	} else
		// or just look up the item
		it = G_FindItem(s);

	if (!it) {
		gi.ClientPrint(ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}

	if (!it->drop) {
		gi.ClientPrint(ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (!ent->client->persistent.inventory[index]) {
		gi.ClientPrint(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop(ent, it);
}

/*
 * G_WeaponPrevious_f
 */
static void G_WeaponPrevious_f(g_edict_t *ent) {
	g_client_t *cl;
	int i, index;
	g_item_t *it;
	int selected_weapon;

	cl = ent->client;

	if (cl->persistent.spectator) {

		if (cl->chase_target) // chase the previous player
			G_ChasePrevious(ent);

		return;
	}

	if (!cl->persistent.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->persistent.weapon);

	// scan  for the next valid one
	for (i = 1; i <= MAX_ITEMS; i++) {
		index = (selected_weapon + i) % MAX_ITEMS;
		if (!cl->persistent.inventory[index])
			continue;
		it = &g_items[index];
		if (!it->use)
			continue;
		if (it->type != ITEM_WEAPON)
			continue;
		it->use(ent, it);
		if (cl->persistent.weapon == it)
			return; // successful
	}
}

/*
 * G_WeaponNext_f
 */
static void G_WeaponNext_f(g_edict_t *ent) {
	g_client_t *cl;
	int i, index;
	g_item_t *it;
	int selected_weapon;

	cl = ent->client;

	if (cl->persistent.spectator) {

		if (cl->chase_target) // chase the next player
			G_ChaseNext(ent);

		return;
	}

	if (!cl->persistent.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->persistent.weapon);

	// scan  for the next valid one
	for (i = 1; i <= MAX_ITEMS; i++) {
		index = (selected_weapon + MAX_ITEMS - i) % MAX_ITEMS;
		if (!cl->persistent.inventory[index])
			continue;
		it = &g_items[index];
		if (!it->use)
			continue;
		if (it->type != ITEM_WEAPON)
			continue;
		it->use(ent, it);
		if (cl->persistent.weapon == it)
			return; // successful
	}
}

/*
 * G_WeaponLast_f
 */
static void G_WeaponLast_f(g_edict_t *ent) {
	g_client_t *cl;
	int index;
	g_item_t *it;

	cl = ent->client;

	if (!cl->persistent.weapon || !cl->persistent.last_weapon)
		return;

	index = ITEM_INDEX(cl->persistent.last_weapon);
	if (!cl->persistent.inventory[index])
		return;
	it = &g_items[index];
	if (!it->use)
		return;
	if (it->type != ITEM_WEAPON)
		return;
	it->use(ent, it);
}

/*
 * G_Kill_f
 */
static void G_Kill_f(g_edict_t *ent) {

	if ((g_level.time - ent->client->respawn_time) < 1.0)
		return;

	if (ent->client->persistent.spectator)
		return;

	if (ent->dead)
		return;

	ent->flags &= ~FL_GOD_MODE;
	ent->health = 0;

	means_of_death = MOD_SUICIDE;

	ent->die(ent, ent, ent, 100000, vec3_origin);
}

/*
 * G_Say_f
 */
static void G_Say_f(g_edict_t *ent) {
	int i;
	size_t len;
	boolean_t team, arg0;
	char *c, text[256];
	g_edict_t *other;
	g_client_t *cl;

	if (ent->client->muted) {
		gi.ClientPrint(ent, PRINT_HIGH,
				"You have been muted.  You're probably an asshole.\n");
		return;
	}

	memset(text, 0, sizeof(text));

	c = gi.Argv(0);
	team = false;
	arg0 = true;

	if (!strncasecmp(c, "say", 3)) {

		if (gi.Argc() == 1)
			return;

		if (!strcasecmp(c, "say_team") && (g_level.teams || g_level.ctf))
			team = true;

		arg0 = false;
	}

	if (team)
		snprintf(text, sizeof(text), "%s^%d: ", ent->client->persistent.net_name, CON_COLOR_TEAMCHAT);
	else
		snprintf(text, sizeof(text), "%s^%d: ", ent->client->persistent.net_name, CON_COLOR_CHAT);
	len = strlen(text);

	i = sizeof(text) - strlen(text) - 2;
	c = gi.Args();

	if (arg0) { // not say or say_team, just arbitrary chat from the console
		strncat(text, gi.Argv(0), i);
		strcat(text, " ");
		strncat(text, c, i);
	} else { // say or say_team
		if (c[0] == '"') { // strip quotes if necessary
			strncat(text, c + 1, i);
			text[strlen(text) - 1] = 0;
		} else {
			strncat(text, c, i);
		}
	}
	strcat(text, "\n");

	// suppress empty messages
	arg0 = true;
	c = text + len;
	while (*c && arg0) {
		if (IS_COLOR(c))
			c++;
		else if (!IS_LEGACY_COLOR(c) && *c != '\n' && *c != ' ')
			arg0 = false;
		c++;
	}
	if (arg0)
		return;

	if (!team) { // chat flood protection, does not pertain to teams
		cl = ent->client;

		if (g_level.time < cl->chat_time)
			return;

		cl->chat_time = g_level.time + 1;
	}

	for (i = 1; i <= sv_max_clients->integer; i++) { // print to clients
		other = &g_game.edicts[i];

		if (!other->in_use)
			continue;

		if (team) {
			if (!G_OnSameTeam(ent, other))
				continue;
			gi.ClientPrint(other, PRINT_TEAMCHAT, "%s", text);
		} else {
			gi.ClientPrint(other, PRINT_CHAT, "%s", text);
		}
	}

	if (dedicated->value) { // print to the console
		if (team)
			gi.Print("%s", text);
		else
			gi.Print("%s", text);
	}

	if (chat_log != NULL) // print to chat_log
		fprintf(chat_log, "%s", text);
}

/*
 * G_PlayerList_f
 */
static void G_PlayerList_f(g_edict_t *ent) {
	int i, seconds;
	char st[80];
	char text[1400];
	g_edict_t *e2;

	memset(text, 0, sizeof(text));

	// connect time, ping, score, name
	for (i = 0, e2 = g_game.edicts + 1; i < sv_max_clients->integer; i++, e2++) {

		if (!e2->in_use)
			continue;

		seconds = (g_level.frame_num - e2->client->persistent.first_frame)
				/ gi.frame_rate;

		snprintf(st, sizeof(st), "%02d:%02d %4d %3d %-16s %s\n",
				(seconds / 60),
				(seconds % 60),
				e2->client->ping,
				e2->client->persistent.score,
				e2->client->persistent.net_name,
				e2->client->persistent.skin);

		if (strlen(text) + strlen(st) > sizeof(text) - 200) {
			sprintf(text + strlen(text), "And more...\n");
			gi.ClientPrint(ent, PRINT_HIGH, "%s", text);
			return;
		}

		strcat(text, st);
	}
	gi.ClientPrint(ent, PRINT_HIGH, "%s", text);
}

static const char *vote_cmds[] = { "g_capture_limit", "g_ctf", "g_frag_limit",
		"g_friendly_fire", "g_gameplay", "g_match", "g_round_limit",
		"g_rounds", "g_spawn_farthest", "g_teams", "g_time_limit", "kick",
		"map", "mute", "restart", "unmute", NULL };

/*
 * Vote_Help
 *
 * Inspects the vote command and issues help if applicable.  Returns
 * true if the command received help and may therefore be ignored, false
 * otherwise.
 */
static boolean_t Vote_Help(g_edict_t *ent) {
	size_t i, j, len;
	char msg[1024];

	if (!g_level.vote_time) { // check for yes/no
		if (gi.Argc() == 1 && (!strcasecmp(gi.Argv(0), "yes") || !strcasecmp(
				gi.Argv(0), "no"))) {
			gi.ClientPrint(ent, PRINT_HIGH, "There is not a vote in progress\n"); // shorthand
			return true;
		}
		if (gi.Argc() == 2 && (!strcasecmp(gi.Argv(1), "yes") || !strcasecmp(
				gi.Argv(1), "no"))) {
			gi.ClientPrint(ent, PRINT_HIGH, "There is not a vote in progress\n"); // explicit
			return true;
		}
	}

	memset(msg, 0, sizeof(msg));

	i = 0;
	if (gi.Argc() == 1) { // no command specified, list them
		strcat(msg, "\nAvailable vote commands:\n\n");

		while (vote_cmds[i]) {
			strcat(msg, "  ");
			strcat(msg, vote_cmds[i]);
			strcat(msg, "\n");
			i++;
		}
		gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
		return true;
	}

	i = 0;
	while (vote_cmds[i]) { // verify that command is supported
		if (!strcasecmp(gi.Argv(1), vote_cmds[i]))
			break;
		i++;
	}

	if (!vote_cmds[i]) { // inform client if it is not
		gi.ClientPrint(ent, PRINT_HIGH, "Voting on \"%s\" is not supported\n",
				gi.Argv(1));
		return true;
	}

	if (!strcasecmp(gi.Argv(1), "restart"))
		return false; // takes no args, this is okay

	// command-specific help for some commands
	if (gi.Argc() == 2 && !strcasecmp(gi.Argv(1), "map")) { // list available maps

		if (!g_map_list.count) { // no maps in maplist
			gi.ClientPrint(ent, PRINT_HIGH, "Map voting is not available\n");
			return true;
		}

		strcat(msg, "\nAvailable maps:\n\n");

		j = 0;
		for (i = 0; i < g_map_list.count; i++) {
			len = strlen(g_map_list.maps[i].name) + 3;
			len += strlen(g_map_list.maps[i].title) + 2;

			if (j + len > sizeof(msg)) // don't overrun msg
				break;

			strcat(msg, "  ");
			strcat(msg, g_map_list.maps[i].name);
			strcat(msg, " ");
			strcat(msg, g_map_list.maps[i].title);
			strcat(msg, "\n");
			j += len;
		}

		gi.ClientPrint(ent, PRINT_HIGH, "%s", msg);
		return true;
	}

	if (gi.Argc() == 2 && !strcasecmp(gi.Argv(1), "g_gameplay")) { // list gameplay modes
		gi.ClientPrint(ent, PRINT_HIGH, "\nAvailable gameplay modes:\n\n"
			"  DEATHMATCH\n  INSTAGIB\n  ARENA\n");
		return true;
	}

	if (gi.Argc() == 2) { // general catch for invalid commands
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <command args>\n",
				gi.Argv(0));
		return true;
	}

	return false;
}

/*
 * G_Vote_f
 */
static void G_Vote_f(g_edict_t *ent) {
	char *c, vote[64];
	unsigned int i;

	if (!g_voting->value) {
		gi.ClientPrint(ent, PRINT_HIGH, "Voting is not allowed");
		return;
	}

	c = gi.Argv(0);
	if (!strcasecmp(c, "yes") || !strcasecmp(c, "no"))
		strcpy(vote, c); // allow shorthand voting
	else { // or the explicit syntax
		strncpy(vote, gi.Args(), sizeof(vote) - 1);
		vote[sizeof(vote) - 1] = 0;
	}

	if (g_level.vote_time) { // check for vote from client
		if (ent->client->persistent.vote) {
			gi.ClientPrint(ent, PRINT_HIGH, "You've already voted\n");
			return;
		}
		if (strcasecmp(vote, "yes") == 0)
			ent->client->persistent.vote = VOTE_YES;
		else if (strcasecmp(vote, "no") == 0)
			ent->client->persistent.vote = VOTE_NO;
		else { // only yes and no are valid during a vote
			gi.ClientPrint(ent, PRINT_HIGH,
					"A vote \"%s\" is already in progress\n", g_level.vote_cmd);
			return;
		}

		g_level.votes[ent->client->persistent.vote]++;
		gi.BroadcastPrint(PRINT_HIGH,
				"Voting results \"%s\":\n  %d Yes     %d No\n",
				g_level.vote_cmd, g_level.votes[VOTE_YES],
				g_level.votes[VOTE_NO]);
		return;
	}

	if (Vote_Help(ent)) // vote command got help, ignore it
		return;

	if (!strcasecmp(gi.Argv(1), "map")) { // ensure map is in maplist
		for (i = 0; i < g_map_list.count; i++) {
			if (!strcasecmp(gi.Argv(2), g_map_list.maps[i].name))
				break; // found it
		}

		if (i == g_map_list.count) { // inform client if it is not
			gi.ClientPrint(ent, PRINT_HIGH, "Map \"%s\" is not available\n",
					gi.Argv(2));
			return;
		}
	}

	strncpy(g_level.vote_cmd, vote, sizeof(g_level.vote_cmd) - 1);
	g_level.vote_cmd[sizeof(g_level.vote_cmd) - 1] = 0;
	g_level.vote_time = g_level.time;

	ent->client->persistent.vote = VOTE_YES; // client has implicity voted
	g_level.votes[VOTE_YES] = 1;

	gi.ConfigString(CS_VOTE, g_level.vote_cmd); // send to layout

	gi.BroadcastPrint(PRINT_HIGH, "%s has called a vote:\n"
		"  %s\n"
		"To vote, press F1 for yes or F2 for no\n",
			ent->client->persistent.net_name, g_level.vote_cmd);
}

/*
 * G_AddClientToTeam
 *
 * Returns true if the client's team was changed, false otherwise.
 */
boolean_t G_AddClientToTeam(g_edict_t *ent, char *team_name) {
	g_team_t *team;

	if (g_level.match_time && g_level.match_time <= g_level.time) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match has already started\n");
		return false;
	}

	if (!(team = G_TeamByName(team_name))) { // resolve team
		gi.ClientPrint(ent, PRINT_HIGH, "Team \"%s\" doesn't exist\n",
				team_name);
		return false;
	}

	if (ent->client->persistent.team == team)
		return false;

	if (!ent->client->persistent.spectator) { // changing teams
		G_TossQuadDamage(ent);
		G_TossFlag(ent);
	}

	ent->client->persistent.team = team;
	ent->client->persistent.spectator = false;
	ent->client->persistent.ready = false;

	G_ClientUserInfoChanged(ent, ent->client->persistent.user_info);
	return true;
}

/*
 * G_AddClientToRound
 */
static void G_AddClientToRound(g_edict_t *ent) {
	int score; // keep score across rounds

	if (g_level.round_time && g_level.round_time <= g_level.time) {
		gi.ClientPrint(ent, PRINT_HIGH, "Round has already started\n");
		return;
	}

	score = ent->client->persistent.score;

	if (g_level.teams) { // attempt to add client to team
		if (!G_AddClientToTeam(ent, gi.Argv(1)))
			return;
	} else { // simply allow them to join
		if (!ent->client->persistent.spectator)
			return;
		ent->client->persistent.spectator = false;
	}

	G_ClientRespawn(ent, true);
	ent->client->persistent.score = score; // lastly restore score
}

/*
 * G_Team_f
 */
static void G_Team_f(g_edict_t *ent) {

	if ((g_level.teams || g_level.ctf) && gi.Argc() != 2) {
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <%s|%s>\n", gi.Argv(0),
				g_team_good.name, g_team_evil.name);
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

	if (!G_AddClientToTeam(ent, gi.Argv(1)))
		return;

	G_ClientRespawn(ent, true);
}

/*
 * G_Teamname_f
 */
static void G_Teamname_f(g_edict_t *ent) {
	int cs;
	char *s;
	g_team_t *t;

	if (gi.Argc() != 2) {
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <name>\n", gi.Argv(0));
		return;
	}

	if (!ent->client->persistent.team) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're not on a team\n");
		return;
	}

	t = ent->client->persistent.team;

	if (g_level.time - t->name_time < TEAM_CHANGE_TIME)
		return; // prevent change spamming

	s = gi.Argv(1);

	if (*s != '\0') // something valid-ish was provided
		strncpy(t->name, s, sizeof(t->name) - 1);
	else
		strcpy(t->name, (t == &g_team_good ? "Good" : "Evil"));

	s = t->name;
	s[sizeof(t->name) - 1] = 0;

	t->name_time = g_level.time;

	cs = t == &g_team_good ? CS_TEAM_GOOD : CS_TEAM_EVIL;
	gi.ConfigString(cs, t->name);

	gi.BroadcastPrint(PRINT_HIGH, "%s changed team_name to %s\n",
			ent->client->persistent.net_name, t->name);
}

/*
 * G_Teamskin_f
 */
static void G_Teamskin_f(g_edict_t *ent) {
	int i;
	g_client_t *cl;
	char *c, *s;
	g_team_t *t;

	if (gi.Argc() != 2) {
		gi.ClientPrint(ent, PRINT_HIGH, "Usage: %s <skin>\n", gi.Argv(0));
		return;
	}

	if (!ent->client->persistent.team) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're not on a team\n");
		return;
	}

	t = ent->client->persistent.team;

	if (g_level.time - t->skin_time < TEAM_CHANGE_TIME)
		return; // prevent change spamming

	s = gi.Argv(1);

	if (s != '\0') // something valid-ish was provided
		strncpy(t->skin, s, sizeof(t->skin) - 1);
	else
		strcpy(t->skin, "qforcer");

	s = t->skin;
	s[sizeof(t->skin) - 1] = 0;

	c = strchr(s, '/');

	// let players use just the model name, client will find skin
	if (!c || *c == '\0') {
		if (c) // null terminate for strcat
			*c = 0;

		strncat(t->skin, "/default", sizeof(t->skin) - 1 - strlen(s));
	}

	t->skin_time = g_level.time;

	for (i = 0; i < sv_max_clients->integer; i++) { // update skins
		cl = g_game.clients + i;

		if (!cl->persistent.team || cl->persistent.team != t)
			continue;

		strncpy(cl->persistent.skin, s, sizeof(cl->persistent.skin) - 1);
		cl->persistent.skin[sizeof(cl->persistent.skin) - 1] = 0;

		gi.ConfigString(CS_CLIENT_INFO + i,
				va("%s\\%s", cl->persistent.net_name, cl->persistent.skin));
	}

	gi.BroadcastPrint(PRINT_HIGH, "%s changed team_skin to %s\n",
			ent->client->persistent.net_name, t->skin);
}

/*
 * G_Ready_f
 *
 * If match is enabled, all clients must issue ready for game to start.
 */
static void G_Ready_f(g_edict_t *ent) {
	int i, g, e, clients;
	g_client_t *cl;

	if (!g_level.match) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match is disabled\n");
		return;
	}

	if (ent->client->persistent.spectator) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're a spectator\n");
		return;
	}

	if (ent->client->persistent.ready) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're already ready\n");
		return;
	}

	ent->client->persistent.ready = true;

	clients = g = e = 0;

	for (i = 0; i < sv_max_clients->integer; i++) { // is everyone ready?
		cl = g_game.clients + i;

		if (!g_game.edicts[i + 1].in_use)
			continue;

		if (cl->persistent.spectator)
			continue;

		if (!cl->persistent.ready)
			break;

		clients++;

		if (g_level.teams || g_level.ctf)
			cl->persistent.team == &g_team_good ? g++ : e++;
	}

	if (i != (int) sv_max_clients->integer) // someone isn't ready
		return;

	if (clients < 2) // need at least 2 clients to trigger match
		return;

	if ((g_level.teams || g_level.ctf) && (!g || !e)) // need at least 1 player per team
		return;

	if (((int) g_level.teams == 2 || (int) g_level.ctf == 2) && (g != e)) { // balanced teams required
		gi.BroadcastPrint(PRINT_HIGH,
				"Teams must be balanced for match to start\n");
		return;
	}

	gi.BroadcastPrint(PRINT_HIGH, "Match starting in 10 seconds...\n");
	g_level.match_time = g_level.time + 10.0;

	g_level.start_match = true;
}

/*
 * G_Unready_f
 */
static void G_Unready_f(g_edict_t *ent) {

	if (!g_level.match) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match is disabled\n");
		return;
	}

	if (ent->client->persistent.spectator) {
		gi.ClientPrint(ent, PRINT_HIGH, "You're a spectator\n");
		return;
	}

	if (g_level.match_time) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match has started\n");
		return;
	}

	if (!ent->client->persistent.ready) {
		gi.ClientPrint(ent, PRINT_HIGH, "You are not ready\n");
		return;
	}

	ent->client->persistent.ready = false;
	g_level.start_match = false;
}

/*
 * G_Spectate_f
 */
static void G_Spectate_f(g_edict_t *ent) {
	boolean_t spectator;

	// prevent spectator spamming
	if (g_level.time - ent->client->respawn_time < 3.0)
		return;

	// prevent spectators from joining matches
	if (g_level.match_time && ent->client->persistent.spectator) {
		gi.ClientPrint(ent, PRINT_HIGH, "Match has already started\n");
		return;
	}

	// prevent spectators from joining rounds
	if (g_level.round_time && ent->client->persistent.spectator) {
		gi.ClientPrint(ent, PRINT_HIGH, "Round has already started\n");
		return;
	}

	spectator = ent->client->persistent.spectator;

	if (ent->client->persistent.spectator) { // they wish to join
		if (g_level.teams || g_level.ctf) {
			if (g_auto_join->value) // assign them to a team
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
			else { // or ask them to pick
				gi.ClientPrint(ent, PRINT_HIGH,
						"Use team <%s|%s> to join the game\n", g_team_good.name,
						g_team_evil.name);
				return;
			}
		}
	} else { // they wish to spectate
		G_TossQuadDamage(ent);
		G_TossFlag(ent);
	}

	ent->client->persistent.spectator = !spectator;
	G_ClientRespawn(ent, true);
}

/*
 * G_Score_f
 */
void G_Score_f(g_edict_t *ent) {
	ent->client->show_scores = !ent->client->show_scores;
}

/*
 * G_Command
 */
void G_ClientCommand(g_edict_t *ent) {
	char *cmd;

	if (!ent->client)
		return; // not fully in game yet

	cmd = gi.Argv(0);

	if (strcasecmp(cmd, "say") == 0) {
		G_Say_f(ent);
		return;
	}
	if (strcasecmp(cmd, "say_team") == 0) {
		G_Say_f(ent);
		return;
	}

	// most commands can not be executed during intermission
	if (g_level.intermission_time)
		return;

	if (strcasecmp(cmd, "score") == 0)
		G_Score_f(ent);
	else if (strcasecmp(cmd, "spectate") == 0)
		G_Spectate_f(ent);
	else if (strcasecmp(cmd, "team") == 0 || strcasecmp(cmd, "join") == 0)
		G_Team_f(ent);
	else if (strcasecmp(cmd, "team_name") == 0)
		G_Teamname_f(ent);
	else if (strcasecmp(cmd, "team_skin") == 0)
		G_Teamskin_f(ent);
	else if (strcasecmp(cmd, "ready") == 0)
		G_Ready_f(ent);
	else if (strcasecmp(cmd, "unready") == 0)
		G_Unready_f(ent);
	else if (strcasecmp(cmd, "use") == 0)
		G_Use_f(ent);
	else if (strcasecmp(cmd, "drop") == 0)
		G_Drop_f(ent);
	else if (strcasecmp(cmd, "give") == 0)
		G_Give_f(ent);
	else if (strcasecmp(cmd, "god") == 0)
		G_God_f(ent);
	else if (strcasecmp(cmd, "no_clip") == 0)
		G_NoClip_f(ent);
	else if (strcasecmp(cmd, "wave") == 0)
		G_Wave_f(ent);
	else if (strcasecmp(cmd, "weapon_previous") == 0)
		G_WeaponPrevious_f(ent);
	else if (strcasecmp(cmd, "weapon_next") == 0)
		G_WeaponNext_f(ent);
	else if (strcasecmp(cmd, "weapon_last") == 0)
		G_WeaponLast_f(ent);
	else if (strcasecmp(cmd, "kill") == 0)
		G_Kill_f(ent);
	else if (strcasecmp(cmd, "player_list") == 0)
		G_PlayerList_f(ent);
	else if (strcasecmp(cmd, "vote") == 0 || strcasecmp(cmd, "yes") == 0
			|| strcasecmp(cmd, "no") == 0)
		G_Vote_f(ent);
	else
		// anything that doesn't match a command will be a chat
		G_Say_f(ent);
}
