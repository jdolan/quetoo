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
// short, server-visible gclient_t and edict_t structures,
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
#define svc_muzzleflash		2
#define svc_temp_entity		3
#define svc_layout			4
#define svc_sound			7
#define svc_stufftext		9

// edict->spawnflags
#define SF_ITEM_TRIGGER			0x00000001
#define SF_ITEM_NO_TOUCH		0x00000002
#define SF_ITEM_HOVER			0x00000004

// we keep these around for compatibility with legacy levels
#define SF_NOT_EASY			0x00000100
#define SF_NOT_MEDIUM			0x00000200
#define SF_NOT_HARD			0x00000400
#define SF_NOT_DEATHMATCH		0x00000800
#define SF_NOT_COOP			0x00001000

#define SF_ITEM_DROPPED			0x00010000
#define SF_ITEM_TARGETS_USED		0x00020000

// edict->flags
#define FL_FLY					0x00000001
#define FL_SWIM					0x00000002  // implied immunity to drowning
#define FL_GODMODE				0x00000004
#define FL_TEAMSLAVE			0x00000008  // not the first on the team
#define FL_RESPAWN				0x80000000  // used for item.locals.wning

// memory tags to allow dynamic memory to be cleaned up
#define TAG_GAME	765  // clear when unloading the dll
#define TAG_LEVEL	766  // clear when loading a new level

typedef enum {
	AMMO_SHELLS,
	AMMO_BULLETS,
	AMMO_GRENADES,
	AMMO_ROCKETS,
	AMMO_CELLS,
	AMMO_BOLTS,
	AMMO_SLUGS,
	AMMO_NUKES
} ammo_t;

// armor types
#define ARMOR_NONE			0
#define ARMOR_JACKET		1
#define ARMOR_COMBAT		2
#define ARMOR_BODY			3
#define ARMOR_SHARD			4

// health types
#define HEALTH_NONE			0
#define HEALTH_SMALL		1
#define HEALTH_MEDIUM		2
#define HEALTH_LARGE		3
#define HEALTH_MEGA			4

// edict->movetype values
typedef enum {
	MOVETYPE_NONE,       // never moves
	MOVETYPE_NOCLIP,     // origin and angles change with no interaction
	MOVETYPE_PUSH,       // no clip to world, push on box contact
	MOVETYPE_STOP,       // no clip to world, stops on box contact

	MOVETYPE_WALK,       // gravity
	MOVETYPE_FLY,
	MOVETYPE_TOSS,       // gravity
} movetype_t;

// a synonym for readability
#define MOVETYPE_THINK MOVETYPE_NONE


// gitem_t->flags
#define IT_WEAPON		1  // use makes active weapon
#define IT_AMMO			2
#define IT_ARMOR		4
#define IT_FLAG			8
#define IT_HEALTH		16
#define IT_POWERUP		32

// gitem_t->weapmodel for weapons indicates model index
#define WEAP_SHOTGUN			0
#define WEAP_SUPERSHOTGUN		1
#define WEAP_MACHINEGUN			2
#define WEAP_GRENADELAUNCHER	3
#define WEAP_ROCKETLAUNCHER		4
#define WEAP_HYPERBLASTER		5
#define WEAP_LIGHTNING			6
#define WEAP_RAILGUN			7
#define WEAP_BFG				8

typedef struct gitem_s {
	char *classname;  // spawning name
	qboolean (*pickup)(struct edict_s *ent, struct edict_s *other);
	void (*use)(struct edict_s *ent, struct gitem_s *item);
	void (*drop)(struct edict_s *ent, struct gitem_s *item);
	void (*weaponthink)(struct edict_s *ent);
	char *pickup_sound;
	char *model;
	int effects;

	// client side info
	char *icon;
	char *pickup_name;  // for printing on pickup

	int quantity;  // for ammo how much, for weapons how much is used per shot
	char *ammo;  // for weapons
	int flags;  // IT_* flags

	int weapmodel;  // weapon model index (for weapons)
	int tag;

	char *precaches;  // string of all models, sounds, and images this item will use
} gitem_t;

// override quake2 items for legacy maps
typedef struct override_s {
	char *old;
	char *new;
} override_t;

extern override_t overrides[];

// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
typedef struct {
	gclient_t *clients;  // [sv_maxclients]

	// items
	int num_items;
	int num_overrides;

} game_locals_t;


