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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shared.h"

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible g_client_t and edict_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include "game/game.h"

#ifdef HAVE_MYSQL
#include <mysql.h>
#endif

// the "gameversion" client command will print this plus compile date
#define GAMEVERSION	"default"

// protocol bytes that can be directly added to messages
// these must be kept in sync with the declarations in common.h
#define svc_muzzle_flash	2
#define svc_temp_entity		3
#define svc_layout			4
#define svc_sound			7
#define svc_stuff_text		9

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
#define FL_GODMODE				0x00000004
#define FL_TEAMSLAVE			0x00000008  // not the first on the team
#define FL_RESPAWN				0x80000000  // used for item respawning

// memory tags to allow dynamic memory to be cleaned up
#define TAG_GAME	765  // clear when unloading the dll
#define TAG_LEVEL	766  // clear when loading a new level

// ammo types
typedef enum {
	AMMO_SHELLS,
	AMMO_BULLETS,
	AMMO_GRENADES,
	AMMO_ROCKETS,
	AMMO_CELLS,
	AMMO_BOLTS,
	AMMO_SLUGS,
	AMMO_NUKES
} g_ammo_t;

// armor types
typedef enum {
	ARMOR_NONE,
	ARMOR_JACKET,
	ARMOR_COMBAT,
	ARMOR_BODY,
	ARMOR_SHARD
} g_armor_t;

// health types
typedef enum {
	HEALTH_NONE,
	HEALTH_SMALL,
	HEALTH_MEDIUM,
	HEALTH_LARGE,
	HEALTH_MEGA
} g_health_t;

// edict->move_type values
typedef enum {
	MOVE_TYPE_NONE,       // never moves
	MOVE_TYPE_NO_CLIP,     // origin and angles change with no interaction
	MOVE_TYPE_PUSH,       // no clip to world, push on box contact
	MOVE_TYPE_STOP,       // no clip to world, stops on box contact

	MOVE_TYPE_WALK,       // gravity
	MOVE_TYPE_FLY,
	MOVE_TYPE_TOSS,       // gravity
} g_move_type_t;

// a synonym for readability
#define MOVE_TYPE_THINK MOVE_TYPE_NONE


// gitem_t->flags
typedef enum {
	ITEM_WEAPON,
	ITEM_AMMO,
	ITEM_ARMOR,
	ITEM_FLAG,
	ITEM_HEALTH,
	ITEM_POWERUP
} g_item_type_t;

typedef struct g_item_s {
	char *class_name;  // spawning name
	qboolean (*pickup)(struct edict_s *ent, struct edict_s *other);
	void (*use)(struct edict_s *ent, struct g_item_s *item);
	void (*drop)(struct edict_s *ent, struct g_item_s *item);
	void (*weapon_think)(struct edict_s *ent);
	char *pickup_sound;
	char *model;
	int effects;

	// client side info
	char *icon;
	char *pickup_name;  // for printing on pickup

	int quantity;  // for ammo how much, for weapons how much is used per shot
	char *ammo;  // for weapons
	g_item_type_t type;  // g_item_type_t, see above
	int tag;  // type-specific flags

	char *precaches;  // string of all models, sounds, and images this item will use
} g_item_t;

// override quake2 items for legacy maps
typedef struct g_override_s {
	char *old;
	char *new;
} g_override_t;

extern g_override_t g_overrides[];


// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actually present
// in edict_t during gameplay
typedef struct g_spawn_temp_s {
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

	int lip;
	int distance;
	int height;
	char *noise;
	float pausetime;
	char *item;
} g_spawn_temp_t;


typedef struct g_move_info_s {
	// fixed data
	vec3_t start_origin;
	vec3_t start_angles;
	vec3_t end_origin;
	vec3_t end_angles;

	int sound_start;
	int sound_middle;
	int sound_end;

	float accel;
	float speed;
	float decel;
	float distance;

	float wait;

	// state data
	int state;
	vec3_t dir;
	float current_speed;
	float move_speed;
	float next_speed;
	float remaining_distance;
	float decel_distance;
	void (*done)(edict_t *);
} g_move_info_t;

// this structure is left intact through an entire game
typedef struct g_locals_s {

	edict_t *edicts;  // [g_max_entities]

	g_client_t *clients;  // [sv_max_clients]

	g_spawn_temp_t spawn;

	int num_items;
	int num_overrides;

} g_game_t;

extern g_game_t g_game;


