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

#pragma once

#include "net/net_types.h"
#include "renderer/r_types.h"
#include "sound/s_types.h"
#include "ui/ui_types.h"

typedef struct {
	pm_cmd_t cmd; // the movement command
	uint32_t time; // simulation time when the command was sent
	uint32_t timestamp; // system time when the command was sent
	struct {
		uint32_t time; // the simulation time when prediction was run
		vec3_t origin; // the predicted origin for this command
		vec3_t error; // the prediction error for this command
	} prediction;
} cl_cmd_t;

typedef struct {
	int32_t frame_num; // sequential identifier, used for delta
	int32_t delta_frame_num; // negatives indicate no delta
	player_state_t ps; // the player state
	int32_t num_entities; // the number of entities in the frame
	uint32_t entity_state; // non-masked index into cl.entity_states array
	bool valid; // false if delta parsing failed
	bool interpolated; // true if this frame has been interpolated one or more times
	uint32_t time; // simulation time for which the frame is valid
} cl_frame_t;

typedef struct {
	entity_animation_t animation;
	uint32_t time;
	int32_t frame;
	int32_t old_frame;
	float lerp;
	float fraction;
	bool reverse;
} cl_entity_animation_t;

typedef enum {
	TRAIL_PRIMARY,
	TRAIL_SECONDARY,
	TRAIL_TERTIARY,
	TRAIL_BUBBLE,

	TRAIL_ID_COUNT
} cl_trail_id_t;

typedef struct {
	entity_state_t baseline; // delta from this if not from a previous frame
	entity_state_t current;
	entity_state_t prev; // will always be valid, but might just be a copy of current

	int32_t frame_num; // the last frame in which this entity was seen

	uint32_t timestamp; // for intermittent effects

	vec3_t trail_origins[TRAIL_ID_COUNT];

	cl_entity_animation_t animation1; // torso animation
	cl_entity_animation_t animation2; // legs animation

	vec3_t origin; // interpolated origin
	vec3_t previous_origin; // the previous interpolated origin
	vec3_t termination; // and termination
	vec3_t angles; // and angles
	box3_t bounds; // bounding box
	box3_t abs_bounds; // absolute bounding box

	float legs_yaw; // only used by player models; leg angle ideal yaw
	float legs_current_yaw; // only used by player models
	float step_offset; // interpolated step offset

	mat4_t matrix; // snapped transform matrix, for traces
	mat4_t inverse_matrix; // inverse transform, for point contents
} cl_entity_t;

/**
 * @brief A circular buffer of recently sent user_cmd_t is maintained so that
 * we can always re-send the last 2 commands to counter packet loss, and so
 * that client-side prediction can verify its accuracy.
 */
#define CMD_BACKUP 64
#define CMD_MASK (CMD_BACKUP - 1)

/**
 * @brief Client side prediction output, produced by running sent but
 * unacknowledged user_cmd_t's through the player movement code locally.
 */
typedef struct {

	struct {
		vec3_t origin; // the predicted view origin
		vec3_t offset; // and offset (ducking)
		vec3_t angles; // and angles (local movement + delta angles)
		float step_offset;
	} view;

	cm_trace_t ground;

	vec3_t error; // the prediction error, interpolated over the current server frame
} cl_predicted_state_t;

/**
 * @brief We accumulate a large buffer of entity states for each entity in order to calculate delta compression.
 */
#define ENTITY_STATE_BACKUP (PACKET_BACKUP * MAX_PACKET_ENTITIES)
#define ENTITY_STATE_MASK (ENTITY_STATE_BACKUP - 1)

/**
 * @brief How many samples to keep of frame/packet counts.
 */
#define STAT_COUNTER_SAMPLE_COUNT 20

/**
 * @brief How many samples to keep of frametimes.
 */
#define FRAMETIME_COUNTER_SAMPLE_COUNT 1024

/**
 * @brief The client structure is cleared at each level load, and is exposed to
 * the client game module to provide access to media and other client state.
 */