// this structure is cleared as each map is entered
// it is read/written to the level.sav file for savegames
typedef struct {
	int framenum;
	float time;

	char title[MAX_STRING_CHARS];  // the descriptive name (Stress Fractures, etc)
	char name[MAX_QPATH];  // the server name (fractures, etc)
	int gravity;  // defaults to 800
	int gameplay;  // DEATHMATCH, INSTAGIB, ARENA
	int teams;
	int ctf;
	int match;
	int rounds;
	int fraglimit;
	int roundlimit;
	int capturelimit;
	float timelimit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];

	// intermission state
	float intermission_time;  // time intermission started
	vec3_t intermission_origin;
	vec3_t intermission_angle;
	const char *changemap;

	qboolean warmup;  // shared by match and round

	qboolean start_match;
	float matchtime;  // time match started
	int matchnum;

	qboolean start_round;
	float roundtime;  // time round started
	int roundnum;

	char vote_cmd[64];  // current vote in question
	int votes[3]; // current vote tallies
	float votetime;  // time vote started

	edict_t *current_entity;  // entity running from G_RunFrame
} level_locals_t;


// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
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
	char *fraglimit;
	char *roundlimit;
	char *capturelimit;
	char *timelimit;
	char *give;
	char *music;

	int lip;
	int distance;
	int height;
	char *noise;
	float pausetime;
	char *item;
} spawn_temp_t;


typedef struct {
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
	void (*endfunc)(edict_t *);
} moveinfo_t;


extern game_locals_t game;
extern level_locals_t level;
extern game_import_t gi;
extern game_export_t globals;
extern spawn_temp_t st;

extern int grenade_index, grenade_hit_index;
extern int rocket_index, rocket_fly_index;
extern int lightning_fly_index;
extern int quad_damage_index;

// means of death
#define MOD_UNKNOWN			0
#define MOD_SHOTGUN			1
#define MOD_SSHOTGUN		2
#define MOD_MACHINEGUN		3
#define MOD_GRENADE			4
#define MOD_G_SPLASH		5
#define MOD_ROCKET			6
#define MOD_R_SPLASH		7
#define MOD_HYPERBLASTER	8
#define MOD_LIGHTNING		9
#define MOD_L_DISCHARGE		10
#define MOD_RAILGUN			11
#define MOD_BFG_LASER		12
#define MOD_BFG_BLAST		13
#define MOD_WATER			14
#define MOD_SLIME			15
#define MOD_LAVA			16
#define MOD_CRUSH			17
#define MOD_TELEFRAG		18
#define MOD_FALLING			19
#define MOD_SUICIDE			20
#define MOD_EXPLOSIVE		21
#define MOD_TRIGGER_HURT	22
#define MOD_FRIENDLY_FIRE	0x8000000

extern int meansOfDeath;

extern edict_t *g_edicts;

#define FOFS(x)(ptrdiff_t)&(((edict_t *)0)->x)
#define STOFS(x)(ptrdiff_t)&(((spawn_temp_t *)0)->x)
#define LLOFS(x)(ptrdiff_t)&(((level_locals_t *)0)->x)
#define CLOFS(x)(ptrdiff_t)&(((gclient_t *)0)->x)

#define random() ((rand() & 0x7fff) / ((float)0x7fff))
#define crandom() (2.0 * (random() - 0.5))

extern cvar_t *g_autojoin;
extern cvar_t *g_capturelimit;
extern cvar_t *g_chatlog;
extern cvar_t *g_cheats;
extern cvar_t *g_ctf;
extern cvar_t *g_fraglimit;
extern cvar_t *g_fraglog;
extern cvar_t *g_friendlyfire;
extern cvar_t *g_gameplay;
extern cvar_t *g_gravity;
extern cvar_t *g_match;
extern cvar_t *g_maxentities;
extern cvar_t *g_mysql;
extern cvar_t *g_mysqldb;
extern cvar_t *g_mysqlhost;
extern cvar_t *g_mysqlpassword;
extern cvar_t *g_mysqluser;
extern cvar_t *g_playerprojectile;
extern cvar_t *g_randommap;
extern cvar_t *g_roundlimit;
extern cvar_t *g_rounds;
extern cvar_t *g_spawnfarthest;
extern cvar_t *g_teams;
extern cvar_t *g_timelimit;
extern cvar_t *g_voting;