// this structure is cleared as each map is entered
typedef struct g_level_s {
	int frame_num;
	float time;

	char title[MAX_STRING_CHARS];  // the descriptive name (Stress Fractures, etc)
	char name[MAX_QPATH];  // the server name (fractures, etc)
	int gravity;  // defaults to 800
	int gameplay;  // DEATHMATCH, INSTAGIB, ARENA
	int teams;
	int ctf;
	int match;
	int rounds;
	int frag_limit;
	int round_limit;
	int capture_limit;
	float time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];

	// intermission state
	float intermission_time;  // time intermission started
	vec3_t intermission_origin;
	vec3_t intermission_angle;
	const char *changemap;

	qboolean warmup;  // shared by match and round

	qboolean start_match;
	float match_time;  // time match started
	int match_num;

	qboolean start_round;
	float round_time;  // time round started
	int round_num;

	char vote_cmd[64];  // current vote in question
	int votes[3]; // current vote tallies
	float votetime;  // time vote started

	edict_t *current_entity;  // entity running from G_RunFrame
} g_level_t;

extern g_level_t g_level;

extern g_import_t gi;
extern g_export_t ge;

extern int grenade_index, grenade_hit_index;
extern int rocket_index, rocket_fly_index;
extern int lightning_fly_index;
extern int quad_damage_index;

// means of death
#define MOD_UNKNOWN					0
#define MOD_SHOTGUN					1
#define MOD_SUPER_SHOTGUN			2
#define MOD_MACHINEGUN				3
#define MOD_GRENADE					4
#define MOD_GRENADE_SPLASH			5
#define MOD_ROCKET					6
#define MOD_ROCKET_SPLASH			7
#define MOD_HYPERBLASTER			8
#define MOD_LIGHTNING				9
#define MOD_LIGHTNING_DISCHARGE		10
#define MOD_RAILGUN					11
#define MOD_BFG_LASER				12
#define MOD_BFG_BLAST				13
#define MOD_WATER					14
#define MOD_SLIME					15
#define MOD_LAVA					16
#define MOD_CRUSH					17
#define MOD_TELEFRAG				18
#define MOD_FALLING					19
#define MOD_SUICIDE					20
#define MOD_EXPLOSIVE				21
#define MOD_TRIGGER_HURT			22
#define MOD_FRIENDLY_FIRE			0x8000000

extern int means_of_death;

// some macros to help with field lookups
#define FOFS(x)(ptrdiff_t)&(((edict_t *)0)->x)
#define SOFS(x)(ptrdiff_t)&(((g_spawn_temp_t *)0)->x)

extern cvar_t *g_auto_join;
extern cvar_t *g_capture_limit;
extern cvar_t *g_chat_log;
extern cvar_t *g_cheats;
extern cvar_t *g_ctf;
extern cvar_t *g_frag_limit;
extern cvar_t *g_frag_log;
extern cvar_t *g_friendly_fire;
extern cvar_t *g_gameplay;
extern cvar_t *g_gravity;
extern cvar_t *g_match;
extern cvar_t *g_max_entities;
extern cvar_t *g_mysql;
extern cvar_t *g_mysql_db;
extern cvar_t *g_mysql_host;
extern cvar_t *g_mysql_password;
extern cvar_t *g_mysql_user;
extern cvar_t *g_player_projectile;
extern cvar_t *g_random_map;
extern cvar_t *g_round_limit;
extern cvar_t *g_rounds;
extern cvar_t *g_spawn_farthest;
extern cvar_t *g_teams;
extern cvar_t *g_time_limit;
extern cvar_t *g_voting;

extern cvar_t *password;

extern cvar_t *sv_max_clients;
extern cvar_t *dedicated;

// maplist structs
typedef struct g_map_list_elt_s {
	char name[32];
	char title[128];
	char sky[32];
	char weather[64];
	int gravity;
	int gameplay;
	int teams;
	int ctf;
	int match;
	int rounds;
	int frag_limit;
	int round_limit;
	int capture_limit;
	float time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];
	float weight;
} g_map_list_elt_t;

#define MAX_MAP_LIST_ELTS 64
#define MAP_LIST_WEIGHT 16384

typedef struct g_map_list_s {
	g_map_list_elt_t maps[MAX_MAP_LIST_ELTS];
	int count, index;

	// weighted random selection
	int weighted_index[MAP_LIST_WEIGHT];
	float total_weight;
} g_map_list_t;

extern g_map_list_t g_map_list;

// voting
#define MAX_VOTE_TIME 60.0
#define VOTE_MAJORITY 0.51

typedef enum {
	VOTE_NOOP, VOTE_YES, VOTE_NO
} g_vote_t;

