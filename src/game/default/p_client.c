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


/*QUAKED info_player_start(1 0 0)(-16 -16 -24)(16 16 32)
The normal starting point for a level.
*/
void G_info_player_start(edict_t *self){}

/*QUAKED info_player_intermission(1 0 1)(-16 -16 -24)(16 16 32)
Level intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.
'pitch yaw roll'
*/
void G_info_player_intermission(edict_t *self){}

/*QUAKED info_player_deathmatch(1 0 1)(-16 -16 -24)(16 16 32)
potential spawning position for deathmatch games
*/
void G_info_player_deathmatch(edict_t *self){}

/*QUAKED info_player_team1(1 0 1)(-16 -16 -24)(16 16 32)
potential spawning position for team games
*/
void G_info_player_team1(edict_t *self){}

/*QUAKED info_player_team2(1 0 1)(-16 -16 -24)(16 16 32)
potential spawning position for team games
*/
void G_info_player_team2(edict_t *self){}


/*
 * P_ClientObituary
 *
 * Make a tasteless death announcement.
 */
static void P_ClientObituary(edict_t *self, edict_t *inflictor, edict_t *attacker){
	int ff, mod;
	char *message, *message2;
	gclient_t *killer;

	ff = meansOfDeath & MOD_FRIENDLY_FIRE;
	mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
	message = NULL;
	message2 = "";

	killer = attacker->client ? attacker->client : self->client;

	if(!level.warmup && fraglog != NULL){  // write fraglog

		fprintf(fraglog, "\\%s\\%s\\\n", killer->locals.netname, self->client->locals.netname);

		fflush(fraglog);
	}

#ifdef HAVE_MYSQL
	if(!level.warmup && mysql != NULL){  // insert to db

		snprintf(sql, sizeof(sql), "insert into frag values(null, now(), '%s', '%s', '%s', %d)",
				 level.name, killer->locals.sqlname, self->client->locals.sqlname, mod
				);

		sql[sizeof(sql) - 1] = '\0';
		mysql_query(mysql, sql);
	}
#endif

	switch(mod){
		case MOD_SUICIDE:
			message = "sucks at life";
			break;
		case MOD_FALLING:
			message = "challenged gravity";
			break;
		case MOD_CRUSH:
			message = "likes it tight";
			break;
		case MOD_WATER:
			message = "took a drink";
			break;
		case MOD_SLIME:
			message = "got slimed";
			break;
		case MOD_LAVA:
			message = "did a back flip into the lava";
			break;
		case MOD_TRIGGER_HURT:
			message = "sucks at life";
			break;
	}

	if(attacker == self){
		switch(mod){
			case MOD_G_SPLASH:
				message = "went pop";
				break;
			case MOD_R_SPLASH:
				message = "needs glasses";
				break;
			case MOD_L_DISCHARGE:
				message = "took a toaster bath";
				break;
			case MOD_BFG_BLAST:
				message = "should have used a smaller gun";
				break;
			default:
				message = "sucks at life";
				break;
		}
	}

	if(message){  // suicide
		gi.Bprintf(PRINT_MEDIUM, "%s %s.\n", self->client->locals.netname, message);

		if(level.warmup)
			return;

		self->client->locals.score--;

		if((level.teams || level.ctf) && self->client->locals.team)
			self->client->locals.team->score--;

		return;
	}

	self->enemy = attacker;
	if(attacker && attacker->client){
		switch(mod){
			case MOD_SHOTGUN:
				message = "was gunned down by";
				message2 = "'s pea shooter";
				break;
			case MOD_SSHOTGUN:
				message = "was blown away by";
				message2 = "'s super shotgun";
				break;
			case MOD_MACHINEGUN:
				message = "was chewed up by";
				break;
			case MOD_GRENADE:
				message = "was popped by";
				message2 = "'s grenade";
				break;
			case MOD_G_SPLASH:
				message = "was shredded by";
				message2 = "'s shrapnel";
				break;
			case MOD_ROCKET:
				message = "was dry-anal-powerslammed by";
				message2 = "'s rocket";
				break;
			case MOD_R_SPLASH:
				message = "almost dodged";
				message2 = "'s rocket";
				break;
			case MOD_HYPERBLASTER:
				message = "was melted by";
				message2 = "'s hyperblaster";
				break;
			case MOD_LIGHTNING:
				message = "was tased by";
				message2 = "'s lightning";
				break;
			case MOD_L_DISCHARGE:
				message = "sipped";
				message2 = "'s discharge";
				break;
			case MOD_RAILGUN:
				message = "was poked by";
				message2 = "'s needledick";
				break;
			case MOD_BFG_LASER:
				message = "saw the pretty lights from";
				message2 = "'s BFG";
				break;
			case MOD_BFG_BLAST:
				message = "was disintegrated by";
				message2 = "'s BFG blast";
				break;
			case MOD_TELEFRAG:
				message = "tried to invade";
				message2 = "'s personal space";
				break;
			}

		if(message){

			gi.Bprintf(PRINT_MEDIUM, "%s%s %s %s %s\n", (ff ? "^1TEAMKILL^7 " : ""),
					self->client->locals.netname, message,
					attacker->client->locals.netname, message2
			);

			if(level.warmup)
				return;

			if(ff)
				attacker->client->locals.score--;
			else
				attacker->client->locals.score++;

			if((level.teams || level.ctf) && attacker->client->locals.team){  // handle team scores too
				if(ff)
					attacker->client->locals.team->score--;
				else
					attacker->client->locals.team->score++;
			}
		}
	}
}