extern cvar_t *password;

extern cvar_t *sv_maxclients;
extern cvar_t *dedicated;

// maplist structs
typedef struct maplistelt_s {
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
	int fraglimit;
	int roundlimit;
	int capturelimit;
	float timelimit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];
	float weight;
} maplistelt_t;

#define MAX_MAPLIST_ELTS 64
#define MAPLIST_WEIGHT 16384
typedef struct maplist_s {
	maplistelt_t maps[MAX_MAPLIST_ELTS];
	int count, index;

	// weighted random selection
	int weighted_index[MAPLIST_WEIGHT];
	float total_weight;
} maplist_t;

extern maplist_t maplist;

// voting
#define MAX_VOTE_TIME 60.0
#define VOTE_MAJORITY 0.51

typedef enum {
	VOTE_NOOP, VOTE_YES, VOTE_NO
} vote_t;

// gameplay modes
typedef enum {
	DEATHMATCH, INSTAGIB, ARENA
} gameplay_t;

// teams
typedef struct team_s {
	char name[16];
	char skin[32];
	int score;
	int captures;
	float nametime;  // prevent change spamming
	float skintime;
} team_t;

#define TEAM_CHANGE_TIME 5.0
extern team_t good, evil;

// text file logging
extern FILE *fraglog, *chatlog;

// mysql database logging
#ifdef HAVE_MYSQL
extern MYSQL *mysql;
extern char sql[512];
#endif

// g_cmds.c
void P_Command(edict_t *ent);
void G_Score_f(edict_t *ent);
qboolean G_AddClientToTeam(edict_t *ent, char *teamname);

// g_items.c
extern gitem_t itemlist[];

#define ITEM_INDEX(x) ((x) - itemlist)

void G_PrecacheItem(gitem_t *it);
void G_InitItems(void);
void G_SetItemNames(void);
gitem_t *G_FindItem(const char *pickup_name);
gitem_t *G_FindItemByClassname(const char *classname);
edict_t *G_DropItem(edict_t *ent, gitem_t *item);
void G_SetRespawn(edict_t *ent, float delay);
void G_SpawnItem(edict_t *ent, gitem_t *item);
void G_ResetFlag(edict_t *ent);
gitem_t *G_ItemByIndex(int index);
qboolean G_AddAmmo(edict_t *ent, gitem_t *item, int count);
void G_TouchItem(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

// g_utils.c
qboolean G_KillBox(edict_t *ent);
void G_ProjectSpawn(edict_t *ent);
void G_ProjectSource(vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
edict_t *G_Find(edict_t *from, int fieldofs, const char *match);
edict_t *G_FindRadius(edict_t *from, vec3_t org, float rad);
edict_t *G_PickTarget(char *targetname);
void G_UseTargets(edict_t *ent, edict_t *activator);
void G_SetMovedir(vec3_t angles, vec3_t movedir);
char *G_GameplayName(int g);
int G_GameplayByName(char *c);
team_t *G_TeamByName(char *c);
team_t *G_OtherTeam(team_t *t);
team_t *G_TeamForFlag(edict_t *ent);
edict_t *G_FlagForTeam(team_t *t);
int G_EffectForTeam(team_t *t);
team_t *G_SmallestTeam(void);
gclient_t *G_ClientByName(char *name);
qboolean G_IsStationary(edict_t *ent);
edict_t *G_Spawn(void);
void G_InitEdict(edict_t *e);
void G_FreeEdict(edict_t *e);
void G_TouchTriggers(edict_t *ent);
void G_TouchSolids(edict_t *ent);
trace_t G_PushEntity(edict_t *ent, vec3_t push);
char *G_CopyString(char *in);
float *tv(float x, float y, float z);
char *vtos(vec3_t v);

// g_combat.c
qboolean G_OnSameTeam(edict_t *ent1, edict_t *ent2);
qboolean G_CanDamage(edict_t *targ, edict_t *inflictor);
void G_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir,
		vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod);
void G_RadiusDamage(edict_t *inflictor, edict_t *attacker, edict_t *ignore,
		int damage, int knockback, float radius, int mod);

// damage flags
#define DAMAGE_RADIUS			0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR			0x00000002  // armor does not protect from this damage
#define DAMAGE_ENERGY			0x00000004  // damage is from an energy based weapon
#define DAMAGE_BULLET			0x00000008  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION	0x00000010  // armor and godmode have no effect

#define DEFAULT_BULLET_HSPREAD	100
#define DEFAULT_BULLET_VSPREAD	200
#define DEFAULT_SHOTGUN_HSPREAD	1000
#define DEFAULT_SHOTGUN_VSPREAD	500
#define DEFAULT_SHOTGUN_COUNT	12
#define DEFAULT_SSHOTGUN_COUNT	24

// g_weapon.c
void G_FireBullet(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback, int hspread, int vspread, int mod);
void G_FireShotgun(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback, int hspread, int vspread, int count, int mod);
void G_FireHyperblaster(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback);
void G_FireGrenadeLauncher(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback, float damage_radius, float timer);
void G_FireRocketLauncher(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback, float damage_radius);
void G_FireLightning(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback);
void G_FireRailgun(edict_t *self, vec3_t start, vec3_t dir,
		int damage, int knockback);
void G_FireBFG(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback, float damage_radius);

// p_client.c
qboolean P_Connect(edict_t *ent, char *userinfo);
void P_UserinfoChanged(edict_t *ent, const char *userinfo);
void P_Disconnect(edict_t *ent);
void P_Begin(edict_t *ent);
void P_Respawn(edict_t *ent, qboolean voluntary);
void P_BeginServerFrame(edict_t *ent);
void P_Think(edict_t *ent, usercmd_t *ucmd);
void P_Pain(edict_t *self, edict_t *other, int damage, int knockback);
void P_Die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);
void P_TossQuadDamage(edict_t *self);
void P_TossFlag(edict_t *self);
void P_NoAmmoWeaponChange(gclient_t *client);