// gameplay modes
typedef enum {
	DEATHMATCH, INSTAGIB, ARENA
} g_gameplay_t;

// teams
typedef struct team_s {
	char name[16];
	char skin[32];
	int score;
	int captures;
	float name_time;  // prevent change spamming
	float skin_time;
} g_team_t;

#define TEAM_CHANGE_TIME 5.0
extern g_team_t good, evil;

// text file logging
extern FILE *frag_log, *chat_log;

// mysql database logging
#ifdef HAVE_MYSQL
extern MYSQL *mysql;
extern char sql[512];
#endif

// damage flags
#define DAMAGE_RADIUS			0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR			0x00000002  // armor does not protect from this damage
#define DAMAGE_ENERGY			0x00000004  // damage is from an energy based weapon
#define DAMAGE_BULLET			0x00000008  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION	0x00000010  // armor and godmode have no effect

// g_ballistics.c
void G_BulletProjectile(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback, int hspread, int vspread, int mod);
void G_ShotgunProjectiles(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback, int hspread, int vspread, int count, int mod);
void G_HyperblasterProjectile(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback);
void G_GrenadeProjectile(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback, float damage_radius, float timer);
void G_RocketProjectile(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback, float damage_radius);
void G_LightningProjectile(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback);
void G_RailgunProjectile(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback);
void G_BfgProjectiles(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback, float damage_radius);

// g_chase.c
void G_ChaseThink(edict_t *ent);
void G_ChaseNext(edict_t *ent);
void G_ChasePrevious(edict_t *ent);
void G_ChaseTarget(edict_t *ent);

// g_client.c
qboolean G_ClientConnect(edict_t *ent, char *user_info);
void G_ClientUserInfoChanged(edict_t *ent, const char *user_info);
void G_ClientDisconnect(edict_t *ent);
void G_ClientBegin(edict_t *ent);
void G_ClientRespawn(edict_t *ent, qboolean voluntary);
void G_ClientBeginFrame(edict_t *ent);
void G_ClientThink(edict_t *ent, user_cmd_t *ucmd);
void G_Pain(edict_t *self, edict_t *other, int damage, int knockback);
void G_Die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);
void G_TossQuadDamage(edict_t *self);
void G_TossFlag(edict_t *self);

// g_combat.c
qboolean G_OnSameTeam(edict_t *ent1, edict_t *ent2);
qboolean G_CanDamage(edict_t *targ, edict_t *inflictor);
void G_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir,
		vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod);
void G_RadiusDamage(edict_t *inflictor, edict_t *attacker, edict_t *ignore,
		int damage, int knockback, float radius, int mod);

// g_cmds.c
void G_ClientCommand(edict_t *ent);
void G_Score_f(edict_t *ent);
qboolean G_AddClientToTeam(edict_t *ent, char *team_name);

// g_entity_func.c
void G_func_plat(edict_t *ent);
void G_func_rotating(edict_t *ent);
void G_func_button(edict_t *ent);
void G_func_door(edict_t *ent);
void G_func_door_secret(edict_t *ent);
void G_func_door_rotating(edict_t *ent);
void G_func_water(edict_t *ent);
void G_func_train(edict_t *ent);
void G_func_conveyor(edict_t *self);
void G_func_wall(edict_t *self);
void G_func_object(edict_t *self);
void G_func_timer(edict_t *self);
void G_func_areaportal(edict_t *ent);
void G_func_killbox(edict_t *ent);

// g_entity_info.c
void G_info_null(edict_t *self);
void G_info_notnull(edict_t *self);
void G_info_player_start(edict_t *ent);
void G_info_player_deathmatch(edict_t *ent);
void G_info_player_team1(edict_t *ent);
void G_info_player_team2(edict_t *ent);
void G_info_player_intermission(edict_t *ent);

// g_entity_misc.c
void G_misc_teleporter(edict_t *self);
void G_misc_teleporter_dest(edict_t *self);

// g_entity_target.c
void G_target_speaker(edict_t *ent);
void G_target_explosion(edict_t *ent);
void G_target_splash(edict_t *ent);
void G_target_string(edict_t *ent);

// g_entity_trigger.c
void G_trigger_always(edict_t *ent);
void G_trigger_once(edict_t *ent);
void G_trigger_multiple(edict_t *ent);
void G_trigger_relay(edict_t *ent);
void G_trigger_push(edict_t *ent);
void G_trigger_hurt(edict_t *ent);
void G_trigger_exec(edict_t *ent);

