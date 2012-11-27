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

#ifndef __G_GAME_TYPES_H__ // don't collide with glib.h
#define __G_GAME_TYPES_H__

#include "game/game.h"

// server commands sent directly to the client game
typedef enum {
	SV_CMD_CENTER_PRINT = SV_CMD_CGAME,
	SV_CMD_MUZZLE_FLASH,
	SV_CMD_SCORES,
	SV_CMD_TEMP_ENTITY,
	SV_CMD_FOOBAR
// add custom commands here
} g_sv_cmd_t;

// scores are transmitted as binary to the client game module
typedef struct {
	uint16_t player_num;
	uint16_t ping;
	byte team;
	byte color;
	int16_t score;
	int16_t captures;
	byte flags;
} player_score_t;

//scores flags
#define SCORES_NOT_READY	(1 << 0)
#define SCORES_FLAG			(1 << 1)

// ConfigStrings that are local to the game and client game
#define CS_GAMEPLAY			(CS_GENERAL + 0) // gameplay string
#define CS_TEAMS			(CS_GENERAL + 1) // are teams enabled?
#define CS_CTF				(CS_GENERAL + 2) // is capture enabled?
#define CS_MATCH			(CS_GENERAL + 3) // is match mode enabled?
#define CS_ROUNDS			(CS_GENERAL + 4) // are rounds enabled?
#define CS_TEAM_GOOD		(CS_GENERAL + 5) // good team name
#define CS_TEAM_EVIL		(CS_GENERAL + 6) // evil team name
#define CS_TIME				(CS_GENERAL + 7) // level or match timer
#define CS_ROUND			(CS_GENERAL + 8) // round number
#define CS_VOTE				(CS_GENERAL + 9) // vote string\yes count\no count

// player_state->stats[] indexes
#define STAT_AMMO			0
#define STAT_AMMO_ICON		1
#define STAT_AMMO_LOW		2
#define STAT_ARMOR			3
#define STAT_ARMOR_ICON		4
#define STAT_CAPTURES		5
#define STAT_CHASE			6
#define STAT_DAMAGE_ARMOR	7
#define STAT_DAMAGE_HEALTH	8
#define STAT_DAMAGE_INFLICT	9
#define STAT_FRAGS			10
#define STAT_HEALTH			11
#define STAT_HEALTH_ICON	12
#define STAT_PICKUP_ICON	13
#define STAT_PICKUP_STRING	14
#define STAT_READY			15
#define STAT_ROUND			16
#define STAT_SCORES			17
#define STAT_SPECTATOR		18
#define STAT_TEAM			19
#define STAT_TIME			20
#define STAT_VOTE			21
#define STAT_WEAPON			22
#define STAT_WEAPON_ICON	23
// mods may define new STATs, up to 31

#define STAT_TOGGLE_BIT		0x8000 // used to force a stats field update

#if defined(__G_LOCAL_H__)

// edict->spawnflags
#define SF_ITEM_TRIGGER			0x00000001
#define SF_ITEM_NO_TOUCH		0x00000002
#define SF_ITEM_HOVER			0x00000004

// we keep these around for compatibility with legacy levels
#define SF_NOT_EASY				0x00000100
#define SF_NOT_MEDIUM			0x00000200
#define SF_NOT_HARD				0x00000400
#define SF_NOT_DEATHMATCH		0x00000800
#define SF_NOT_COOP				0x00001000

#define SF_ITEM_DROPPED			0x00010000
#define SF_ITEM_TARGETS_USED	0x00020000

// edict->flags
#define FL_FLY					0x00000001
#define FL_SWIM					0x00000002  // implied immunity to drowning
#define FL_GOD_MODE				0x00000004
#define FL_TEAM_SLAVE			0x00000008  // not the first on the team
#define FL_RESPAWN				0x80000000  // used for item respawning

// ammo types
typedef enum {
	AMMO_NONE,
	AMMO_SHELLS,
	AMMO_BULLETS,
	AMMO_GRENADES,
	AMMO_ROCKETS,
	AMMO_CELLS,
	AMMO_BOLTS,
	AMMO_SLUGS,
	AMMO_NUKES
}g_ammo_t;

// armor types
typedef enum {
	ARMOR_NONE, ARMOR_JACKET, ARMOR_COMBAT, ARMOR_BODY, ARMOR_SHARD
}g_armor_t;