// p_view.c
void P_EndServerFrame(edict_t *ent);
void P_EndServerFrames(void);

// p_hud.c
void P_MoveToIntermission(edict_t *client);
void P_SetStats(edict_t *ent);
void P_SetSpectatorStats(edict_t *ent);
void P_CheckChaseStats(edict_t *ent);
void P_TeamsScoreboard(edict_t *client);
void P_Scoreboard(edict_t *client);

// p_weapon.c
void P_ChangeWeapon(edict_t *ent);
void P_WeaponThink(edict_t *ent);
qboolean P_PickupWeapon(edict_t *ent, edict_t *other);
void P_UseWeapon(edict_t *ent, gitem_t *inv);
void P_DropWeapon(edict_t *ent, gitem_t *inv);

void P_FireShotgun(edict_t *ent);
void P_FireSuperShotgun(edict_t *ent);
void P_FireMachinegun(edict_t *ent);
void P_FireHyperblaster(edict_t *ent);
void P_FireRocketLauncher(edict_t *ent);
void P_FireGrenadeLauncher(edict_t *ent);
void P_FireLightning(edict_t *ent);
void P_FireRailgun(edict_t *ent);
void P_FireBFG(edict_t *ent);


// g_phys.c
void G_RunEntity(edict_t *ent);

// g_main.c
void G_Init(void);
void G_Shutdown(void);
void G_ResetTeams(void);
void G_ResetVote(void);

// p_chase.c
void P_UpdateChaseCam(edict_t *ent);
void P_ChaseNext(edict_t *ent);
void P_ChasePrev(edict_t *ent);
void P_GetChaseTarget(edict_t *ent);

// g_spawn.c
void G_SpawnEntities(const char *name, const char *entities);

// g_func.c
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

// p_client.c
void G_info_player_start(edict_t *ent);
void G_info_player_deathmatch(edict_t *ent);
void G_info_player_team1(edict_t *ent);
void G_info_player_team2(edict_t *ent);
void G_info_player_intermission(edict_t *ent);

// g_trigger.c
void G_trigger_always(edict_t *ent);
void G_trigger_once(edict_t *ent);
void G_trigger_multiple(edict_t *ent);
void G_trigger_relay(edict_t *ent);
void G_trigger_push(edict_t *ent);
void G_trigger_hurt(edict_t *ent);
void G_trigger_exec(edict_t *ent);