// g_items.c
extern g_item_t g_items[];

#define ITEM_INDEX(x) ((x) - g_items)

void G_PrecacheItem(g_item_t *it);
void G_InitItems(void);
void G_SetItemNames(void);
g_item_t *G_FindItem(const char *pickup_name);
g_item_t *G_FindItemByClassname(const char *class_name);
edict_t *G_DropItem(edict_t *ent, g_item_t *item);
void G_SetRespawn(edict_t *ent, float delay);
void G_SpawnItem(edict_t *ent, g_item_t *item);
void G_ResetFlag(edict_t *ent);
g_item_t *G_ItemByIndex(int index);
qboolean G_AddAmmo(edict_t *ent, g_item_t *item, int count);
void G_TouchItem(edict_t *ent, edict_t *other, c_plane_t *plane, c_surface_t *surf);

// g_main.c
void G_Init(void);
void G_Shutdown(void);
void G_ResetTeams(void);
void G_ResetVote(void);

// g_physics.c
void G_RunEntity(edict_t *ent);

// g_spawn.c
void G_SpawnEntities(const char *name, const char *entities);

// g_stats.c
void G_ClientToIntermission(edict_t *client);
void G_ClientStats(edict_t *ent);
void G_ClientSpectatorStats(edict_t *ent);
void G_ClientTeamsScoreboard(edict_t *client);
void G_ClientScoreboard(edict_t *client);

// g_utils.c
qboolean G_KillBox(edict_t *ent);
void G_ProjectSpawn(edict_t *ent);
void G_SetupProjectile(edict_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t org);
edict_t *G_Find(edict_t *from, ptrdiff_t field, const char *match);
edict_t *G_FindRadius(edict_t *from, vec3_t org, float rad);
edict_t *G_PickTarget(char *target_name);
void G_UseTargets(edict_t *ent, edict_t *activator);
void G_SetMovedir(vec3_t angles, vec3_t movedir);
char *G_GameplayName(int g);
int G_GameplayByName(char *c);
g_team_t *G_TeamByName(char *c);
g_team_t *G_OtherTeam(g_team_t *t);
g_team_t *G_TeamForFlag(edict_t *ent);
edict_t *G_FlagForTeam(g_team_t *t);
int G_EffectForTeam(g_team_t *t);
g_team_t *G_SmallestTeam(void);
g_client_t *G_ClientByName(char *name);
qboolean G_IsStationary(edict_t *ent);
void G_SetAnimation(edict_t *ent, entity_animation_t anim, qboolean restart);
edict_t *G_Spawn(void);
void G_InitEdict(edict_t *e);
void G_FreeEdict(edict_t *e);
void G_TouchTriggers(edict_t *ent);
void G_TouchSolids(edict_t *ent);
trace_t G_PushEntity(edict_t *ent, vec3_t push);
char *G_CopyString(char *in);
float *tv(float x, float y, float z);
char *vtos(vec3_t v);

// g_view.c
void G_ClientEndFrame(edict_t *ent);
void G_EndClientFrames(void);

// g_weapon.c
void G_ChangeWeapon(edict_t *ent);
void G_WeaponThink(edict_t *ent);
qboolean G_PickupWeapon(edict_t *ent, edict_t *other);
void G_UseBestWeapon(g_client_t *client);
void G_UseWeapon(edict_t *ent, g_item_t *inv);
void G_DropWeapon(edict_t *ent, g_item_t *inv);
void G_FireShotgun(edict_t *ent);
void G_FireSuperShotgun(edict_t *ent);
void G_FireMachinegun(edict_t *ent);
void G_FireHyperblaster(edict_t *ent);
void G_FireRocketLauncher(edict_t *ent);
void G_FireGrenadeLauncher(edict_t *ent);
void G_FireLightning(edict_t *ent);
void G_FireRailgun(edict_t *ent);
void G_FireBfg(edict_t *ent);

#define MAX_NET_NAME 64

// client data that persists through respawns
typedef struct g_client_locals_s {
	int first_frame;  // g_level.frame_num the client entered the game

	char user_info[MAX_INFO_STRING];
	char net_name[MAX_NET_NAME];
	char sql_name[20];
	char skin[32];
	int score;
	int captures;

	int health;
	int max_health;

	int armor;
	int max_armor;

	int inventory[MAX_ITEMS];

	// ammo capacities
	int max_shells;
	int max_bullets;
	int max_grenades;
	int max_rockets;
	int max_cells;
	int max_bolts;
	int max_slugs;
	int max_nukes;

	g_item_t *weapon;
	g_item_t *last_weapon;

	int weapon_frame;

	qboolean spectator;  // client is a spectator
	qboolean ready;  // ready

	g_team_t *team;  // current team (good/evil)
	g_vote_t vote;  // current vote (yes/no)
	int match_num;  // most recent match
	int round_num;  // most recent arena round
	int color;  // weapon effect colors
} g_client_locals_t;