typedef struct {
	uint32_t time_demo_frames;
	uint32_t time_demo_start;

	uint16_t frame_counter[STAT_COUNTER_SAMPLE_COUNT];
	uint16_t packet_counter[STAT_COUNTER_SAMPLE_COUNT];
	uint8_t sample_index, sample_count;
	
	uint8_t frametime_counter[FRAMETIME_COUNTER_SAMPLE_COUNT];
	uint8_t frametime_index, frametime_count;

	/**
	 * @brief The client commands, buffered.
	 * @details The client sends several commands each frame, to best ensure that the server
	 * receives them. This buffer also enables client side prediction to run several commands
	 * ahead of what we know the server has received.
	 */
	cl_cmd_t cmds[CMD_BACKUP];

	/**
	 * @brief The predicted state (view origin, offset, angles, etc) of the client.
	 */
	cl_predicted_state_t predicted_state;

	/**
	 * @brief The most recently interpolated server frame.
	 */
	cl_frame_t frame;

	/**
	 * @brief A circular buffer of received server frames, so that the client can take
	 * advantage of delta-compression.
	 */
	cl_frame_t frames[PACKET_BACKUP];

	/**
	 * @brief The delta frame for the currently received frame, if any. `NULL` otherwise.
	 * @details This is a pointer into `frames`.
	 */
	const cl_frame_t *delta_frame;

	/**
	 * @brief The previously received frame, if sequential. `NULL` otherwise.
	 * @detaiils This is a pointer into `frames`.
	 */
	const cl_frame_t *previous_frame;

	/**
	 * @brief All known server-side entities, parsed from received frames.
	 */
	cl_entity_t entities[MAX_ENTITIES];

	/**
	 * @brief The server entity which represents our local client (player).
	 * @details This is a pointer into `entities`, and may point to an entity we are chasing.
	 */
	cl_entity_t *entity;

	/**
	 * @brief A large buffer of entity states, shared by all parsed frames.
	 * @details Each frame maintains an index into this buffer. Entity states are parsed
	 * from the frame into this buffer, and then copied into the relevant entities.
	 */
	entity_state_t entity_states[ENTITY_STATE_BACKUP]; // accumulated each frame

	/**
	 * @brief The entity state index.
	 */
	uint32_t entity_state;

	/**
	 * @brief Our local client number, or index into `CS_CLIENTS`.
	 * @details This is equal to our entity number - 1, since the world is entity 0.
	 */
	int32_t client_num;

	/**
	 * @brief Suppressed messages count, for rate throttling.
	 */
	uint32_t suppress_count;

	/**
	 * @brief Clamped simulation time. This will always be between the previously received
	 * server frame time, and the most recently received server frame time.
	 */
	uint32_t time;

	/**
	 * @brief Unclamped simulation time. This will always reflect actual milliseconds since
	 * the game was launched. This is useful for effect durations and constant-time events. Affected by time_scale.
	 */
	uint32_t unclamped_time;

	/**
	 * @brief Unclamped simulation time. This will always reflect actual milliseconds since
	 * the player connected. This is useful for effect durations and constant-time events. Not affected by time_scale.
	 */
	uint32_t ticks;

	/**
	 * @brief The duration of the current frame, in milliseconds.
	 */
	uint32_t frame_msec;

	/**
	 * @brief The interpolation fraction for the current frame.
	 */
	float lerp;

	/**
	 * @brief The client view angles, derived from input, and sent to the server.
	 * @details These are cleared upon entering each level. The server sends a delta when
	 * necessary to correct for spawn and teleport direction changes.
	 */
	vec3_t angles;

	/**
	 * @brief True if we are viewing a demo.
	 */
	bool demo_server;

	/**
	 * @brief True if we are in 3rd person view, which disables client-side prediction.
	 */
	bool third_person;

	/**
	 * @brief The parsed configuration strings.
	 */
	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

	/**
	 * @brief The client loads BSP inline models for collision tracing and client-side prediction.
	 */
	cm_bsp_model_t *cm_models[MAX_MODELS];

	/**
	 * @brief The cache of known models contained within `config_strings`.
	 */
	r_model_t  *models[MAX_MODELS];

	/**
	 * @brief The cache of known images contained within `config_strings`.
	 */
	r_image_t  *images[MAX_IMAGES];

	/**
	 * @brief The cache of known sounds contained within `config_strings`.
	 */
	s_sample_t *sounds[MAX_SOUNDS];

	/**
	 * @brief The cache of known musics contained within `config_strings`.
	 */
	s_music_t  *musics[MAX_MUSICS];

	/**
	 * @brief The index into `config_strings` to check for file presence or download.
	 */
	int32_t precache_check;
} cl_client_t;

