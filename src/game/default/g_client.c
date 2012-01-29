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
 * G_ClientObituary
 *
 * Make a tasteless death announcement.
 */
static void G_ClientObituary(g_edict_t *self, g_edict_t *inflictor,
		g_edict_t *attacker) {
	unsigned int ff, mod;
	char *message, *message2;
	g_client_t *killer;

	ff = means_of_death & MOD_FRIENDLY_FIRE;
	mod = means_of_death & ~MOD_FRIENDLY_FIRE;
	message = NULL;
	message2 = "";

	killer = attacker->client ? attacker->client : self->client;

	if (!g_level.warmup && frag_log != NULL) { // write frag_log

		fprintf(frag_log, "\\%s\\%s\\\n", killer->persistent.net_name,
				self->client->persistent.net_name);

		fflush(frag_log);
	}

#ifdef HAVE_MYSQL
	if(!g_level.warmup && mysql != NULL) { // insert to db

		snprintf(sql, sizeof(sql), "insert into frag values(null, now(), '%s', '%s', '%s', %d)",
				g_level.name, killer->persistent.sql_name, self->client->persistent.sql_name, mod
		);

		sql[sizeof(sql) - 1] = '\0';
		mysql_query(mysql, sql);
	}
#endif

	switch (mod) {
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

	if (attacker == self) {
		switch (mod) {
		case MOD_GRENADE_SPLASH:
			message = "went pop";
			break;
		case MOD_ROCKET_SPLASH:
			message = "needs glasses";
			break;
		case MOD_LIGHTNING_DISCHARGE:
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

	if (message) { // suicide
		gi.BroadcastPrint(PRINT_MEDIUM, "%s %s.\n",
				self->client->persistent.net_name, message);

		if (g_level.warmup)
			return;

		self->client->persistent.score--;

		if ((g_level.teams || g_level.ctf) && self->client->persistent.team)
			self->client->persistent.team->score--;

		return;
	}

	self->enemy = attacker;
	if (attacker && attacker->client) {
		switch (mod) {
		case MOD_SHOTGUN:
			message = "was gunned down by";
			message2 = "'s pea shooter";
			break;
		case MOD_SUPER_SHOTGUN:
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
		case MOD_GRENADE_SPLASH:
			message = "was shredded by";
			message2 = "'s shrapnel";
			break;
		case MOD_ROCKET:
			message = "was dry-anal-powerslammed by";
			message2 = "'s rocket";
			break;
		case MOD_ROCKET_SPLASH:
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
		case MOD_LIGHTNING_DISCHARGE:
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

		if (message) {

			gi.BroadcastPrint(PRINT_MEDIUM, "%s%s %s %s %s\n",
					(ff ? "^1TEAMKILL^7 " : ""), self->client->persistent.net_name,
					message, attacker->client->persistent.net_name, message2);

			if (g_level.warmup)
				return;

			if (ff)
				attacker->client->persistent.score--;
			else
				attacker->client->persistent.score++;

			if ((g_level.teams || g_level.ctf) && attacker->client->persistent.team) { // handle team scores too
				if (ff)
					attacker->client->persistent.team->score--;
				else
					attacker->client->persistent.team->score++;
			}
		}
	}
}

/*
 * G_ClientPain
 *
 * Pain effects and pain sounds are actually handled in G_ClientDamage. Here we
 * simply inform our attacker that they injured us by playing a hit sound.
 */
static void G_ClientPain(g_edict_t *self, g_edict_t *other, int damage,
		int knockback) {

	if (other && other->client && other != self) { // play a hit sound
		gi.Sound(other, gi.SoundIndex("misc/hit"), ATTN_STATIC);
	}
}

/*
 * G_ClientCorpse_think
 */
static void G_ClientCorpse_think(g_edict_t *ent) {
	const float age = g_level.time - ent->timestamp;

	if (age > 5.0) {
		G_FreeEdict(ent);
		return;
	}

	if (age > 2.0 && ent->ground_entity) {
		ent->s.origin[2] -= 1.0;
	}

	ent->next_think = g_level.time + 0.1;
}

/*
 * G_ClientCorpse
 */
static void G_ClientCorpse(g_edict_t *self) {
	const float f = frand();
	g_edict_t *ent = G_Spawn();

	ent->class_name = "corpse";
	ent->move_type = MOVE_TYPE_TOSS;
	ent->solid = SOLID_NOT;

	VectorScale(PM_MINS, PM_SCALE, ent->mins);
	VectorScale(PM_MAXS, PM_SCALE, ent->maxs);

	VectorCopy(self->velocity, ent->velocity);

	memcpy(&ent->s, &self->s, sizeof(ent->s));
	ent->s.number = ent - g_game.edicts;

	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;

	if (f < 0.33)
		G_SetAnimation(ent, ANIM_BOTH_DEATH1, true);
	else if (f < 0.66)
		G_SetAnimation(ent, ANIM_BOTH_DEATH2, true);
	else
		G_SetAnimation(ent, ANIM_BOTH_DEATH3, true);

	ent->think = G_ClientCorpse_think;
	ent->next_think = g_level.time + 1.0;

	gi.LinkEntity(ent);
}

/*
 * G_ClientDie
 *
 * A client's health is less than or equal to zero. Render death effects, drop
 * certain items we're holding and force the client into a temporary spectator
 * state with the scoreboard shown.
 */
static void G_ClientDie(g_edict_t *self, g_edict_t *inflictor,
		g_edict_t *attacker, int damage, vec3_t point) {

	G_ClientCorpse(self);

	gi.Sound(self, gi.SoundIndex("*death_1"), ATTN_NORM);

	self->client->respawn_time = g_level.time + 1.0;
	self->client->ps.pmove.pm_type = PM_DEAD;

	G_ClientObituary(self, inflictor, attacker);

	if (!g_level.gameplay && !g_level.warmup) // drop weapon
		G_TossWeapon(self);

	self->client->new_weapon = NULL; // reset weapon state
	G_ChangeWeapon(self);

	if (!g_level.gameplay && !g_level.warmup) // drop quad
		G_TossQuadDamage(self);

	if (g_level.ctf && !g_level.warmup) // drop flag in ctf
		G_TossFlag(self);

	G_Score_f(self); // show scores

	self->sv_flags |= SVF_NO_CLIENT;

	self->dead = true;
	self->class_name = "dead";

	self->s.model1 = 0;
	self->s.model2 = 0;
	self->s.model3 = 0;
	self->s.model4 = 0;
	self->s.sound = 0;
	self->s.event = 0;
	self->s.effects = 0;

	self->solid = SOLID_NOT;
	self->take_damage = false;

	gi.LinkEntity(self);

	// TODO: If health is sufficiently low, emit a gib.
	// TODO: We need a gib model ;)

	/*gi.WriteByte(svc_temp_entity);
	 gi.WriteByte(TE_GIB);
	 gi.WritePosition(self->s.origin);
	 gi.Multicast(self->s.origin, MULTICAST_PVS);*/
}

/*
 *  Stocks client's inventory with specified item.  Weapons receive
 *  specified quantity of ammo, while health and armor are set to
 *  the specified quantity.
 */
static void G_Give(g_client_t *client, char *it, int quantity) {
	g_item_t *item;
	int index;

	if (!strcasecmp(it, "Health")) {
		client->persistent.health = quantity;
		return;
	}

	if (!strcasecmp(it, "Armor")) {
		client->persistent.armor = quantity;
		return;
	}

	item = G_FindItem(it);

	if (!item)
		return;

	index = ITEM_INDEX(item);

	if (item->type == ITEM_WEAPON) { // weapons receive quantity as ammo
		client->persistent.inventory[index] = 1;

		item = G_FindItem(item->ammo);
		index = ITEM_INDEX(item);

		if (quantity > -1)
			client->persistent.inventory[index] = quantity;
		else
			client->persistent.inventory[index] = item->quantity;
	} else { // while other items receive quantity directly
		if (quantity > -1)
			client->persistent.inventory[index] = quantity;
		else
			client->persistent.inventory[index] = item->quantity;
	}
}

/*
 * G_GiveLevelLocals
 */
static boolean_t G_GiveLevelLocals(g_client_t *client) {
	char buf[512], *it, *q;
	int quantity;

	if (*g_level.give == '\0')
		return false;

	strncpy(buf, g_level.give, sizeof(buf));

	it = strtok(buf, ",");

	while (true) {

		if (!it)
			break;

		it = Trim(it);

		if (*it != '\0') {

			if ((q = strrchr(it, ' '))) {
				quantity = atoi(q + 1);

				if (quantity > -1) // valid quantity
					*q = 0;
			} else
				quantity = -1;

			G_Give(client, it, quantity);
		}

		it = strtok(NULL, ",");
	}

	return true;
}

/*
 * G_InitClientLocals
 */
static void G_InitClientLocals(g_client_t *client) {
	g_item_t *item;
	int i;

	// clear inventory
	for (i = 0; i < MAX_ITEMS; i++)
		client->persistent.inventory[i] = 0;

	// set max inventory levels
	client->persistent.health = 100;
	client->persistent.max_health = 100;

	client->persistent.armor = 0;
	client->persistent.max_armor = 200;

	client->persistent.max_shells = 80;
	client->persistent.max_bullets = 200;
	client->persistent.max_grenades = 50;
	client->persistent.max_rockets = 50;
	client->persistent.max_cells = 200;
	client->persistent.max_bolts = 100;
	client->persistent.max_slugs = 50;
	client->persistent.max_nukes = 10;

	// instagib gets railgun and slugs, both in normal mode and warmup
	if (g_level.gameplay == INSTAGIB) {
		G_Give(client, "Railgun", 1000);
		item = G_FindItem("Railgun");
	}
	// arena or dm warmup yields all weapons, health, etc..
	else if ((g_level.gameplay == ARENA) || g_level.warmup) {
		G_Give(client, "Railgun", 50);
		G_Give(client, "Lightning", 200);
		G_Give(client, "Hyperblaster", 200);
		G_Give(client, "Rocket Launcher", 50);
		G_Give(client, "Grenade Launcher", 50);
		G_Give(client, "Machinegun", 200);
		G_Give(client, "Super Shotgun", 80);
		G_Give(client, "Shotgun", 80);

		G_Give(client, "Armor", 200);

		item = G_FindItem("Rocket Launcher");
	}
	// dm gets shotgun and 10 shots
	else {
		G_Give(client, "Shotgun", 10);
		item = G_FindItem("Shotgun");
	}

	if (G_GiveLevelLocals(client)) { // use the best weapon we were given by level
		G_UseBestWeapon(client);
		client->persistent.weapon = client->new_weapon;
	} else
		// or use best given by gameplay
		client->persistent.weapon = item;

	// clean up weapon state
	client->persistent.last_weapon = NULL;
	client->new_weapon = NULL;
}

/*
 * G_EnemyRangeFromSpot
 *
 * Returns the distance to the nearest enemy from the given spot
 */
static float G_EnemyRangeFromSpot(g_edict_t *ent, g_edict_t *spot) {
	g_edict_t *player;
	float dist, best_dist;
	vec3_t v;
	int n;

	best_dist = 9999999.0;

	for (n = 1; n <= sv_max_clients->integer; n++) {
		player = &g_game.edicts[n];

		if (!player->in_use)
			continue;

		if (player->health <= 0)
			continue;

		if (player->client->persistent.spectator)
			continue;

		VectorSubtract(spot->s.origin, player->s.origin, v);
		dist = VectorLength(v);

		if (g_level.teams || g_level.ctf) { // avoid collision with team mates

			if (player->client->persistent.team == ent->client->persistent.team) {
				if (dist > 64.0) // if they're far away, ignore them
					continue;
			}
		}

		if (dist < best_dist)
			best_dist = dist;
	}

	return best_dist;
}

/*
 * P_SelectRandomSpawnPoint
 */
static g_edict_t *G_SelectRandomSpawnPoint(g_edict_t *ent,
		const char *class_name) {
	g_edict_t *spot;
	int count = 0;

	spot = NULL;

	while ((spot = G_Find(spot, FOFS(class_name), class_name)) != NULL)
		count++;

	if (!count)
		return NULL;

	count = rand() % count;

	while (count-- >= 0)
		spot = G_Find(spot, FOFS(class_name), class_name);

	return spot;
}

/*
 * P_SelectFarthestSpawnPoint
 */
static g_edict_t *G_SelectFarthestSpawnPoint(g_edict_t *ent,
		const char *class_name) {
	g_edict_t *spot, *best_spot;
	float dist, best_dist;

	spot = best_spot = NULL;
	best_dist = 0.0;

	while ((spot = G_Find(spot, FOFS(class_name), class_name)) != NULL) {

		dist = G_EnemyRangeFromSpot(ent, spot);

		if (dist > best_dist) {
			best_spot = spot;
			best_dist = dist;
		}
	}

	if (best_spot)
		return best_spot;

	// if there is an enemy just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	spot = G_Find(NULL, FOFS(class_name), class_name);

	return spot;
}

/*
 * G_SelectDeathmatchSpawnPoint
 */
static g_edict_t *G_SelectDeathmatchSpawnPoint(g_edict_t *ent) {

	if (g_spawn_farthest->value)
		return G_SelectFarthestSpawnPoint(ent, "info_player_deathmatch");

	return G_SelectRandomSpawnPoint(ent, "info_player_deathmatch");
}

/*
 * G_SelectCaptureSpawnPoint
 */
static g_edict_t *G_SelectCaptureSpawnPoint(g_edict_t *ent) {
	char *c;

	if (!ent->client->persistent.team)
		return NULL;

	c = ent->client->persistent.team == &g_team_good ? "info_player_team1"
			: "info_player_team2";

	if (g_spawn_farthest->value)
		return G_SelectFarthestSpawnPoint(ent, c);

	return G_SelectRandomSpawnPoint(ent, c);
}

/*
 * G_SelectSpawnPoint
 *
 * Chooses a player start, deathmatch start, etc
 */
static void G_SelectSpawnPoint(g_edict_t *ent, vec3_t origin, vec3_t angles) {
	g_edict_t *spot = NULL;

	if (g_level.teams || g_level.ctf) // try teams/ctf spawns first if applicable
		spot = G_SelectCaptureSpawnPoint(ent);

	if (!spot) // fall back on dm spawns (e.g ctf games on dm maps)
		spot = G_SelectDeathmatchSpawnPoint(ent);

	// and lastly fall back on single player start
	if (!spot) {
		while ((spot = G_Find(spot, FOFS(class_name), "info_player_start"))
				!= NULL) {
			if (!spot->target_name) // hopefully without a target
				break;
		}

		if (!spot) { // last resort, find any
			if ((spot = G_Find(spot, FOFS(class_name), "info_player_start"))
					== NULL)
				gi.Error("P_SelectSpawnPoint: Couldn't find spawn point.");
		}
	}

	VectorCopy(spot->s.origin, origin);
	origin[2] += 12;
	VectorCopy(spot->s.angles, angles);
}

/*
 * G_ClientRespawn_
 *
 * The grunt work of putting the client into the server on [re]spawn.
 */
static void G_ClientRespawn_(g_edict_t *ent) {
	vec3_t spawn_origin, spawn_angles, old_angles;
	float height;
	g_client_t *cl;
	g_client_persistent_t locals;
	int i;

	// find a spawn point
	G_SelectSpawnPoint(ent, spawn_origin, spawn_angles);

	cl = ent->client;

	// retain last angles for delta
	VectorCopy(ent->client->cmd_angles, old_angles);

	// reset inventory, health, etc
	G_InitClientLocals(cl);

	// clear everything but locals
	locals = cl->persistent;
	memset(cl, 0, sizeof(*cl));
	cl->persistent = locals;

	// clear entity values
	VectorScale(PM_MINS, PM_SCALE, ent->mins);
	VectorScale(PM_MAXS, PM_SCALE, ent->maxs);
	height = ent->maxs[2] - ent->mins[2];

	ent->ground_entity = NULL;
	ent->take_damage = true;
	ent->move_type = MOVE_TYPE_WALK;
	ent->view_height = ent->mins[2] + (height * 0.75);
	ent->class_name = "player";
	ent->mass = 200.0;
	ent->solid = SOLID_BOX;
	ent->dead = false;
	ent->clip_mask = MASK_PLAYER_SOLID;
	ent->pain = G_ClientPain;
	ent->die = G_ClientDie;
	ent->water_level = 0;
	ent->water_type = 0;
	ent->sv_flags = 0;

	// copy these back once they have been set in locals
	ent->health = ent->client->persistent.health;
	ent->max_health = ent->client->persistent.max_health;

	VectorClear(ent->velocity);
	ent->velocity[2] = 150.0;

	// clear player state values
	memset(&ent->client->ps, 0, sizeof(cl->ps));

	cl->ps.pmove.origin[0] = spawn_origin[0] * 8.0;
	cl->ps.pmove.origin[1] = spawn_origin[1] * 8.0;
	cl->ps.pmove.origin[2] = spawn_origin[2] * 8.0;

	// clear entity state values
	ent->s.effects = 0;
	ent->s.model1 = 0xff; // use the client info model
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;
	ent->s.client = ent - g_game.edicts - 1;

	G_SetAnimation(ent, ANIM_TORSO_STAND1, true);
	G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

	VectorCopy(spawn_origin, ent->s.origin);
	VectorCopy(ent->s.origin, ent->s.old_origin);

	// set the delta angle of the spawn point
	for (i = 0; i < 3; i++) {
		cl->ps.pmove.delta_angles[i]
				= ANGLE2SHORT(spawn_angles[i] - old_angles[i]);
	}

	VectorClear(cl->cmd_angles);
	VectorClear(cl->angles);
	VectorClear(ent->s.angles);

	// spawn a spectator
	if (cl->persistent.spectator) {
		cl->chase_target = NULL;

		cl->persistent.weapon = NULL;
		cl->persistent.team = NULL;
		cl->persistent.ready = false;

		ent->move_type = MOVE_TYPE_NO_CLIP;
		ent->solid = SOLID_NOT;
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->take_damage = false;

		gi.LinkEntity(ent);
		return;
	}

	// or spawn a player
	ent->s.event = EV_TELEPORT;

	// hold in place briefly
	cl->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	cl->ps.pmove.pm_time = 20;

	cl->persistent.match_num = g_level.match_num;
	cl->persistent.round_num = g_level.round_num;

	gi.UnlinkEntity(ent);

	G_KillBox(ent); // telefrag anyone in our spot

	gi.LinkEntity(ent);

	// force the current weapon up
	cl->new_weapon = cl->persistent.weapon;
	G_ChangeWeapon(ent);
}

/*
 * G_ClientRespawn
 *
 * In this case, voluntary means that the client has explicitly requested
 * a respawn by changing their spectator status.
 */
void G_ClientRespawn(g_edict_t *ent, boolean_t voluntary) {

	G_ClientRespawn_(ent);

	// clear scores and match/round on voluntary changes
	if (ent->client->persistent.spectator && voluntary) {
		ent->client->persistent.score = ent->client->persistent.captures = 0;
		ent->client->persistent.match_num = ent->client->persistent.round_num = 0;
	}

	ent->client->respawn_time = g_level.time;

	if (!voluntary) // don't announce involuntary spectator changes
		return;

	if (ent->client->persistent.spectator)
		gi.BroadcastPrint(PRINT_HIGH, "%s likes to watch\n",
				ent->client->persistent.net_name);
	else if (ent->client->persistent.team)
		gi.BroadcastPrint(PRINT_HIGH, "%s has joined %s\n",
				ent->client->persistent.net_name, ent->client->persistent.team->name);
	else
		gi.BroadcastPrint(PRINT_HIGH, "%s wants some\n",
				ent->client->persistent.net_name);
}

/*
 * G_ClientBegin
 *
 * Called when a client has finished connecting, and is ready
 * to be placed into the game.  This will happen every level load.
 */
void G_ClientBegin(g_edict_t *ent) {
	char welcome[256];

	int player_num = ent - g_game.edicts - 1;

	ent->client = g_game.clients + player_num;

	G_InitEdict(ent);

	G_InitClientLocals(ent->client);

	VectorClear(ent->client->cmd_angles);
	ent->client->persistent.first_frame = g_level.frame_num;

	// force spectator if match or rounds
	if (g_level.match || g_level.rounds)
		ent->client->persistent.spectator = true;
	else if (g_level.teams || g_level.ctf) {
		if (g_auto_join->value)
			G_AddClientToTeam(ent, G_SmallestTeam()->name);
		else
			ent->client->persistent.spectator = true;
	}

	// spawn them in
	G_ClientRespawn(ent, true);

	if (g_level.intermission_time) {
		G_ClientToIntermission(ent);
	} else {
		memset(welcome, 0, sizeof(welcome));

		snprintf(welcome, sizeof(welcome),
				"^2Welcome to ^7http://quake2world.net\n"
				"^2Gameplay is ^1%s\n", G_GameplayName(g_level.gameplay));

		if (g_level.teams)
			strncat(welcome, "^2Teams are enabled\n", sizeof(welcome));

		if (g_level.ctf)
			strncat(welcome, "^2CTF is enabled\n", sizeof(welcome));

		if (g_voting->value)
			strncat(welcome, "^2Voting is allowed\n", sizeof(welcome));

		gi.ClientCenterPrint(ent, "%s", welcome);
	}

	// make sure all view stuff is valid
	G_ClientEndFrame(ent);

	srand(time(NULL)); // set random seed
}

/*
 * G_ClientUserInfoChanged
 */
void G_ClientUserInfoChanged(g_edict_t *ent, const char *user_info) {
	const char *s;
	char *c;
	char name[MAX_NET_NAME];
	int player_num, i;
	boolean_t color;
	g_client_t *cl;

	// check for malformed or illegal info strings
	if (!ValidateUserInfo(user_info)) {
		user_info = "\\name\\newbie\\skin\\qforcer/enforcer";
	}

	cl = ent->client;

	// set name, use a temp buffer to compute length and crutch up bad names
	s = GetUserInfo(user_info, "name");

	strncpy(name, s, sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	color = false;
	c = name;
	i = 0;

	// trim to 15 printable chars
	while (i < 15) {

		if (!*c)
			break;

		if (IS_COLOR(c)) {
			color = true;
			c += 2;
			continue;
		}

		c++;
		i++;
	}
	name[c - name] = 0;

	if (!i) // name had nothing printable
		strcpy(name, "newbie");

	if (color) // reset to white
		strcat(name, "^7");

	if (strncmp(cl->persistent.net_name, name, sizeof(cl->persistent.net_name))) {

		if (*cl->persistent.net_name != '\0')
			gi.BroadcastPrint(PRINT_MEDIUM, "%s changed name to %s\n",
					cl->persistent.net_name, name);

		strncpy(cl->persistent.net_name, name, sizeof(cl->persistent.net_name) - 1);
		cl->persistent.net_name[sizeof(cl->persistent.net_name) - 1] = 0;
	}

#ifdef HAVE_MYSQL
	if(mysql != NULL) { // escape name for safe db insertions

		StripColor(cl->persistent.net_name, name);

		mysql_real_escape_string(mysql, name, cl->persistent.sql_name,
				sizeof(cl->persistent.sql_name));
	}
#endif

	// set skin
	if ((g_level.teams || g_level.ctf) && cl->persistent.team) // players must use team_skin to change
		s = cl->persistent.team->skin;
	else
		s = GetUserInfo(user_info, "skin");

	if (*s != '\0') // something valid-ish was provided
		strncpy(cl->persistent.skin, s, sizeof(cl->persistent.skin) - 1);
	else {
		strcpy(cl->persistent.skin, "qforcer/enforcer");
		cl->persistent.skin[sizeof(cl->persistent.skin) - 1] = 0;
	}

	// set color
	s = GetUserInfo(user_info, "color");
	cl->persistent.color = ColorByName(s, 243);

	player_num = ent - g_game.edicts - 1;

	// combine name and skin into a config_string
	gi.ConfigString(CS_CLIENT_INFO + player_num,
			va("%s\\%s", cl->persistent.net_name, cl->persistent.skin));

	// save off the user_info in case we want to check something later
	strncpy(ent->client->persistent.user_info, user_info, sizeof(ent->client->persistent.user_info) - 1);
}

/*
 * G_ClientConnect
 *
 * Called when a player begins connecting to the server.
 * The game can refuse entrance to a client by returning false.
 * If the client is allowed, the connection process will continue
 * and eventually get to G_Begin()
 * Changing levels will NOT cause this to be called again.
 */
boolean_t G_ClientConnect(g_edict_t *ent, char *user_info) {

	// check password
	const char *value = GetUserInfo(user_info, "password");
	if (*password->string && strcmp(password->string, "none") && strcmp(
			password->string, value)) {
		SetUserInfo(user_info, "rejmsg", "Password required or incorrect.");
		return false;
	}

	// they can connect
	ent->client = g_game.clients + (ent - g_game.edicts - 1);

	// clean up locals things which are not reset on spawns
	ent->client->persistent.score = 0;
	ent->client->persistent.team = NULL;
	ent->client->persistent.vote = VOTE_NO_OP;
	ent->client->persistent.spectator = false;
	ent->client->persistent.net_name[0] = 0;

	// set name, skin, etc..
	G_ClientUserInfoChanged(ent, user_info);

	if (sv_max_clients->integer > 1)
		gi.BroadcastPrint(PRINT_HIGH, "%s connected\n",
				ent->client->persistent.net_name);

	ent->sv_flags = 0; // make sure we start with known default
	return true;
}

/*
 * G_ClientDisconnect
 *
 * Called when a player drops from the server.  Not be called between levels.
 */
void G_ClientDisconnect(g_edict_t *ent) {
	int player_num;

	if (!ent->client)
		return;

	G_TossQuadDamage(ent);
	G_TossFlag(ent);

	gi.BroadcastPrint(PRINT_HIGH, "%s bitched out\n",
			ent->client->persistent.net_name);

	// send effect
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_LOGOUT);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);

	gi.UnlinkEntity(ent);

	ent->client->persistent.user_info[0] = 0;

	ent->in_use = false;
	ent->solid = SOLID_NOT;
	ent->s.model1 = 0;
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;
	ent->class_name = "disconnected";

	player_num = ent - g_game.edicts - 1;
	gi.ConfigString(CS_CLIENT_INFO + player_num, "");
}

/*
 * G_ClientMoveTrace
 *
 * Ignore ourselves, clipping to the correct mask based on our status.
 */
static c_trace_t G_ClientMoveTrace(vec3_t start, vec3_t mins, vec3_t maxs,
		vec3_t end) {
	g_edict_t *self = g_level.current_entity;

	if (g_level.current_entity->health > 0)
		return gi.Trace(start, mins, maxs, end, self, MASK_PLAYER_SOLID);
	else
		return gi.Trace(start, mins, maxs, end, self, MASK_DEAD_SOLID);
}

/*
 * G_InventoryThink
 *
 * Expire any items which are time-sensitive.
 */
static void G_ClientInventoryThink(g_edict_t *ent) {

	if (ent->client->persistent.inventory[quad_damage_index]) { // if they have quad

		if (ent->client->quad_damage_time < g_level.time) { // expire it

			ent->client->quad_damage_time = 0.0;
			ent->client->persistent.inventory[quad_damage_index] = 0;

			gi.Sound(ent, gi.SoundIndex("quad/expire"), ATTN_NORM);

			ent->s.effects &= ~EF_QUAD;
		}
	}

	// other power-ups and things can be timed out here as well
}

/*
 * G_ClientThink
 *
 * This will be called once for each client frame, which will usually be a
 * couple times for each server frame.
 */
void G_ClientThink(g_edict_t *ent, user_cmd_t *ucmd) {
	g_client_t *client;
	g_edict_t *other;
	int i, j;

	g_level.current_entity = ent;
	client = ent->client;

	if (g_level.intermission_time) {
		client->ps.pmove.pm_type = PM_FREEZE;
		return;
	}

	if (client->chase_target) { // ensure chase is valid

		client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

		if (!client->chase_target->in_use
				|| client->chase_target->client->persistent.spectator) {

			other = client->chase_target;

			G_ChaseNext(ent);

			if (client->chase_target == other) { // no one to chase
				client->chase_target = NULL;
			}
		}
	}

	if (!client->chase_target) { // set up for pmove
		pm_move_t pm;

		client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

		if (ent->move_type == MOVE_TYPE_NO_CLIP)
			client->ps.pmove.pm_type = PM_SPECTATOR;
		else if (ent->s.model1 != 255 || ent->dead)
			client->ps.pmove.pm_type = PM_DEAD;
		else
			client->ps.pmove.pm_type = PM_NORMAL;

		client->ps.pmove.gravity = g_level.gravity;

		memset(&pm, 0, sizeof(pm));
		pm.s = client->ps.pmove;

		for (i = 0; i < 3; i++) {
			pm.s.origin[i] = ent->s.origin[i] * 8.0;
			pm.s.velocity[i] = ent->velocity[i] * 8.0;
		}

		pm.cmd = *ucmd;

		pm.Trace = G_ClientMoveTrace; // adds default params
		pm.PointContents = gi.PointContents;

		// perform a pmove
		gi.Pmove(&pm);

		// save results of pmove
		client->ps.pmove = pm.s;

		for (i = 0; i < 3; i++) {
			ent->s.origin[i] = pm.s.origin[i] * 0.125;
			ent->velocity[i] = pm.s.velocity[i] * 0.125;
		}

		VectorCopy(pm.mins, ent->mins);
		VectorCopy(pm.maxs, ent->maxs);

		client->cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		client->cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		client->cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

		// check for jump, play randomized sound
		if (ent->ground_entity && !pm.ground_entity && (pm.cmd.up >= 10)
				&& (pm.water_level == 0) && client->jump_time < g_level.time
				- 0.2) {

			vec3_t angles, forward, velocity;
			float speed;

			VectorSet(angles, 0.0, ent->s.angles[YAW], 0.0);
			AngleVectors(angles, forward, NULL, NULL);

			VectorCopy(ent->velocity, velocity);
			velocity[2] = 0.0;

			speed = VectorNormalize(velocity);

			if (DotProduct(velocity, forward) < 0.0 && speed > 200.0)
				G_SetAnimation(ent, ANIM_LEGS_JUMP2, true);
			else
				G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

			ent->s.event = EV_CLIENT_JUMP;
			client->jump_time = g_level.time;
		}

		ent->view_height = pm.view_height;
		ent->water_level = pm.water_level;
		ent->water_type = pm.water_type;
		ent->ground_entity = pm.ground_entity;

		if (ent->ground_entity)
			ent->ground_entity_link_count = ent->ground_entity->link_count;

		VectorCopy(pm.angles, client->angles);
		VectorCopy(pm.angles, client->ps.angles);

		gi.LinkEntity(ent);

		// touch jump pads, hurt brushes, etc..
		if (ent->move_type != MOVE_TYPE_NO_CLIP && ent->health > 0)
			G_TouchTriggers(ent);

		// touch other objects
		for (i = 0; i < pm.num_touch; i++) {

			other = pm.touch_ents[i];

			for (j = 0; j < i; j++)
				if (pm.touch_ents[j] == other)
					break;

			if (j != i)
				continue; // duplicated

			if (!other->touch)
				continue;

			other->touch(other, ent, NULL, NULL);
		}
	}

	client->old_buttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->old_buttons;

	// fire weapon if requested
	if (client->latched_buttons & BUTTON_ATTACK) {
		if (client->persistent.spectator) {

			client->latched_buttons = 0;

			if (client->chase_target) { // toggle chase camera
				client->chase_target = NULL;
				client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			} else {
				G_ChaseTarget(ent);
			}
		} else if (client->weapon_think_time < g_level.time) {
			G_WeaponThink(ent);
		}
	}

	// update chase camera if being followed
	for (i = 1; i <= sv_max_clients->integer; i++) {

		other = g_game.edicts + i;

		if (other->in_use && other->client->chase_target == ent) {
			G_ChaseThink(other);
		}
	}

	G_ClientInventoryThink(ent);
}

/*
 * G_ClientBeginFrame
 *
 * This will be called once for each server frame, before running
 * any other entities in the world.
 */
void G_ClientBeginFrame(g_edict_t *ent) {
	g_client_t *client;

	if (g_level.intermission_time)
		return;

	client = ent->client;

	if (ent->ground_entity) // let this be reset each frame as needed
		client->ps.pmove.pm_flags &= ~PMF_PUSHED;

	// run weapon think if it hasn't been done by a command
	if (client->weapon_think_time < g_level.time && !client->persistent.spectator)
		G_WeaponThink(ent);

	if (ent->dead) { // check for respawn conditions

		// rounds mode implies last-man-standing, force to spectator immediately if round underway
		if (g_level.rounds && g_level.round_time && g_level.time
				>= g_level.round_time) {
			client->persistent.spectator = true;
			G_ClientRespawn(ent, false);
		} else if (g_level.time > client->respawn_time
				&& (client->latched_buttons & BUTTON_ATTACK)) {
			G_ClientRespawn(ent, false); // all other respawns require a click from the player
		}
	}

	client->latched_buttons = 0;
}