/*
 * TossWeapon
 */
static void TossWeapon(edict_t *self){
	gitem_t *item;

	// don't drop weapon when falling into void
	if(meansOfDeath == MOD_TRIGGER_HURT)
		return;

	item = self->client->locals.weapon;

	if(!self->client->locals.inventory[self->client->ammo_index])
		return;  // don't drop when out of ammo

	G_DropItem(self, item);
}


/*
 * TossQuadDamage
 */
static void TossQuadDamage(edict_t *self){
	edict_t *quad;

	// don't drop quad when falling into void
	if(meansOfDeath == MOD_TRIGGER_HURT)
		return;

	if(!self->client->locals.inventory[quad_damage_index])
		return;

	quad = G_DropItem(self, G_FindItemByClassname("item_quad"));
	quad->timestamp = self->client->quad_damage_time;

	self->client->quad_damage_time = 0.0;
	self->client->locals.inventory[quad_damage_index] = 0;
}


/*
 * TossFlag
 */
static void TossFlag(edict_t *self){
	team_t *ot;
	edict_t *of;
	int index;

	if(!(ot = G_OtherTeam(self->client->locals.team)))
		return;

	if(!(of = G_FlagForTeam(ot)))
		return;

	index = ITEM_INDEX(of->item);

	if(!self->client->locals.inventory[index])
		return;

	gi.Bprintf(PRINT_HIGH, "%s dropped the %s flag\n",
			self->client->locals.netname, ot->name);

	// don't drop flag when falling into void
	if(meansOfDeath == MOD_TRIGGER_HURT){

		gi.Sound(self, CHAN_AUTO, gi.SoundIndex("misc/return.wav"),
				1.0, ATTN_NONE, 0.0);

		of->svflags &= ~SVF_NOCLIENT;
		of->s.event = EV_ITEM_RESPAWN;

		return;
	}

	G_DropItem(self, of->item);
}


/*
 * P_Pain
 */
void P_Pain(edict_t *self, edict_t *other, int damage, int knockback){

	if(other && other->client && other != self){  // play a hit sound
		gi.Sound(other, CHAN_VOICE, gi.SoundIndex("misc/hit.wav"),
				1.0, ATTN_STATIC, 0.0);
	}
}


/*
 * P_Die
 */
void P_Die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){

	gi.Sound(self, CHAN_VOICE, gi.SoundIndex("*death_1.wav"),
			1.0, ATTN_NORM, 0.0);

	self->client->respawn_time = level.time + 1.0;
	self->client->ps.pmove.pm_type = PM_DEAD;

	P_ClientObituary(self, inflictor, attacker);

	if(!level.gameplay && !level.warmup)  // drop weapon
		TossWeapon(self);

	self->client->newweapon = NULL;  // reset weapon state
	P_ChangeWeapon(self);

	if(!level.gameplay && !level.warmup)  // drop quad
		TossQuadDamage(self);

	if(level.ctf && !level.warmup)  // drop flag in ctf
		TossFlag(self);

	G_Score_f(self);  // show scores

	self->svflags |= SVF_NOCLIENT;

	self->dead = true;
	self->classname = "dead";

	self->s.modelindex = 0;
	self->s.modelindex2 = 0;
	self->s.sound = 0;
	self->s.event = 0;

	self->solid = SOLID_NOT;
	self->takedamage = false;

	gi.LinkEntity(self);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_GIB);
	gi.WritePosition(self->s.origin);
	gi.Multicast(self->s.origin, MULTICAST_PVS);
}