// this structure is cleared on each respawn
struct g_client_s {
	// known to server
	player_state_t ps;  // communicated by server to clients
	int ping;

	// private to game

	g_client_locals_t locals;

	qboolean show_scores;  // sets layout bit mask in player state

	int ammo_index;

	int buttons;
	int old_buttons;
	int latched_buttons;

	float weapon_think_time;  // time when the weapon think was called
	float weapon_fire_time;  // can fire when time > this
	float muzzle_flash_time;  // should send muzzle flash when time > this
	g_item_t *new_weapon;

	int damage_armor;  // damage absorbed by armor
	int damage_blood;  // damage taken out of health
	vec3_t damage_from;  // origin for vector calculation

	vec3_t angles;  // aiming direction
	vec3_t old_angles;
	vec3_t old_velocity;

	vec3_t cmd_angles;  // angles sent over in the last command

	float drown_time;
	float sizzle_time;
	int old_water_level;

	float fall_time;  // eligible for landing event when time > this
	float footstep_time;  // play a footstep when time > this

	float pickup_msg_time;  // display msg until time > this

	float chat_time;  // can chat when time > this
	qboolean muted;

	float respawn_time;  // can respawn when time > this

	float quad_damage_time;  // has quad when time < this
	float quad_attack_time;  // play attack sound when time > this

	edict_t *chase_target;  // player we are chasing
};


struct edict_s {
	entity_state_t s;
	struct g_client_s *client;  // NULL if not a player

	qboolean in_use;
	int link_count;

	link_t area;  // linked to a division node or leaf

	int num_clusters;  // if -1, use head_node instead
	int cluster_nums[MAX_ENT_CLUSTERS];
	int head_node;  // unused if num_clusters != -1
	int area_num, area_num2;

	int sv_flags;
	vec3_t mins, maxs;
	vec3_t abs_mins, abs_maxs, size;
	solid_t solid;
	int clip_mask;
	edict_t *owner;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	g_move_type_t move_type;
	int flags;

	char *model;
	float free_time;  // sv.time when the object was freed

	// only used locally in game, not by server
	char *class_name;
	int spawn_flags;

	float timestamp;

	float angle;  // set in qe3, -1 = up, -2 = down
	char *target;
	char *target_name;
	char *path_target;
	char *kill_target;
	char *message;
	char *team;
	char *command;
	char *script;

	edict_t *target_ent;

	float speed, accel, decel;
	vec3_t movedir;
	vec3_t pos1, pos2;

	vec3_t velocity;
	vec3_t avelocity;

	float mass;
	float gravity;

	float next_think;
	void (*prethink)(edict_t *ent);
	void (*think)(edict_t *self);
	void (*blocked)(edict_t *self, edict_t *other);  // move to move_info?
	void (*touch)(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf);
	void (*use)(edict_t *self, edict_t *other, edict_t *activator);
	void (*pain)(edict_t *self, edict_t *other, int damage, int knockback);
	void (*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

	float touch_time;  // func_door activation
	float push_time;  // trigger_push sound
	float jump_time;  // jump sound
	float pain_time;  // pain sound
	float gasp_time;  // gasp sound
	float drown_time;  // drown sound and damage

	int health;
	int max_health;
	qboolean dead;

	int view_height;  // height above origin where eyesight is determined
	qboolean take_damage;
	int dmg;
	int knockback;
	float dmg_radius;
	int sounds;  // make this a spawntemp var?
	int count;

	edict_t *chain;
	edict_t *enemy;
	edict_t *activator;
	edict_t *ground_entity;
	int ground_entity_link_count;
	edict_t *team_chain;
	edict_t *team_master;
	edict_t *lightning;

	int noise_index;
	int attenuation;

	// timing variables
	float wait;
	float delay;  // before firing targets
	float random;

	int water_type;
	int water_level;

	vec3_t dest;  // projectile destination

	int style;  // also used as areaportal number

	g_item_t *item;  // for bonus items

	c_plane_t plane;  // last touched
	c_surface_t *surf;

	vec3_t map_origin;  // where the map says we spawn

	// common data blocks
	g_move_info_t move_info;
};