// health types
typedef enum {
	HEALTH_NONE, HEALTH_SMALL, HEALTH_MEDIUM, HEALTH_LARGE, HEALTH_MEGA
}g_health_t;

// edict->move_type values
typedef enum {
	MOVE_TYPE_NONE, // never moves
	MOVE_TYPE_NO_CLIP, // origin and angles change with no interaction
	MOVE_TYPE_PUSH, // no clip to world, push on box contact
	MOVE_TYPE_STOP, // no clip to world, stops on box contact

	MOVE_TYPE_WALK, // gravity
	MOVE_TYPE_FLY,
	MOVE_TYPE_TOSS,
	// gravity
}g_move_type_t;

// a synonym for readability
#define MOVE_TYPE_THINK MOVE_TYPE_NONE

// gitem_t->flags
typedef enum {
	ITEM_WEAPON, ITEM_AMMO, ITEM_ARMOR, ITEM_FLAG, ITEM_HEALTH, ITEM_POWERUP
}g_item_type_t;

typedef struct g_item_s {
	char *class_name; // spawning name
	bool (*pickup)(struct g_edict_s *ent, struct g_edict_s *other);
	void (*use)(struct g_edict_s *ent, struct g_item_s *item);
	void (*drop)(struct g_edict_s *ent, struct g_item_s *item);
	void (*weapon_think)(struct g_edict_s *ent);
	char *pickup_sound;
	char *model;
	uint32_t effects;

	// client side info
	char *icon;
	char *pickup_name; // for printing on pickup

	uint16_t quantity; // for ammo how much, for weapons how much is used per shot
	char *ammo; // for weapons
	g_item_type_t type; // g_item_type_t, see above
	uint16_t tag; // type-specific flags

	char *precaches; // string of all models, sounds, and images this item will use
}g_item_t;

// override quake2 items for legacy maps
typedef struct {
	char *old;
	char *new;
}g_override_t;

extern g_override_t g_overrides[];

// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actually present
// in g_edict_t at runtime
typedef struct {
	// world vars, we use strings to avoid ambiguity between 0 and unset
	char *sky;
	char *weather;
	char *gravity;
	char *gameplay;
	char *teams;
	char *ctf;
	char *match;
	char *rounds;
	char *frag_limit;
	char *round_limit;
	char *capture_limit;
	char *time_limit;
	char *give;
	char *music;

	int32_t lip;
	int32_t distance;
	int32_t height;
	char *noise;
	float pause_time;
	char *item;
}g_spawn_temp_t;

#define FOFS(x) (ptrdiff_t)&(((g_edict_t *)0)->x)
#define SOFS(x) (ptrdiff_t)&(((g_spawn_temp_t *)0)->x)

typedef enum {
	STATE_TOP, STATE_BOTTOM, STATE_UP, STATE_DOWN
}g_move_state_t;

typedef struct {
	// fixed data
	vec3_t start_origin;
	vec3_t start_angles;
	vec3_t end_origin;
	vec3_t end_angles;

	uint16_t sound_start;
	uint16_t sound_middle;
	uint16_t sound_end;

	float accel;
	float speed;
	float decel;
	float distance;

	float wait;

	// state data
	g_move_state_t state;
	vec3_t dir;
	float current_speed;
	float move_speed;
	float next_speed;
	float remaining_distance;
	float decel_distance;
	void (*done)(g_edict_t *);
}g_move_info_t;

// this structure is left intact through an entire game
typedef struct {
	g_edict_t *edicts; // [g_max_entities]
	g_client_t *clients; // [sv_max_clients]

	g_spawn_temp_t spawn;

	uint16_t num_items;
	uint16_t num_overrides;
}g_game_t;

extern g_game_t g_game;

// this structure is cleared as each map is entered
typedef struct {
	uint32_t frame_num;
	uint32_t time;

	char title[MAX_STRING_CHARS]; // the descriptive name (Stress Fractures, etc)
	char name[MAX_QPATH]; // the server name (fractures, etc)
	int32_t gravity; // defaults to 800
	int32_t gameplay; // DEATHMATCH, INSTAGIB, ARENA
	int32_t teams;
	int32_t ctf;
	int32_t match;
	int32_t rounds;
	int32_t frag_limit;
	int32_t round_limit;
	int32_t capture_limit;
	uint32_t time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];

	// intermission state
	uint32_t intermission_time; // time intermission started
	vec3_t intermission_origin;
	vec3_t intermission_angle;
	const char *changemap;

	bool warmup; // shared by match and round

	bool start_match;
	uint32_t match_time; // time match started
	uint32_t match_num;

	bool start_round;
	uint32_t round_time; // time round started
	uint32_t round_num;

	char vote_cmd[64]; // current vote in question
	uint32_t votes[3]; // current vote tallies
	uint32_t vote_time; // time vote started

	g_edict_t *current_entity; // entity running from G_RunFrame
}g_level_t;