/*
 *  Stocks client's inventory with specified item.  Weapons receive
 *  specified quantity of ammo, while health and armor are set to
 *  the specified quantity.
 */
static void P_Give(gclient_t *client, char *it, int quantity){
	gitem_t *item;
	int index;

	if(!strcasecmp(it, "Health")){
		client->locals.health = quantity;
		return;
	}

	if(!strcasecmp(it, "Armor")){
		client->locals.armor = quantity;
		return;
	}

	item = G_FindItem(it);

	if(!item)
		return;

	index = ITEM_INDEX(item);

	if(item->flags & IT_WEAPON){  // weapons receive quantity as ammo
		client->locals.inventory[index] = 1;

		item = G_FindItem(item->ammo);
		index = ITEM_INDEX(item);

		if(quantity > -1)
			client->locals.inventory[index] = quantity;
		else
			client->locals.inventory[index] = item->quantity;
	}
	else {  // while other items receive quantity directly
		if(quantity > -1)
			client->locals.inventory[index] = quantity;
		else
			client->locals.inventory[index] = item->quantity;
	}
}


/*
 * P_GiveLevelLocals
 */
static qboolean P_GiveLevelLocals(gclient_t *client){
	char buf[512], *it, *q;
	int quantity;

	if(!strlen(level.give))
		return false;

	strncpy(buf, level.give, sizeof(buf));

	it = strtok(buf, ",");

	while(true){

		if(!it)
			break;

		it = Com_TrimString(it);

		if((q = strrchr(it, ' '))){
			quantity = atoi(q + 1);

			if(quantity > -1)  // valid quantity
				*q = 0;
		}
		else
			quantity = -1;

		P_Give(client, it, quantity);
		it = strtok(NULL, ",");
	}

	return true;
}

/*
 * P_InitClientLocals
 */
static void P_InitClientLocals(gclient_t *client){
	gitem_t *item;
	int i;

	// clear inventory
	for(i = 0; i < MAX_ITEMS; i++)
		client->locals.inventory[i] = 0;

	// set max inventory levels
	client->locals.health = 100;
	client->locals.max_health = 100;

	client->locals.armor = 0;
	client->locals.max_armor = 200;

	client->locals.max_bullets = 200;
	client->locals.max_shells = 80;
	client->locals.max_rockets = 50;
	client->locals.max_grenades = 50;
	client->locals.max_cells = 200;
	client->locals.max_slugs = 50;

	// instagib gets railgun and slugs, both in normal mode and warmup
	if(level.gameplay == INSTAGIB){
		P_Give(client, "Railgun", 1000);
		item = G_FindItem("Railgun");
	}
	// arena or dm warmup yields all weapons, health, etc..
	else if((level.gameplay == ARENA) || level.warmup){
		P_Give(client, "Railgun", 50);
		P_Give(client, "Lightning", 200);
		P_Give(client, "Hyperblaster", 200);
		P_Give(client, "Rocket Launcher", 50);
		P_Give(client, "Grenade Launcher", 50);
		P_Give(client, "Machinegun", 200);
		P_Give(client, "Super Shotgun", 80);
		P_Give(client, "Shotgun", 80);

		P_Give(client, "Armor", 200);

		item = G_FindItem("Rocket Launcher");
	}
	// dm gets shotgun and 10 shots
	else {
		P_Give(client, "Shotgun", 10);
		item = G_FindItem("Shotgun");
	}

	if(P_GiveLevelLocals(client)){  // use the best weapon we were given by level
		P_NoAmmoWeaponChange(client);
		client->locals.weapon = client->newweapon;
	}
	else  // or use best given by gameplay
		client->locals.weapon = item;

	// clean up weapon state
	client->locals.lastweapon = NULL;
	client->newweapon = NULL;
	client->locals.weapon_frame = 0;
}


/*
 * P_EnemyRangeFromSpot
 *
 * Returns the distance to the nearest enemy from the given spot
 */
