/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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
#include "m_player.h"


/*
 * G_Give_f
 *
 * Give items to a client
 */
static void G_Give_f(edict_t *ent){
	char *name;
	gitem_t *it;
	int index;
	int i;
	qboolean give_all;
	edict_t *it_ent;

	if(sv_maxclients->value > 1 && !g_cheats->value){
		gi.Cprintf(ent, PRINT_HIGH, "Cheats are disabled.\n");
		return;
	}

	name = gi.Args();

	if(strcasecmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if(give_all || strcasecmp(gi.Argv(1), "health") == 0){
		if(gi.Argc() == 3)
			ent->health = atoi(gi.Argv(2));
		else
			ent->health = ent->max_health;
		if(!give_all)
			return;
	}

	if(give_all || strcasecmp(name, "weapons") == 0){
		for(i = 0; i < game.num_items; i++){
			it = itemlist + i;
			if(!it->pickup)
				continue;
			if(!(it->flags & IT_WEAPON))
				continue;
			ent->client->locals.inventory[i] += 1;
		}
		if(!give_all)
			return;
	}

	if(give_all || strcasecmp(name, "ammo") == 0){
		for(i = 0; i < game.num_items; i++){
			it = itemlist + i;
			if(!it->pickup)
				continue;
			if(!(it->flags & IT_AMMO))
				continue;
			G_AddAmmo(ent, it, 1000);
		}
		if(!give_all)
			return;
	}

	if(give_all || strcasecmp(name, "armor") == 0){
		if(gi.Argc() == 3)
			ent->client->locals.armor = atoi(gi.Argv(2));
		else
			ent->client->locals.armor = ent->client->locals.max_armor;

		if(!give_all)
			return;
	}

	if(give_all)  // we've given full health and inventory
		return;

	it = G_FindItem(name);
	if(!it){
		name = gi.Argv(1);
		it = G_FindItem(name);
		if(!it){
			gi.Cprintf(ent, PRINT_HIGH, "Unknown item: %s\n", name);
			return;
		}
	}

	if(!it->pickup){
		gi.Cprintf(ent, PRINT_HIGH, "Non-pickup item: %s\n", name);
		return;
	}

	if(it->flags & IT_AMMO){  // give the requested ammo quantity
		index = ITEM_INDEX(it);

		if(gi.Argc() == 3)
			ent->client->locals.inventory[index] = atoi(gi.Argv(2));
		else
			ent->client->locals.inventory[index] += it->quantity;
	}
	else {  // or spawn and touch whatever they asked for
		it_ent = G_Spawn();
		it_ent->classname = it->classname;

		G_SpawnItem(it_ent, it);
		G_TouchItem(it_ent, ent, NULL, NULL);

		if(it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}


/*
 * G_God_f
 */
static void G_God_f(edict_t *ent){
	char *msg;

	if(sv_maxclients->value > 1 && !g_cheats->value){
		gi.Cprintf(ent, PRINT_HIGH, "Cheats are disabled.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if(!(ent->flags & FL_GODMODE))
		msg = "god OFF\n";
	else
		msg = "god ON\n";

	gi.Cprintf(ent, PRINT_HIGH, "%s", msg);
}


/*
 * G_Noclip_f
 */
static void G_Noclip_f(edict_t *ent){
	char *msg;

	if(sv_maxclients->value > 1 && !g_cheats->value){
		gi.Cprintf(ent, PRINT_HIGH, "Cheats are disabled.\n");
		return;
	}

	if(ent->movetype == MOVETYPE_NOCLIP){
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	} else {
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.Cprintf(ent, PRINT_HIGH, "%s", msg);
}


/*
 * G_Use_f
 */
static void G_Use_f(edict_t *ent){
	int index;
	gitem_t *it;
	char *s;

	s = gi.Args();
	it = G_FindItem(s);
	if(!it){
		gi.Cprintf(ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}
	if(!it->use){
		gi.Cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if(!ent->client->locals.inventory[index]){
		gi.Cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use(ent, it);
}


/*
 * G_Drop_f
 */
static void G_Drop_f(edict_t *ent){
	int index;
	gitem_t *it;
	char *s;

	// we dont drop in instagib or arena
	if(level.gameplay)
		return;

	s = gi.Args();
	it = G_FindItem(s);
	if(!it){
		gi.Cprintf(ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}
	if(!it->drop){
		gi.Cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if(!ent->client->locals.inventory[index]){
		gi.Cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop(ent, it);
}


/*
 * G_WeapPrev_f
 */
static void G_WeapPrev_f(edict_t *ent){
	gclient_t *cl;
	int i, index;
	gitem_t *it;
	int selected_weapon;

	cl = ent->client;

	if(cl->locals.spectator){

		if(cl->chase_target)  // chase the previous player
			P_ChasePrev(ent);

		return;
	}

	if(!cl->locals.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->locals.weapon);

	// scan  for the next valid one
	for(i = 1; i <= MAX_ITEMS; i++){
		index =(selected_weapon + i) % MAX_ITEMS;
		if(!cl->locals.inventory[index])
			continue;
		it = &itemlist[index];
		if(!it->use)
			continue;
		if(!(it->flags & IT_WEAPON))
			continue;
		it->use(ent, it);
		if(cl->locals.weapon == it)
			return;  // successful
	}
}

/*
 * G_WeapNext_f
 */
static void G_WeapNext_f(edict_t *ent){
	gclient_t *cl;
	int i, index;
	gitem_t *it;
	int selected_weapon;

	cl = ent->client;

	if(cl->locals.spectator){

		if(cl->chase_target)  // chase the next player
			P_ChaseNext(ent);

		return;
	}

	if(!cl->locals.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->locals.weapon);

	// scan  for the next valid one
	for(i = 1; i <= MAX_ITEMS; i++){
		index =(selected_weapon + MAX_ITEMS - i) % MAX_ITEMS;
		if(!cl->locals.inventory[index])
			continue;
		it = &itemlist[index];
		if(!it->use)
			continue;
		if(!(it->flags & IT_WEAPON))
			continue;
		it->use(ent, it);
		if(cl->locals.weapon == it)
			return;  // successful
	}
}


/*
 * G_WeapLast_f
 */
static void G_WeapLast_f(edict_t *ent){
	gclient_t *cl;
	int index;
	gitem_t *it;

	cl = ent->client;

	if(!cl->locals.weapon || !cl->locals.lastweapon)
		return;

	index = ITEM_INDEX(cl->locals.lastweapon);
	if(!cl->locals.inventory[index])
		return;
	it = &itemlist[index];
	if(!it->use)
		return;
	if(!(it->flags & IT_WEAPON))
		return;
	it->use(ent, it);
}


/*
 * G_Kill_f
 */
static void G_Kill_f(edict_t *ent){

	if((level.time - ent->client->respawn_time) < 1)
		return;

	if(ent->client->locals.spectator)
		return;

	if(ent->dead)
		return;

	ent->flags &= ~FL_GODMODE;
	ent->health = 0;

	meansOfDeath = MOD_SUICIDE;

	P_Die(ent, ent, ent, 100000, vec3_origin);
}


/*
 * G_Wave_f
 */
static void G_Wave_f(edict_t *ent){
	int i;

	i = atoi(gi.Argv(1));

	// can't wave when ducked
	if(ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if(ent->client->anim_priority > ANIM_WAVE)
		return;

	ent->client->anim_priority = ANIM_WAVE;

	switch(i){
		case 0:
			gi.Cprintf(ent, PRINT_LOW, "flipoff\n");
			ent->s.frame = FRAME_flip01 - 1;
			ent->client->anim_end = FRAME_flip12;
			break;
		case 1:
			gi.Cprintf(ent, PRINT_LOW, "salute\n");
			ent->s.frame = FRAME_salute01 - 1;
			ent->client->anim_end = FRAME_salute11;
			break;
		case 2:
			gi.Cprintf(ent, PRINT_LOW, "taunt\n");
			ent->s.frame = FRAME_taunt01 - 1;
			ent->client->anim_end = FRAME_taunt17;
			break;
		case 3:
			gi.Cprintf(ent, PRINT_LOW, "wave\n");
			ent->s.frame = FRAME_wave01 - 1;
			ent->client->anim_end = FRAME_wave11;
			break;
		case 4:
		default:
			gi.Cprintf(ent, PRINT_LOW, "point\n");
			ent->s.frame = FRAME_point01 - 1;
			ent->client->anim_end = FRAME_point12;
			break;
	}
}


/*
 * G_Say_f
 */
static void G_Say_f(edict_t *ent){
	int i;
	size_t len;
	qboolean team, arg0;
	char *c, text[256];
	edict_t *other;
	gclient_t *cl;

	if(ent->client->muted){
		gi.Cprintf(ent, PRINT_HIGH, "You have been muted.  You're probably an asshole.\n");
		return;
	}

	memset(text, 0, sizeof(text));

	c = gi.Argv(0);
	team = false;
	arg0 = true;

	if(!strncasecmp(c, "say", 3)){

		if(gi.Argc() == 1)
			return;

		if(!strcasecmp(c, "say_team") && (level.teams || level.ctf))
			team = true;

		arg0 = false;
	}

	if(team)
		snprintf(text, sizeof(text), "%s^%d: ", ent->client->locals.netname, CON_COLOR_TEAMCHAT);
	else
		snprintf(text, sizeof(text), "%s^%d: ", ent->client->locals.netname, CON_COLOR_CHAT);
	len = strlen(text);

	i = sizeof(text) - strlen(text) - 2;
	c = gi.Args();

	if(arg0){  // not say or say_team, just arbitrary chat from the console
		strncat(text, gi.Argv(0), i);
		strcat(text, " ");
		strncat(text, c, i);
	} else {  // say or say_team
		if(c[0] == '"'){  // strip quotes if necessary
			strncat(text, c + 1, i);
			text[strlen(text) - 1] = 0;
		}
		else {
			strncat(text, c, i);
		}
	}
	strcat(text, "\n");

	// suppress empty messages
	arg0 = true;
	c = text+len;
	while (*c && arg0) {
		if (IS_COLOR(c))
			c++;
		else if(!IS_LEGACY_COLOR(c) && *c != '\n' && *c != ' ')
			arg0 = false;
		c++;
	}
	if (arg0)
		return;

	if(!team){  // chat flood protection, does not pertain to teams
		cl = ent->client;

		if(level.time < cl->chat_time)
			return;

		cl->chat_time = level.time + 1;
	}

	for(i = 1; i <= sv_maxclients->value; i++){  // print to clients
		other = &g_edicts[i];

		if(!other->inuse)
			continue;

		if(team){
			if(!G_OnSameTeam(ent, other)){
				continue;
			} else {
				gi.Cprintf(other, PRINT_TEAMCHAT, "%s", text);
			}
		} else {
			gi.Cprintf(other, PRINT_CHAT, "%s", text);
		}
	}

	if(dedicated->value){  // print to the console
		if(team)
			gi.Cprintf(NULL, PRINT_TEAMCHAT, "%s", text);
		else
			gi.Cprintf(NULL, PRINT_CHAT, "%s", text);
	}

	if(chatlog != NULL)  // print to chatlog
		fprintf(chatlog, "%s", text);
}


/*
 * G_PlayerList_f
 */
static void G_PlayerList_f(edict_t *ent){
	int i;
	char st[80];
	char text[1400];
	edict_t *e2;

	// connect time, ping, score, name
	*text = 0;
	for(i = 0, e2 = g_edicts + 1; i < sv_maxclients->value; i++, e2++){
		if(!e2->inuse)
			continue;

		snprintf(st, sizeof(st), "%02d:%02d %4d %3d %-16s %s\n",
					(level.framenum - e2->client->locals.enterframe) / 600,
					((level.framenum - e2->client->locals.enterframe) % 600) / 10,
					e2->client->ping,
					e2->client->locals.score,
					e2->client->locals.netname,
					e2->client->locals.skin);

		if(strlen(text) + strlen(st) > sizeof(text) - 200){
			sprintf(text + strlen(text), "And more...\n");
			gi.Cprintf(ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	gi.Cprintf(ent, PRINT_HIGH, "%s", text);
}

static const char *vote_cmds[] = {
	"map", "g_fraglimit", "g_roundlimit", "g_capturelimit", "g_timelimit",
	"g_gameplay", "kick", "mute", "unmute", "g_teams", "g_ctf", "g_match",
	"g_rounds", "restart", NULL
};


/*
 * Vote_Help
 *
 * Inspects the vote command and issues help if applicable.  Returns
 * true if the command received help and may therefore be ignored, false
 * otherwise.
 */
static qboolean Vote_Help(edict_t *ent){
	int i, j, len;
	char msg[1024];

	if(!level.votetime){  // check for yes/no
		if(gi.Argc() == 1 && (!strcasecmp(gi.Argv(0), "yes") || !strcasecmp(gi.Argv(0), "no"))){
			gi.Cprintf(ent, PRINT_HIGH, "There is not a vote in progress\n");  // shorthand
			return true;
		}
		if(gi.Argc() == 2 && (!strcasecmp(gi.Argv(1), "yes") || !strcasecmp(gi.Argv(1), "no"))){
			gi.Cprintf(ent, PRINT_HIGH, "There is not a vote in progress\n");  // explicit
			return true;
		}
	}

	memset(msg, 0, sizeof(msg));

	i = 0;
	if(gi.Argc() == 1){  // no command specified, list them
		strcat(msg, "\nAvailable vote commands:\n\n");

		while(vote_cmds[i]){
			strcat(msg, "  ");
			strcat(msg, vote_cmds[i]);
			strcat(msg, "\n");
			i++;
		}
		gi.Cprintf(ent, PRINT_HIGH, "%s", msg);
		return true;
	}

	i = 0;
	while(vote_cmds[i]){  // verify that command is supported
		if(!strcasecmp(gi.Argv(1), vote_cmds[i]))
			break;
		i++;
	}

	if(!vote_cmds[i]){  // inform client if it is not
		gi.Cprintf(ent, PRINT_HIGH, "Voting on \"%s\" is not supported\n", gi.Argv(1));
		return true;
	}

	if(!strcasecmp(gi.Argv(1), "restart"))
		return false;  // takes no args, this is okay

	// command-specific help for some commands
	if(gi.Argc() == 2 && !strcasecmp(gi.Argv(1), "map")){  // list available maps

		if(!maplist.count){  // no maps in maplist
			gi.Cprintf(ent, PRINT_HIGH, "Map voting is not available\n");
			return true;
		}

		strcat(msg, "\nAvailable maps:\n\n");

		j = 0;
		for(i = 0; i < maplist.count; i++){
			len = strlen(maplist.maps[i].name) + 3;
			len += strlen(maplist.maps[i].title) + 2;

			if(j + len > sizeof(msg))  // don't overrun msg
				break;

			strcat(msg, "  ");
			strcat(msg, maplist.maps[i].name);
			strcat(msg, " ");
			strcat(msg, maplist.maps[i].title);
			strcat(msg, "\n");
			j += len;
		}

		gi.Cprintf(ent, PRINT_HIGH, "%s", msg);
		return true;
	}

	if(gi.Argc() == 2 && !strcasecmp(gi.Argv(1), "g_gameplay")){  // list gameplay modes
		gi.Cprintf(ent, PRINT_HIGH, "\nAvailable gameplay modes:\n\n"
			 "  DEATHMATCH\n  INSTAGIB\n  ARENA\n");
		return true;
	}

	if(gi.Argc() == 2){  // general catch for invalid commands
		gi.Cprintf(ent, PRINT_HIGH, "Usage: %s <command args>\n", gi.Argv(0));
		return true;
	}

	return false;
}


/*
 * G_Vote_f
 */
static void G_Vote_f(edict_t *ent){
	char *c, vote[64];
	int i;

	if(!g_voting->value){
		gi.Cprintf(ent, PRINT_HIGH, "Voting is not allowed");
		return;
	}

	c = gi.Argv(0);
	if(!strcasecmp(c, "yes") || !strcasecmp(c, "no"))
		strcpy(vote, c);  // allow shorthand voting
	else {  // or the explicit syntax
		strncpy(vote, gi.Args(), sizeof(vote) - 1);
		vote[sizeof(vote) - 1] = 0;
	}

	if(level.votetime){  // check for vote from client
		if(ent->client->locals.vote){
			gi.Cprintf(ent, PRINT_HIGH, "You've already voted\n");
			return;
		}
		if(strcasecmp(vote, "yes") == 0)
			ent->client->locals.vote = VOTE_YES;
		else if(strcasecmp(vote, "no") == 0)
			ent->client->locals.vote = VOTE_NO;
		else {  // only yes and no are valid during a vote
			gi.Cprintf(ent, PRINT_HIGH, "A vote \"%s\" is already in progress\n", level.vote_cmd);
			return;
		}

		level.votes[ent->client->locals.vote]++;
		gi.Bprintf(PRINT_HIGH, "Voting results \"%s\":\n  %d Yes     %d No\n",
				level.vote_cmd, level.votes[VOTE_YES], level.votes[VOTE_NO]);
		return;
	}

	if(Vote_Help(ent))  // vote command got help, ignore it
		return;

	if(!strcasecmp(gi.Argv(1), "map")){  // ensure map is in maplist
		for(i = 0; i < maplist.count; i++){
			if(!strcasecmp(gi.Argv(2), maplist.maps[i].name))
				break;  // found it
		}

		if(i == maplist.count){  // inform client if it is not
			gi.Cprintf(ent, PRINT_HIGH, "Map \"%s\" is not available\n", gi.Argv(2));
			return;
		}
	}

	strncpy(level.vote_cmd, vote, sizeof(level.vote_cmd) - 1);
	level.vote_cmd[sizeof(level.vote_cmd) - 1] = 0;
	level.votetime = level.time;

	ent->client->locals.vote = VOTE_YES;  // client has implicity voted
	level.votes[VOTE_YES] = 1;

	gi.Configstring(CS_VOTE, level.vote_cmd);  // send to layout

	gi.Bprintf(PRINT_HIGH, "%s has called a vote:\n"
			"  %s\n"
			"To vote, press F1 for yes or F2 for no\n",
			ent->client->locals.netname, level.vote_cmd);
}


/*
 * G_AddClientToTeam
 *
 * Returns true if the client's team was changed, false otherwise.
 */
qboolean G_AddClientToTeam(edict_t *ent, char *teamname){
	team_t *team;

	if(level.matchtime && level.matchtime <= level.time){
		gi.Cprintf(ent, PRINT_HIGH, "Match has already started\n");
		return false;
	}

	if(!(team = G_TeamByName(teamname))){  // resolve team
		gi.Cprintf(ent, PRINT_HIGH, "Team \"%s\" doesn't exist\n", teamname);
		return false;
	}

	ent->client->locals.team = team;
	ent->client->locals.spectator = false;
	ent->client->locals.ready = false;

	P_UserinfoChanged(ent, ent->client->locals.userinfo);
	return true;
}


/*
 * G_AddClientToRound
 */
static void G_AddClientToRound(edict_t *ent){
	int score;  // keep score across rounds

	if(level.roundtime && level.roundtime <= level.time){
		gi.Cprintf(ent, PRINT_HIGH, "Round has already started\n");
		return;
	}

	score = ent->client->locals.score;

	if(level.teams){  // attempt to add client to team
		if(!G_AddClientToTeam(ent, gi.Argv(1)))
			return;
	}
	else {  // simply allow them to join
		if(!ent->client->locals.spectator)
			return;
		ent->client->locals.spectator = false;
	}

	P_Respawn(ent, true);
	ent->client->locals.score = score;  // lastly restore score
}


/*
 * G_Team_f
 */
static void G_Team_f(edict_t *ent){

	if((level.teams || level.ctf) && gi.Argc() != 2){
		gi.Cprintf(ent, PRINT_HIGH, "Usage: %s <%s|%s>\n",
				gi.Argv(0), good.name, evil.name);
		return;
	}

	if(level.rounds){  // special case for rounds play
		G_AddClientToRound(ent);
		return;
	}

	if(!level.teams && !level.ctf){
		gi.Cprintf(ent, PRINT_HIGH, "Teams are disabled\n");
		return;
	}

	if(!G_AddClientToTeam(ent, gi.Argv(1)))
		return;

	P_Respawn(ent, true);
}


/*
 * G_Teamname_f
 */
static void G_Teamname_f(edict_t *ent){
	int cs;
	char *s;
	team_t *t;

	if(gi.Argc() != 2){
		gi.Cprintf(ent, PRINT_HIGH, "Usage: %s <name>\n", gi.Argv(0));
		return;
	}

	if(!ent->client->locals.team){
		gi.Cprintf(ent, PRINT_HIGH, "You're not on a team\n");
		return;
	}

	t = ent->client->locals.team;

	if(level.time - t->nametime < TEAM_CHANGE_TIME)
		return;  // prevent change spamming

	s = gi.Argv(1);

	if(strlen(s))  // something valid-ish was provided
		strncpy(t->name, s, sizeof(t->name) - 1);
	else strcpy(t->name, (t == &good ? "Good" : "Evil"));

	s = t->name;
	s[sizeof(t->name) - 1] = 0;

	t->nametime = level.time;

	cs = t == &good ? CS_TEAMGOOD : CS_TEAMEVIL;
	gi.Configstring(cs, va("%15s", t->name));

	gi.Bprintf(PRINT_HIGH, "%s changed teamname to %s\n",
			ent->client->locals.netname, t->name);
}


/*
 * G_Teamskin_f
 */
static void G_Teamskin_f(edict_t *ent){
	int i;
	gclient_t *cl;
	char *c, *s;
	team_t *t;

	if(gi.Argc() != 2){
		gi.Cprintf(ent, PRINT_HIGH, "Usage: %s <skin>\n", gi.Argv(0));
		return;
	}

	if(!ent->client->locals.team){
		gi.Cprintf(ent, PRINT_HIGH, "You're not on a team\n");
		return;
	}

	t = ent->client->locals.team;

	if(level.time - t->skintime < TEAM_CHANGE_TIME)
		return;  // prevent change spamming

	s = gi.Argv(1);

	if(strlen(s))  // something valid-ish was provided
		strncpy(t->skin, s, sizeof(t->skin) - 1);
	else strcpy(t->skin, "ichabod");

	s = t->skin;
	s[sizeof(t->skin) - 1] = 0;

	c = strchr(s, '/');

	// let players use just the model name, client will find skin
	if(!c || !strlen(c)){
		if(c)  // null terminate for strcat
			*c = 0;

		strncat(t->skin, "/default", sizeof(t->skin) - 1 - strlen(s));
	}

	t->skintime = level.time;

	for(i = 0; i < sv_maxclients->value; i++){  // update skins
		cl = game.clients + i;

		if(!cl->locals.team || cl->locals.team != t)
			continue;

		strncpy(cl->locals.skin, s, sizeof(cl->locals.skin) - 1);
		cl->locals.skin[sizeof(cl->locals.skin) - 1] = 0;

		gi.Configstring(CS_PLAYERSKINS + i, va("%s\\%s", cl->locals.netname, cl->locals.skin));
	}

	gi.Bprintf(PRINT_HIGH, "%s changed teamskin to %s\n",
			ent->client->locals.netname, t->skin);
}


/*
 * G_Ready_f
 *
 * If match is enabled, all clients must issue ready for game to start.
 */
static void G_Ready_f(edict_t *ent){
	int i, g, e, clients;
	gclient_t *cl;

	if(!level.match){
		gi.Cprintf(ent, PRINT_HIGH, "Match is disabled\n");
		return;
	}

	if(ent->client->locals.spectator){
		gi.Cprintf(ent, PRINT_HIGH, "You're a spectator\n");
		return;
	}

	if(ent->client->locals.ready){
		gi.Cprintf(ent, PRINT_HIGH, "You're already ready\n");
		return;
	}

	ent->client->locals.ready = true;

	clients = g = e = 0;

	for(i = 0; i < sv_maxclients->value; i++){  // is everyone ready?
		cl = game.clients + i;

		if(!g_edicts[i + 1].inuse)
			continue;

		if(cl->locals.spectator)
			continue;

		if(!cl->locals.ready)
			break;

		clients++;

		if(level.teams || level.ctf)
			cl->locals.team == &good ? g++ : e++;
	}

	if(i != (int)sv_maxclients->value)  // someone isn't ready
		return;

	if(clients < 2)  // need at least 2 clients to trigger match
		return;

	if((level.teams || level.ctf) && (!g || !e))  // need at least 1 player per team
		return;

	if(((int)level.teams == 2 || (int)level.ctf == 2) && (g != e)){  // balanced teams required
		gi.Bprintf(PRINT_HIGH, "Teams must be balanced for match to start\n");
		return;
	}

	gi.Bprintf(PRINT_HIGH, "Match starting in 10 seconds..\n");
	level.matchtime = level.time + 10.0;

	level.start_match = true;
}

/*
 * G_Unready_f
 */
static void G_Unready_f(edict_t *ent){

	if(!level.match){
		gi.Cprintf(ent, PRINT_HIGH, "Match is disabled\n");
		return;
	}

	if(ent->client->locals.spectator){
		gi.Cprintf(ent, PRINT_HIGH, "You're a spectator\n");
		return;
	}

	if(level.matchtime){
		gi.Cprintf(ent, PRINT_HIGH, "Match has started\n");
		return;
	}

	if(!ent->client->locals.ready){
		gi.Cprintf(ent, PRINT_HIGH, "You are not ready\n");
		return;
	}

	ent->client->locals.ready = false;
	level.start_match = false;
}


/*
 * G_Spectate_f
 */
static void G_Spectate_f(edict_t *ent){
	qboolean spectator;

	// prevent spectator spamming
	if(level.time - ent->client->respawn_time < 3.0)
		return;

	// prevent spectators from joining matches
	if(level.matchtime && ent->client->locals.spectator){
		gi.Cprintf(ent, PRINT_HIGH, "Match has already started\n");
		return;
	}

	// prevent spectators from joining rounds
	if(level.roundtime && ent->client->locals.spectator){
		gi.Cprintf(ent, PRINT_HIGH, "Round has already started\n");
		return;
	}

	spectator = ent->client->locals.spectator;

	if(ent->client->locals.spectator){  // they wish to join
		if(level.teams || level.ctf){
			if(g_autojoin->value)  // assign them to a team
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
			else {  // or ask them to pick
				gi.Cprintf(ent, PRINT_HIGH, "Use team <%s|%s> to join the game\n",
						good.name, evil.name);
				return;
			}
		}
	}

	ent->client->locals.spectator = !spectator;
	P_Respawn(ent, true);
}


/*
 * G_Score_f
 */
void G_Score_f(edict_t *ent){

	if(ent->client->showscores){
		ent->client->showscores = false;
		return;
	}

	ent->client->showscores = true;

	if(level.teams || level.ctf)
		P_TeamsScoreboard(ent);
	else P_Scoreboard(ent);

	gi.Unicast(ent, true);
}


/*
 * P_Command
 */
void P_Command(edict_t *ent){
	char *cmd;

	if(!ent->client)
		return;  // not fully in game yet

	cmd = gi.Argv(0);

	if(strcasecmp(cmd, "say") == 0){
		G_Say_f(ent);
		return;
	}
	if(strcasecmp(cmd, "say_team") == 0){
		G_Say_f(ent);
		return;
	}

	if(level.intermissiontime)
		return;

	if(strcasecmp(cmd, "score") == 0)
		G_Score_f(ent);
	else if(strcasecmp(cmd, "spectate") == 0)
		G_Spectate_f(ent);
	else if(strcasecmp(cmd, "team") == 0 || strcasecmp(cmd, "join") == 0)
		G_Team_f(ent);
	else if(strcasecmp(cmd, "teamname") == 0)
		G_Teamname_f(ent);
	else if(strcasecmp(cmd, "teamskin") == 0)
		G_Teamskin_f(ent);
	else if(strcasecmp(cmd, "ready") == 0)
		G_Ready_f(ent);
	else if(strcasecmp(cmd, "unready") == 0)
		G_Unready_f(ent);
	else if(strcasecmp(cmd, "use") == 0)
		G_Use_f(ent);
	else if(strcasecmp(cmd, "drop") == 0)
		G_Drop_f(ent);
	else if(strcasecmp(cmd, "give") == 0)
		G_Give_f(ent);
	else if(strcasecmp(cmd, "god") == 0)
		G_God_f(ent);
	else if(strcasecmp(cmd, "noclip") == 0)
		G_Noclip_f(ent);
	else if(strcasecmp(cmd, "weapprev") == 0)
		G_WeapPrev_f(ent);
	else if(strcasecmp(cmd, "weapnext") == 0)
		G_WeapNext_f(ent);
	else if(strcasecmp(cmd, "weaplast") == 0)
		G_WeapLast_f(ent);
	else if(strcasecmp(cmd, "kill") == 0)
		G_Kill_f(ent);
	else if(strcasecmp(cmd, "wave") == 0)
		G_Wave_f(ent);
	else if(strcasecmp(cmd, "playerlist") == 0)
		G_PlayerList_f(ent);
	else if(strcasecmp(cmd, "vote") == 0 || strcasecmp(cmd, "yes") == 0 || strcasecmp(cmd, "no") == 0)
		G_Vote_f(ent);
	else  // anything that doesn't match a command will be a chat
		G_Say_f(ent);
}