// means of death
#define MOD_UNKNOWN					0
#define MOD_BLASTER					1
#define MOD_SHOTGUN					2
#define MOD_SUPER_SHOTGUN			3
#define MOD_MACHINEGUN				4
#define MOD_GRENADE					5
#define MOD_GRENADE_SPLASH			6
#define MOD_ROCKET					7
#define MOD_ROCKET_SPLASH			8
#define MOD_HYPERBLASTER			9
#define MOD_LIGHTNING				10
#define MOD_LIGHTNING_DISCHARGE		11
#define MOD_RAILGUN					12
#define MOD_BFG_LASER				13
#define MOD_BFG_BLAST				14
#define MOD_WATER					15
#define MOD_SLIME					16
#define MOD_LAVA					17
#define MOD_CRUSH					18
#define MOD_TELEFRAG				19
#define MOD_FALLING					20
#define MOD_SUICIDE					21
#define MOD_EXPLOSIVE				22
#define MOD_TRIGGER_HURT			23
#define MOD_FRIENDLY_FIRE			0x8000000

#define MAX_MAP_LIST_ELTS 64
#define MAP_LIST_WEIGHT 16384

// voting
#define MAX_VOTE_TIME 60000
#define VOTE_MAJORITY 0.51

typedef enum {
	VOTE_NO_OP, VOTE_YES, VOTE_NO
}g_vote_t;

// gameplay modes
typedef enum {
	DEFAULT, DEATHMATCH, INSTAGIB, ARENA
}g_gameplay_t;

#define TEAM_CHANGE_TIME 5.0

// damage flags
#define DAMAGE_RADIUS			0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR			0x00000002  // armor does not protect from this damage
#define DAMAGE_ENERGY			0x00000004  // damage is from an energy based weapon
#define DAMAGE_BULLET			0x00000008  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION	0x00000010  // armor and godmode have no effect
#define MAX_NET_NAME 64

// teams
typedef struct {
	char name[16];
	char skin[32];
	int16_t score;
	int16_t captures;
	uint32_t name_time; // prevent change spamming
	uint32_t skin_time;
}g_team_t;

// client data that persists through respawns
typedef struct {
	uint32_t first_frame; // g_level.frame_num the client entered the game

	char user_info[MAX_USER_INFO_STRING];
	char net_name[MAX_NET_NAME];
	char sql_name[20];
	char skin[32];
	int16_t score;
	int16_t captures;

	int16_t health;
	int16_t max_health;

	int16_t armor;
	int16_t max_armor;

	int16_t inventory[MAX_ITEMS];

	// ammo capacities
	int16_t max_shells;
	int16_t max_bullets;
	int16_t max_grenades;
	int16_t max_rockets;
	int16_t max_cells;
	int16_t max_bolts;
	int16_t max_slugs;
	int16_t max_nukes;

	g_item_t *weapon;
	g_item_t *last_weapon;

	bool spectator; // client is a spectator
	bool ready; // ready

	g_team_t *team; // current team (good/evil)
	g_vote_t vote; // current vote (yes/no)
	uint32_t match_num; // most recent match
	uint32_t round_num; // most recent arena round
	int32_t color; // weapon effect colors
}g_client_persistent_t;

// this structure is cleared on each respawn
struct g_client_s {
	// known to server
	player_state_t ps; // communicated by server to clients
	uint32_t ping;

	// private to game

	user_cmd_t cmd;

	g_client_persistent_t persistent;

	bool show_scores; // sets layout bit mask in player state
	uint32_t scores_time; // eligible for scores when time > this

	uint16_t ammo_index;

	uint32_t buttons;
	uint32_t old_buttons;
	uint32_t latched_buttons;

	uint32_t weapon_think_time; // time when the weapon think was called
	uint32_t weapon_fire_time; // can fire when time > this
	g_item_t *new_weapon;