static float P_EnemyRangeFromSpot(edict_t *ent, edict_t *spot){
	edict_t *player;
	float bestplayerdistance;
	vec3_t v;
	int n;
	float playerdistance;

	bestplayerdistance = 9999999;

	for(n = 1; n <= sv_maxclients->value; n++){
		player = &g_edicts[n];

		if(!player->inuse)
			continue;

		if(player->health <= 0)
			continue;

		if(player->client->locals.spectator)
			continue;

		VectorSubtract(spot->s.origin, player->s.origin, v);
		playerdistance = VectorLength(v);

		// ignore teammates, so long as they're away from the spawnpoint
		if(playerdistance > 20 && (level.teams || level.ctf) && ent->client->locals.team){
			if(!player->client->locals.team)
				continue;
			if(ent->client->locals.team == player->client->locals.team)
				continue;
		}

		if(playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}


/*
 * P_SelectRandomDeathmatchSpawnPoint
 */
static edict_t *P_SelectRandomSpawnPoint(edict_t *ent, const char *classname){
	edict_t *spot;
	int count = 0;

	spot = NULL;

	while((spot = G_Find(spot, FOFS(classname), classname)) != NULL)
		count++;

	if(!count)
		return NULL;

	count = rand() % count;

	while(count-- >= 0)
		spot = G_Find(spot, FOFS(classname), classname);

	return spot;
}


/*
 * P_SelectFarthestDeathmatchSpawnPoint
 */
static edict_t *P_SelectFarthestSpawnPoint(edict_t *ent, const char *classname){
	edict_t *bestspot;
	float bestdistance, bestenemydistance;
	edict_t *spot;

	spot = NULL;
	bestspot = NULL;
	bestdistance = 0;
	while((spot = G_Find(spot, FOFS(classname), classname)) != NULL){
		bestenemydistance = P_EnemyRangeFromSpot(ent, spot);

		if(bestenemydistance > bestdistance){
			bestspot = spot;
			bestdistance = bestenemydistance;
		}
	}

	if(bestspot)
		return bestspot;

	// if there is an enemy just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	spot = G_Find(NULL, FOFS(classname), classname);

	return spot;
}


/*
 * P_SelectDeathmatchSpawnPoint
 */
static edict_t *P_SelectDeathmatchSpawnPoint(edict_t *ent){
	if((int)(g_dmflags->value) & DF_SPAWN_RANDOM)
		return P_SelectRandomSpawnPoint(ent, "info_player_deathmatch");
	return P_SelectFarthestSpawnPoint(ent, "info_player_deathmatch");
}


/*
 * P_SelectCaptureSpawnPoint
 */
static edict_t *P_SelectCaptureSpawnPoint(edict_t *ent){
	char *c;

	if(!ent->client->locals.team)
		return NULL;

	c = ent->client->locals.team == &good ?
		"info_player_team1" : "info_player_team2";

	if((int)(g_dmflags->value) & DF_SPAWN_RANDOM)
		return P_SelectRandomSpawnPoint(ent, c);
	return P_SelectFarthestSpawnPoint(ent, c);
}


/*
 * P_SelectSpawnPoint
 *
 * Chooses a player start, deathmatch start, etc
 */
static void P_SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles){
	edict_t *spot = NULL;

	if(level.ctf)  // try ctf spawns first if applicable
		spot = P_SelectCaptureSpawnPoint(ent);

	if(!spot)  // fall back on dm spawns (e.g ctf games on dm maps)
		spot = P_SelectDeathmatchSpawnPoint(ent);

	// and lastly fall back on single player start
	if(!spot){
		while((spot = G_Find(spot, FOFS(classname), "info_player_start")) != NULL){
			if(!spot->targetname)  // hopefully without a target
				break;
		}

		if(!spot){  // last resort, find any
			if((spot = G_Find(spot, FOFS(classname), "info_player_start")) == NULL)
				gi.Error("P_SelectSpawnPoint: Couldn't find spawn point.");
		}
	}

	VectorCopy(spot->s.origin, origin);
	origin[2] += 12;
	VectorCopy(spot->s.angles, angles);
}


/*
 * P_PutClientInServer
 *
 * The grunt work of putting the client into the server on [re]spawn.
 */
static void P_PutClientInServer(edict_t *ent){
	vec3_t mins = { -16, -16, -24};
	vec3_t maxs = {16, 16, 32};
	vec3_t spawn_origin, spawn_angles, old_angles;
	gclient_t *client;
	client_locals_t locals;
	int i;

	// find a spawn point
	P_SelectSpawnPoint(ent, spawn_origin, spawn_angles);

	client = ent->client;

	// retain last angles for delta
	VectorCopy(ent->client->cmd_angles, old_angles);

	// reset inventory, health, etc
	P_InitClientLocals(client);

	// clear everything but locals
	locals = client->locals;
	memset(client, 0, sizeof(*client));
	client->locals = locals;

	// clear entity values
	ent->groundentity = NULL;
	ent->takedamage = true;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->inuse = true;
	ent->classname = "player";
	ent->mass = 200.0;
	ent->solid = SOLID_BBOX;
	ent->dead = false;
	ent->jump_time = 0.0;
	ent->gasp_time = 0.0;
	ent->drown_time = level.time + 12.0;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->model = "players/ichabod/tris.md2";
	ent->pain = P_Pain;
	ent->die = P_Die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->svflags = 0;

	// copy these back once they have been set in locals
	ent->health = ent->client->locals.health;
	ent->max_health = ent->client->locals.max_health;

	VectorCopy(mins, ent->mins);
	VectorCopy(maxs, ent->maxs);
	VectorClear(ent->velocity);
	ent->velocity[2] = 150.0;

	// clear playerstate values
	memset(&ent->client->ps, 0, sizeof(client->ps));

	client->ps.pmove.origin[0] = spawn_origin[0] * 8;
	client->ps.pmove.origin[1] = spawn_origin[1] * 8;
	client->ps.pmove.origin[2] = spawn_origin[2] * 8;

	// clear entity state values
	ent->s.effects = 0;
	ent->s.modelindex = 255;  // will use the skin specified model
	ent->s.modelindex2 = 255;  // custom gun model
	// skinnum is player num and weapon number
	// weapon number will be added in changeweapon
	ent->s.skinnum = ent - g_edicts - 1;

	ent->s.frame = 0;
	VectorCopy(spawn_origin, ent->s.origin);
	VectorCopy(ent->s.origin, ent->s.old_origin);

	// set the delta angle of the spawn point
	for(i = 0; i < 3; i++){
		client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT(spawn_angles[i] - old_angles[i]);
	}

	VectorClear(client->cmd_angles);
	VectorClear(client->angles);
	VectorClear(ent->s.angles);

	// spawn a spectator
	if(client->locals.spectator){
		client->chase_target = NULL;

		client->locals.weapon = NULL;
		client->locals.team = NULL;
		client->locals.ready = false;

		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->svflags |= SVF_NOCLIENT;
		ent->takedamage = false;

		gi.LinkEntity(ent);
		return;
	}

	// or spawn a player
	ent->s.event = EV_TELEPORT;

	// hold in place briefly
	client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	client->ps.pmove.pm_time = 20;

	client->locals.matchnum = level.matchnum;
	client->locals.roundnum = level.roundnum;

	gi.LinkEntity(ent);

	// force the current weapon up
	client->newweapon = client->locals.weapon;
	P_ChangeWeapon(ent);
}


/*
 * P_Respawn
 *
 * In this case, voluntary means that the client has explicitly requested
 * a respawn by changing their spectator status.
 */
void P_Respawn(edict_t *ent, qboolean voluntary){

	P_PutClientInServer(ent);

	// clear scores and match/round on voluntary changes
	if(ent->client->locals.spectator && voluntary){
		ent->client->locals.score = ent->client->locals.captures = 0;
		ent->client->locals.matchnum = ent->client->locals.roundnum = 0;
	}

	ent->client->respawn_time = level.time;

	if(!voluntary)  // dont announce involuntary spectator changes
		return;

	if(ent->client->locals.spectator)
		gi.Bprintf(PRINT_HIGH, "%s likes to watch\n", ent->client->locals.netname);
	else if(ent->client->locals.team)
		gi.Bprintf(PRINT_HIGH, "%s has joined %s\n", ent->client->locals.netname,
				ent->client->locals.team->name);
	else
		gi.Bprintf(PRINT_HIGH, "%s wants some\n", ent->client->locals.netname);
}


/*
 * P_Begin
 *
 * Called when a client has finished connecting, and is ready
 * to be placed into the game.  This will happen every level load.
 */
void P_Begin(edict_t *ent){
	char welcome[256];

	int playernum = ent - g_edicts - 1;

	ent->client = game.clients + playernum;

	G_InitEdict(ent);

	P_InitClientLocals(ent->client);

	VectorClear(ent->client->cmd_angles);
	ent->client->locals.enterframe = level.framenum;

	// force spectator if match or rounds
	if(level.match || level.rounds)
		ent->client->locals.spectator = true;
	else if(level.teams || level.ctf){
		if(g_autojoin->value)
			G_AddClientToTeam(ent, G_SmallestTeam()->name);
		else
			ent->client->locals.spectator = true;
	}

	// spawn them in
	P_Respawn(ent, true);

	if(level.intermissiontime){
		P_MoveToIntermission(ent);
	} else {
		memset(welcome, 0, sizeof(welcome));

		snprintf(welcome, sizeof(welcome),
				"^2Welcome to ^7http://quake2world.net\n"
				"^2Gameplay is ^1%s\n", G_GameplayName(level.gameplay));

		if(level.teams)
			strncat(welcome, "^2Teams are enabled\n", sizeof(welcome));

		if(level.ctf)
			strncat(welcome, "^2CTF is enabled\n", sizeof(welcome));

		if(g_voting->value)
			strncat(welcome, "^2Voting is allowed\n", sizeof(welcome));

		gi.Cnprintf(ent, "%s", welcome);
	}

	// make sure all view stuff is valid
	P_EndServerFrame(ent);

	srand(time(NULL));  // set random seed
}


/*
 * P_UserinfoChanged
 */
void P_UserinfoChanged(edict_t *ent, const char *userinfo){
	const char *s;
	char *c;
	char name[MAX_NETNAME];
	int playernum, i;
	qboolean color;
	gclient_t *cl;

	// check for malformed or illegal info strings
	if(!Info_Validate(userinfo)){
		userinfo = "\\name\\newbie\\skin\\ichabod/ichabod";
	}

	cl = ent->client;

	// set name, use a temp buffer to compute length and crutch up bad names
	s = Info_ValueForKey(userinfo, "name");

	strncpy(name, s, sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	color = false;
	c = name;
	i = 0;

	// trim to 15 printable chars
	while(i < 15){

		if(!*c)
			break;

		if(IS_COLOR(c)){
			color = true;
			c += 2;
			continue;
		}

		c++;
		i++;
	}
	name[c - name] = 0;

	if(!i)  // name had nothing printable
		strcpy(name, "newbie");

	if(color)  // reset to white
		strcat(name, "^7");

	if(strncmp(cl->locals.netname, name, sizeof(cl->locals.netname))){

		if(strlen(cl->locals.netname))
			gi.Bprintf(PRINT_MEDIUM, "%s changed name to %s\n", cl->locals.netname, name);

		strncpy(cl->locals.netname, name, sizeof(cl->locals.netname) - 1);
		cl->locals.netname[sizeof(cl->locals.netname) - 1] = 0;
	}

#ifdef HAVE_MYSQL
	if(mysql != NULL){  // escape name for safe db insertions

		Com_StripColor(cl->locals.netname, name);

		mysql_real_escape_string(mysql, name, cl->locals.sqlname,
				sizeof(cl->locals.sqlname));
	}
#endif

	// set skin
	if((level.teams || level.ctf) && cl->locals.team)  // players must use teamskin to change
		s = cl->locals.team->skin;
	else
		s = Info_ValueForKey(userinfo, "skin");

	if(strlen(s))  // something valid-ish was provided
		strncpy(cl->locals.skin, s, sizeof(cl->locals.skin) - 1);
	else {
		strcpy(cl->locals.skin, "ichabod");
		cl->locals.skin[sizeof(cl->locals.skin) - 1] = 0;
	}

	s = cl->locals.skin;

	c = strchr(s, '/');

	// let players use just the model name, client will find skin
	if(!c || !strlen(c)){
		if(c)  // null terminate for strcat
			*c = 0;

		strncat(cl->locals.skin, "/default", sizeof(cl->locals.skin) - 1 - strlen(s));
	}

	// set color
	s = Info_ValueForKey(userinfo, "color");
	cl->locals.color = ColorByName(s, 243);

	playernum = ent - g_edicts - 1;

	// combine name and skin into a configstring
	gi.Configstring(CS_PLAYERSKINS + playernum, va("%s\\%s", cl->locals.netname, cl->locals.skin));

	// save off the userinfo in case we want to check something later
	strncpy(ent->client->locals.userinfo, userinfo, sizeof(ent->client->locals.userinfo) - 1);
}


/*
 * P_Connect
 *
 * Called when a player begins connecting to the server.
 * The game can refuse entrance to a client by returning false.
 * If the client is allowed, the connection process will continue
 * and eventually get to P_Begin()
 * Changing levels will NOT cause this to be called again.
 */
qboolean P_Connect(edict_t *ent, char *userinfo){

	// check password
	const char *value = Info_ValueForKey(userinfo, "password");
	if(*password->string && strcmp(password->string, "none") &&
			strcmp(password->string, value)){
		Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
		return false;
	}

	// they can connect
	ent->client = game.clients + (ent - g_edicts - 1);

	// clean up locals things which are not reset on spawns
	ent->client->locals.score = 0;
	ent->client->locals.team = NULL;
	ent->client->locals.vote = VOTE_NOOP;
	ent->client->locals.spectator = false;
	ent->client->locals.netname[0] = 0;

	// set name, skin, etc..
	P_UserinfoChanged(ent, userinfo);

	if(sv_maxclients->value > 1)
		gi.Bprintf(PRINT_HIGH, "%s connected\n", ent->client->locals.netname);

	ent->svflags = 0; // make sure we start with known default
	return true;
}


/*
 * P_Disconnect
 *
 * Called when a player drops from the server.
 * Will not be called between levels.
 */
void P_Disconnect(edict_t *ent){
	int playernum;

	if(!ent->client)
		return;

	gi.Bprintf(PRINT_HIGH, "%s bitched out\n", ent->client->locals.netname);

	// send effect
	gi.WriteByte(svc_muzzleflash);
	gi.WriteShort(ent - g_edicts);
	gi.WriteByte(MZ_LOGOUT);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);

	gi.UnlinkEntity(ent);

	ent->client->locals.userinfo[0] = 0;

	ent->inuse = false;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = 0;
	ent->classname = "disconnected";

	playernum = ent - g_edicts - 1;
	gi.Configstring(CS_PLAYERSKINS + playernum, "");
}


static edict_t *pm_passent;

// pmove doesn't need to know about passent and contentmask
static trace_t P_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end){
	if(pm_passent->health > 0)
		return gi.Trace(start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	else
		return gi.Trace(start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}


/*
 * P_InventoryThink
 */
static void P_InventoryThink(edict_t *ent){

	if(ent->client->locals.inventory[quad_damage_index]){  // if they have quad

		if(ent->client->quad_damage_time < level.time){  // expire it

			ent->client->quad_damage_time = 0.0;
			ent->client->locals.inventory[quad_damage_index] = 0;

			gi.Sound(ent, CHAN_AUTO, gi.SoundIndex("quad/expire.wav"),
					1.0, ATTN_NORM, 0.0);
		}
	}

	// other runes and things can be timed out here as well
}


/*
 * P_Think
 *
 * This will be called once for each client frame, which will
 * usually be a couple times for each server frame.
 */
void P_Think(edict_t *ent, usercmd_t *ucmd){
	gclient_t *client;
	edict_t *other;
	int i, j;
	pmove_t pm;

	level.current_entity = ent;
	client = ent->client;

	if(level.intermissiontime){
		client->ps.pmove.pm_type = PM_FREEZE;
		return;
	}

	pm_passent = ent;  // ignore ourselves on traces

	if(client->chase_target){  // ensure chase is valid

		client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

		if(!client->chase_target->inuse ||
				client->chase_target->client->locals.spectator){

			other = client->chase_target;

			P_ChaseNext(ent);

			if(client->chase_target == other){  // no one to chase
				client->chase_target = NULL;
			}
		}
	}

	if(!client->chase_target){  // set up for pmove

		client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

		if(ent->movetype == MOVETYPE_NOCLIP)
			client->ps.pmove.pm_type = PM_SPECTATOR;
		else if(ent->s.modelindex != 255 || ent->dead)
			client->ps.pmove.pm_type = PM_DEAD;
		else
			client->ps.pmove.pm_type = PM_NORMAL;

        client->ps.pmove.gravity = level.gravity;

		memset(&pm, 0, sizeof(pm));
		pm.s = client->ps.pmove;

		for(i = 0; i < 3; i++){
			pm.s.origin[i] = ent->s.origin[i] * 8.0;
			pm.s.velocity[i] = ent->velocity[i] * 8.0;
		}

		pm.cmd = *ucmd;

		pm.trace = P_Trace;  // adds default params
		pm.pointcontents = gi.PointContents;

		// perform a pmove
		gi.Pmove(&pm);

		// save results of pmove
		client->ps.pmove = pm.s;

		for(i = 0; i < 3; i++){
			ent->s.origin[i] = pm.s.origin[i] * 0.125;
			ent->velocity[i] = pm.s.velocity[i] * 0.125;
		}

		VectorCopy(pm.mins, ent->mins);
		VectorCopy(pm.maxs, ent->maxs);

		client->cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		client->cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		client->cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

		// check for jump, play randomized sound
		if(ent->groundentity && !pm.groundentity && 
				(pm.cmd.upmove >= 10) && (pm.waterlevel == 0) &&
				ent->jump_time < level.time - 0.2){

			if(crandom() > 0)
				gi.Sound(ent, CHAN_VOICE, gi.SoundIndex("*jump_1.wav"),
						1.0, ATTN_NORM, 0.0);
			else
				gi.Sound(ent, CHAN_VOICE, gi.SoundIndex("*jump_2.wav"),
						1.0, ATTN_NORM, 0.0);

			ent->jump_time = level.time;
		}

		ent->viewheight = pm.viewheight;
		ent->waterlevel = pm.waterlevel;
		ent->watertype = pm.watertype;

		ent->groundentity = pm.groundentity;
		if(pm.groundentity)
			ent->groundentity_linkcount = pm.groundentity->linkcount;

		VectorCopy(pm.angles, client->angles);
		VectorCopy(pm.angles, client->ps.angles);

		gi.LinkEntity(ent);

		if(ent->movetype != MOVETYPE_NOCLIP)
			G_TouchTriggers(ent);

		// touch other objects
		for(i = 0; i < pm.numtouch; i++){

			other = pm.touchents[i];

			for(j = 0; j < i; j++)
				if(pm.touchents[j] == other)
					break;

			if(j != i)
				continue;  // duplicated

			if(!other->touch)
				continue;

			other->touch(other, ent, NULL, NULL);
		}
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// fire weapon if requested
	if(client->latched_buttons & BUTTON_ATTACK){
		if(client->locals.spectator){

			client->latched_buttons = 0;

			if(client->chase_target){  // toggle chasecam
				client->chase_target = NULL;
				client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			} else {
				P_GetChaseTarget(ent);
			}
		} else if(!client->weapon_thunk){
			client->weapon_thunk = true;
			P_WeaponThink(ent);
		}
	}

	// update chase cam if being followed
	for(i = 1; i <= sv_maxclients->value; i++){
		other = g_edicts + i;
		if(other->inuse && other->client->chase_target == ent)
			P_UpdateChaseCam(other);
	}

	P_InventoryThink(ent);
}


/*
 * P_BeginServerFrame
 *
 * This will be called once for each server frame, before running
 * any other entities in the world.
 */
void P_BeginServerFrame(edict_t *ent){
	gclient_t *client;

	if(level.intermissiontime)
		return;

	client = ent->client;

	if(ent->groundentity)  // let this be reset each frame as needed
		client->ps.pmove.pm_flags &= ~PMF_PUSHED;

	// run weaponthink if it hasn't been done by a ucmd_t
	if(!client->weapon_thunk && !client->locals.spectator)
		P_WeaponThink(ent);

	client->weapon_thunk = false;

	if(ent->dead){  // check for respawn conditions

		// rounds mode implies last-man-standing, force to spectator immediately if round underway
		if(level.rounds && level.roundtime && level.time >= level.roundtime){
			client->locals.spectator = true;
			P_Respawn(ent, false);
		}
		else if(level.time > client->respawn_time && client->latched_buttons & BUTTON_ATTACK){
			P_Respawn(ent, false);  // all other respawns require a click from the player
		}
	}

	client->latched_buttons = 0;
}