typedef enum {
	CL_UNINITIALIZED, // not initialized
	CL_DISCONNECTED, // not talking to a server
	CL_CONNECTING, // sending request packets to the server
	CL_CONNECTED, // netchan_t established, waiting for svc_server_data
	CL_LOADING, // loading media
	CL_ACTIVE // game views should be displayed
} cl_state_t;

typedef enum {
	KEY_UI = 1,
	KEY_CONSOLE,
	KEY_GAME,
	KEY_CHAT
} cl_key_dest_t;

typedef enum {
	SDL_SCANCODE_MOUSE1 = (SDL_SCANCODE_APP2 + 1),
	SDL_SCANCODE_MOUSE3,
	SDL_SCANCODE_MOUSE2,
	SDL_SCANCODE_MOUSE4,
	SDL_SCANCODE_MOUSE5,
	SDL_SCANCODE_MOUSE6,
	SDL_SCANCODE_MOUSE7,
	SDL_SCANCODE_MOUSE8,
	SDL_SCANCODE_MOUSE9,
	SDL_SCANCODE_MOUSE10,
	SDL_SCANCODE_MOUSE11,
	SDL_SCANCODE_MOUSE12,
	SDL_SCANCODE_MOUSE13,
	SDL_SCANCODE_MOUSE14,
	SDL_SCANCODE_MOUSE15,

	SDL_SCANCODE_MWHEELUP,
	SDL_SCANCODE_MWHEELDOWN
} SDL_Buttoncode;

enum {
	SDLK_MOUSE1 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE1),
	SDLK_MOUSE2 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE2),
	SDLK_MOUSE3 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE3),
	SDLK_MOUSE4 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE4),
	SDLK_MOUSE5 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE5),
	SDLK_MOUSE6 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE6),
	SDLK_MOUSE7 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE7),
	SDLK_MOUSE8 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MOUSE8)
};

#if defined(__APPLE__)
	#define KMOD_CLIPBOARD KMOD_GUI
#else
	#define KMOD_CLIPBOARD KMOD_CTRL
#endif

typedef struct {
	cl_key_dest_t dest;

	char *binds[SDL_NUM_SCANCODES];
	bool down[SDL_NUM_SCANCODES];
	bool latched[SDL_NUM_SCANCODES];
} cl_key_state_t;

typedef struct {
	bool team_chat;
} cl_chat_state_t;

typedef struct {
	int32_t x, y;
	int32_t old_x, old_y;
} cl_mouse_state_t;

typedef struct {
	file_t *file;
	char tempname[MAX_OS_PATH];
	char name[MAX_OS_PATH];
} cl_download_t;

// server information, for finding network games
typedef enum {
	SERVER_SOURCE_INTERNET,
	SERVER_SOURCE_USER,
	SERVER_SOURCE_BCAST
} cl_server_source_t;

typedef struct {
	net_addr_t addr;
	cl_server_source_t source;
	char hostname[48];
	char name[32];
	char gameplay[32];
	char error[128];
	int32_t clients;
	int32_t max_clients;
	uint32_t ping_time; // when we pinged the server
	int32_t ping; // server latency
} cl_server_info_t;

typedef struct {
	int32_t percent;
	const char *status;
	char mapshot[MAX_QPATH];
} cl_loading_t;

/**
 * Custom Notification names.
 */
typedef enum {
	NOTIFICATION_NONE,
	NOTIFICATION_SERVER_PARSED
} cl_notification_t;

/**
 * @brief The cl_static_t structure is persistent for the execution of the
 * game. It is only cleared when Cl_Init is called. It is not exposed to the
 * client game module, but is (rarely) visible to other subsystems (renderer).
 */
typedef struct {
	cl_state_t state;

	cl_key_state_t key_state;

	cl_mouse_state_t mouse_state;

	cl_chat_state_t chat_state;

	// connection information
	char server_name[MAX_OS_PATH]; // name of server to connect to
	uint32_t connect_time; // for connection retransmits

	net_chan_t net_chan; // network channel

	uint32_t challenge; // from the server to use for connecting
	uint32_t spawn_count;

	cl_loading_t loading; // loading status

	char download_url[MAX_OS_PATH]; // for http downloads
	cl_download_t download; // current download (udp or http)

	char demo_filename[MAX_OS_PATH];
	file_t *demo_file;

	GList *servers; // list of cl_server_info_t from all sources

	uint32_t broadcast_time; // time when last broadcast ping was sent

	struct cg_export_s *cgame;
} cl_static_t;

#ifdef __CL_LOCAL_H__
#endif /* __CL_LOCAL_H__ */