// g_misc.c
void G_info_null(edict_t *self);
void G_info_notnull(edict_t *self);
void G_misc_teleporter(edict_t *self);
void G_misc_teleporter_dest(edict_t *self);
void G_target_speaker(edict_t *ent);
void G_target_explosion(edict_t *ent);
void G_target_splash(edict_t *ent);
void G_target_string(edict_t *ent);

// client_t->anim_priority
#define ANIM_BASIC		0  // stand / run
#define ANIM_WAVE		1
#define ANIM_JUMP		2
#define ANIM_PAIN		3
#define ANIM_ATTACK		4
#define ANIM_DEATH		5
#define ANIM_REVERSE	6

#define MAX_NETNAME 64
// client data that persists through respawns
typedef struct {
	int enterframe;  // level.framenum the client entered the game

	char userinfo[MAX_INFO_STRING];
	char netname[MAX_NETNAME];
	char sqlname[20];
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

	gitem_t *weapon;
	gitem_t *lastweapon;

	int weapon_frame;

	qboolean spectator;  // client is a spectator
	qboolean ready;  // ready

	team_t *team;  // current team (good/evil)
	vote_t vote;  // current vote (yes/no)
	int matchnum;  // most recent match
	int roundnum;  // most recent arena round
	int color;  // weapon effect colors
} client_locals_t;


// this structure is cleared on each respawn
struct gclient_s {
	// known to server
	player_state_t ps;  // communicated by server to clients
	int ping;

	// private to game
	client_locals_t locals;

	qboolean showscores;  // set layout stat

	int ammo_index;

	int buttons;
	int oldbuttons;
	int latched_buttons;

	qboolean weapon_thunk;
	float weapon_fire_time;  // can fire when time > this
	float muzzleflash_time;  // should send muzzle flash when time > this
	gitem_t *newweapon;

	int damage_armor;  // damage absorbed by armor
	int damage_blood;  // damage taken out of health
	vec3_t damage_from;  // origin for vector calculation

	vec3_t angles;  // aiming direction
	vec3_t oldangles;
	vec3_t oldvelocity;

	vec3_t cmd_angles;  // angles sent over in the last command

	float drown_time;
	float sizzle_time;
	int old_waterlevel;
	float footstep_time;

	// animation vars
	int anim_end;
	int anim_priority;
	qboolean anim_duck;
	qboolean anim_run;
	float anim_time;

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
	struct gclient_s *client;  // NULL if not a player
	// the server expects the first part
	// of gclient_s to be a player_state_t
	// but the rest of it is opaque

	qboolean inuse;
	int linkcount;

	link_t area;  // linked to a division node or leaf

	int num_clusters;  // if -1, use head_node instead
	int clusternums[MAX_ENT_CLUSTERS];
	int head_node;  // unused if num_clusters != -1
	int areanum, areanum2;

	int svflags;
	vec3_t mins, maxs;
	vec3_t absmin, absmax, size;
	solid_t solid;
	int clipmask;
	edict_t *owner;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	int movetype;
	int flags;

	char *model;
	float freetime;  // sv.time when the object was freed

	// only used locally in game, not by server
	char *classname;
	int spawnflags;

	float timestamp;

	float angle;  // set in qe3, -1 = up, -2 = down
	char *target;
	char *targetname;
	char *pathtarget;
	char *killtarget;
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

	float nextthink;
	void (*prethink)(edict_t *ent);
	void (*think)(edict_t *self);
	void (*blocked)(edict_t *self, edict_t *other);  // move to moveinfo?
	void (*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
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

	int viewheight;  // height above origin where eyesight is determined
	qboolean takedamage;
	int dmg;
	int knockback;
	float dmg_radius;
	int sounds;  // make this a spawntemp var?
	int count;

	edict_t *chain;
	edict_t *enemy;
	edict_t *activator;
	edict_t *groundentity;
	int groundentity_linkcount;
	edict_t *teamchain;
	edict_t *teammaster;
	edict_t *lightning;

	int noise_index;
	int attenuation;

	// timing variables
	float wait;
	float delay;  // before firing targets
	float random;

	int watertype;
	int waterlevel;

	vec3_t dest;  // projectile destination

	int style;  // also used as areaportal number

	gitem_t *item;  // for bonus items

	cplane_t plane;  // last touched
	csurface_t surf;

	vec3_t map_origin;  // where the map says we spawn

	// common data blocks
	moveinfo_t moveinfo;
};