	int16_t damage_armor; // damage absorbed by armor
	int16_t damage_health; // damage taken out of health
	int16_t damage_inflicted; // damage done to other clients

	float speed; // x/y speed after moving
	vec3_t angles; // aiming direction
	vec3_t forward, right, up; // aiming direction vectors
	vec3_t cmd_angles; // angles sent over in the last command

	uint32_t respawn_time; // eligible for respawn when time > this
	uint32_t respawn_protection_time; // respawn protected till this time
	uint32_t ground_time; // last touched ground whence
	uint32_t drown_time; // eligible for drowning damage when time > this
	uint32_t sizzle_time; // eligible for sizzle damage when time > this
	uint32_t land_time; // eligible for landing event when time > this
	uint32_t jump_time; // eligible for jump when time > this
	uint32_t pain_time; // eligible for pain sound when time > this
	uint32_t footstep_time; // play a footstep when time > this
	uint32_t animation1_time; // eligible for animation update when time > this
	uint32_t animation2_time; // eligible for animation update when time > this

	uint32_t pickup_msg_time; // display message until time > this

	uint32_t chat_time; // can chat when time > this
	bool muted;

	uint32_t quad_damage_time; // has quad when time < this
	uint32_t quad_attack_time; // play attack sound when time > this

	g_edict_t *chase_target; // player we are chasing
	g_edict_t *old_chase_target; // player we were chasing

	g_item_t *last_dropped; // last dropped item, used for variable expansion
};

struct g_edict_s {
	entity_state_t s;
	struct g_client_s *client; // NULL if not a player

	bool in_use;
	int32_t link_count;

	link_t area; // linked to a division node or leaf

	int32_t num_clusters; // if -1, use head_node instead
	int32_t cluster_nums[MAX_ENT_CLUSTERS];
	int32_t head_node; // unused if num_clusters != -1
	int32_t area_num, area_num2;

	unsigned sv_flags;
	vec3_t mins, maxs;
	vec3_t abs_mins, abs_maxs, size;
	solid_t solid;
	uint32_t clip_mask;
	g_edict_t *owner;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	uint32_t spawn_flags;
	uint32_t flags; // FL_GOD_MODE, etc..

	char *class_name;
	char *model;

	g_move_type_t move_type;
	g_move_info_t move_info;

	uint32_t timestamp;

	char *target;
	char *target_name;
	char *path_target;
	char *kill_target;
	char *message;
	char *team;
	char *command;
	char *script;

	g_edict_t *target_ent;

	float speed, accel, decel;
	vec3_t move_dir;
	vec3_t pos1, pos2;

	vec3_t velocity;
	vec3_t avelocity;

	float mass;

	float next_think;
	void (*pre_think)(g_edict_t *ent);
	void (*think)(g_edict_t *self);
	void (*blocked)(g_edict_t *self, g_edict_t *other); // move to move_info?
	void (*touch)(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
			c_bsp_surface_t *surf);
	void (*use)(g_edict_t *self, g_edict_t *other, g_edict_t *activator);
	void (*pain)(g_edict_t *self, g_edict_t *other, int32_t damage, int32_t knockback);
	void (*die)(g_edict_t *self, g_edict_t *inflictor, g_edict_t *attacker,
			int32_t damage, vec3_t point);

	uint32_t touch_time;
	uint32_t push_time;

	int16_t health;
	int16_t max_health;
	bool dead;

	bool take_damage;
	int16_t dmg;
	int16_t knockback;
	float dmg_radius;
	int16_t sounds; // make this a spawntemp var?
	int32_t count;

	g_edict_t *chain;
	g_edict_t *enemy;
	g_edict_t *activator;
	g_edict_t *ground_entity;
	int32_t ground_entity_link_count;
	g_edict_t *team_chain;
	g_edict_t *team_master;
	g_edict_t *lightning;

	uint16_t noise_index;
	int16_t attenuation;

	// timing variables
	float wait;
	float delay; // before firing targets
	float random;

	uint32_t water_type;
	uint32_t old_water_level;
	uint32_t water_level;

	int32_t area_portal; // the area portal to toggle

	g_item_t *item; // for bonus items

	c_bsp_plane_t plane; // last touched
	c_bsp_surface_t *surf;

	vec3_t map_origin; // where the map says we spawn
};

#endif

#endif /* __G_GAME_TYPES_H__ */
