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
  pm_cmd_t cmd; ///< The movement command.
  uint32_t time; ///< Simulation time when the command was sent.
  uint32_t timestamp; ///< System time when the command was sent.
  struct {
    uint32_t time; ///< The simulation time when prediction was run.
    vec3_t origin; ///< The predicted origin for this command.
    vec3_t error; ///< The prediction error for this command.
  } prediction;
} cl_cmd_t;

typedef struct {
  int32_t frame_num; ///< Sequential frame identifier, used for delta.
  int32_t delta_frame_num; ///< The delta frame number; negative values indicate no delta.
  player_state_t ps; ///< The player state.
  int32_t num_entities; ///< The number of entities in the frame.
  uint32_t entity_state; ///< Non-masked index into `cl.entity_states`.
  bool valid; ///< False if delta parsing failed.
  bool interpolated; ///< True if this frame has been interpolated one or more times.
  uint32_t time; ///< Simulation time for which the frame is valid.
} cl_frame_t;

typedef struct {
  entity_animation_t animation; ///< The animation definition.
  uint32_t time; ///< The time when this animation started.
  int32_t frame; ///< The current frame index.
  int32_t old_frame; ///< The previous frame index.
  float lerp; ///< The interpolation fraction between old_frame and frame.
  float fraction; ///< The fraction of the animation that has elapsed.
  bool reverse; ///< True if the animation is playing in reverse.
} cl_entity_animation_t;

typedef enum {
  TRAIL_PRIMARY,
  TRAIL_SECONDARY,
  TRAIL_TERTIARY,
  TRAIL_BUBBLE,

  TRAIL_ID_COUNT
} cl_trail_id_t;

typedef struct {
  entity_state_t baseline; ///< Delta base state; used when no previous frame is available.
  entity_state_t current; ///< The current entity state.
  entity_state_t prev; ///< The previous entity state; always valid, may be a copy of current.

  int32_t frame_num; ///< The last frame in which this entity was seen.

  uint32_t timestamp; ///< Timestamp for intermittent effects.

  vec3_t trail_origins[TRAIL_ID_COUNT]; ///< Trail emission origins, one per trail ID.

  cl_entity_animation_t animation1; ///< Torso animation state.
  cl_entity_animation_t animation2; ///< Legs animation state.

  vec3_t origin; ///< Interpolated origin.
  vec3_t previous_origin; ///< The previous interpolated origin.
  vec3_t termination; ///< Interpolated termination (for beams).
  vec3_t angles; ///< Interpolated angles.
  box3_t bounds; ///< Bounding box in model space.
  box3_t abs_bounds; ///< Absolute bounding box in world space.

  float legs_yaw; ///< Ideal leg yaw (player models only).
  float legs_current_yaw; ///< Current interpolated leg yaw (player models only).
  float step_offset; ///< Interpolated vertical step offset for stair smoothing.

  mat4_t matrix; ///< Snapped transform matrix, used for traces.
  mat4_t inverse_matrix; ///< Inverse transform matrix, used for point contents tests.
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
    vec3_t origin; ///< The predicted view origin.
    vec3_t offset; ///< The predicted view offset (ducking).
    vec3_t angles; ///< The predicted view angles (local movement + delta angles).
    float step_offset; ///< The predicted step offset.
  } view;

  cm_trace_t ground; ///< The ground trace for the predicted position.

  vec3_t error; ///< The prediction error, interpolated over the current server frame.
} cl_predicted_state_t;

/**
 * @brief We accumulate a large buffer of entity states for each entity in order to calculate delta compression.
 */
#define ENTITY_STATE_BACKUP (PACKET_BACKUP * MAX_ENTITIES)
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
  uint32_t time_demo_frames; ///< Total frames rendered during a timedemo run.
  uint32_t time_demo_start; ///< System time at which the current timedemo run began.

  uint16_t frame_counter[STAT_COUNTER_SAMPLE_COUNT]; ///< Circular sample buffer of frames-per-second counts.
  uint16_t packet_counter[STAT_COUNTER_SAMPLE_COUNT]; ///< Circular sample buffer of packets-per-second counts.
  uint8_t sample_index, sample_count; ///< Current write index and valid sample count for the stat counters.

  uint8_t frametime_counter[FRAMETIME_COUNTER_SAMPLE_COUNT]; ///< Circular sample buffer of frame durations in milliseconds.
  uint8_t frametime_index, frametime_count; ///< Current write index and valid sample count for frametime counters.

  cl_cmd_t cmds[CMD_BACKUP]; ///< Circular buffer of recently sent commands, enabling re-send for loss recovery and client-side prediction.
  cl_predicted_state_t predicted_state; ///< The predicted state (view origin, offset, angles, etc.) of the client.
  cl_frame_t frame; ///< The most recently interpolated server frame.
  cl_frame_t frames[PACKET_BACKUP]; ///< Circular buffer of received server frames, used for delta-compression.
  const cl_frame_t *delta_frame; ///< The delta frame for the currently received frame, or `NULL`. Pointer into `frames`.
  const cl_frame_t *previous_frame; ///< The previously received sequential frame, or `NULL`. Pointer into `frames`.
  cl_entity_t entities[MAX_ENTITIES]; ///< All known server-side entities, parsed from received frames.
  cl_entity_t *entity; ///< The server entity representing the local client (player). Pointer into `entities`; may point to a chasecam target.
  entity_state_t entity_states[ENTITY_STATE_BACKUP]; ///< Large shared buffer of entity states used for delta-compression across parsed frames.
  uint32_t entity_state; ///< The entity state index for parsing server frames.
  uint32_t time; ///< Clamped simulation time, always between the previous and most recent server frame times.
  uint32_t unclamped_time; ///< Unclamped time in milliseconds since launch. Affected by time_scale; useful for effect durations.
  uint32_t ticks; ///< Unclamped time in milliseconds since the player connected. Not affected by time_scale.
  uint32_t frame_msec; ///< The duration of the current frame, in milliseconds.
  float lerp; ///< The interpolation fraction for the current frame.
  vec3_t angles; ///< The client view angles derived from input, sent to the server. Cleared on level entry.
  bool demo_server; ///< True if the client is viewing a demo.
  bool third_person; ///< True if the client is in third-person view (disables client-side prediction).
  char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS]; ///< The parsed configuration strings.
  cm_bsp_model_t *cm_models[MAX_MODELS]; ///< Collision BSP inline models loaded for client-side prediction.
  r_model_t *models[MAX_MODELS]; ///< Renderer models resolved from `config_strings`.
  r_image_t *images[MAX_IMAGES]; ///< Renderer images resolved from `config_strings`.
  s_sample_t *sounds[MAX_SOUNDS]; ///< Sound samples resolved from `config_strings`.
  s_music_t *musics[MAX_MUSICS]; ///< Music tracks resolved from `config_strings`.
  cm_entity_t *entity_definitions[MAX_ENTITIES]; ///< Entity definitions resolved from `config_strings`.
  int32_t precache_check; ///< Index into `config_strings` used to verify file presence or initiate downloads.
} cl_client_t;

typedef enum {
  CL_UNINITIALIZED, ///< Not initialized.
  CL_DISCONNECTED, ///< Not talking to a server.
  CL_CONNECTING, ///< Sending request packets to the server.
  CL_CONNECTED, ///< Netchan established, waiting for svc_server_data.
  CL_LOADING, ///< Loading media.
  CL_ACTIVE ///< Game views are being displayed.
} cl_state_t;

typedef enum {
  KEY_UI = 1,
  KEY_CONSOLE,
  KEY_GAME,
  KEY_CHAT
} cl_key_dest_t;

typedef enum {
  SDL_SCANCODE_MOUSE1 = (SDL_SCANCODE_RESERVED + 1),
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
  #define SDL_KMOD_CLIPBOARD SDL_KMOD_GUI
#else
  #define SDL_KMOD_CLIPBOARD SDL_KMOD_CTRL
#endif

typedef struct {
  cl_key_dest_t dest; ///< The current key destination (UI, console, game, chat).
  char *binds[SDL_SCANCODE_COUNT]; ///< Key binding strings, indexed by SDL_Scancode.
  bool down[SDL_SCANCODE_COUNT]; ///< True if the key is currently held down.
  bool latched[SDL_SCANCODE_COUNT]; ///< True if the key was pressed this frame.
} cl_key_state_t;

typedef struct {
  bool team_chat; ///< True if the current chat message is for team only.
} cl_chat_state_t;

typedef struct {
  int32_t x, y; ///< Current mouse position in window coordinates.
  int32_t old_x, old_y; ///< Previous mouse position in window coordinates.
} cl_mouse_state_t;

typedef struct {
  file_t *file; ///< The download file handle.
  char tempname[MAX_OS_PATH]; ///< Temporary file path used during download.
  char name[MAX_OS_PATH]; ///< Final destination file path.
} cl_download_t;

// server information, for finding network games
typedef enum {
  SERVER_SOURCE_INTERNET,
  SERVER_SOURCE_USER,
  SERVER_SOURCE_BCAST
} cl_server_source_t;

typedef struct {
  net_addr_t addr; ///< The server network address.
  cl_server_source_t source; ///< How this server was discovered.
  char hostname[48]; ///< The server hostname.
  char name[32]; ///< The server name (map/game title).
  char gameplay[32]; ///< The gameplay mode name.
  char error[128]; ///< Error string if the server could not be queried.
  int32_t clients; ///< The current number of connected clients.
  int32_t max_clients; ///< The maximum number of clients.
  uint32_t ping_time; ///< System time when the server was last pinged.
  int32_t ping; ///< Measured round-trip latency to the server in milliseconds.
} cl_server_info_t;

typedef struct {
  int32_t percent; ///< Load progress from 0 to 100.
  const char *status; ///< Human-readable status string describing what is loading.
  char mapshot[MAX_QPATH]; ///< Path to the mapshot image for the current map.
} cl_loading_t;

/**
 * Custom Notification names.
 */
typedef enum {
  NOTIFICATION_NONE,
  NOTIFICATION_SERVER_PARSED,
  NOTIFICATION_ENTITY_PARSED,
  NOTIFICATION_ENTITY_SELECTED,
} cl_notification_t;

/**
 * @brief The `cl_static_t` structure is persistent for the execution of the
 * game. It is only cleared when `Cl_Init` is called. It is not exposed to the
 * client game module.
 */
typedef struct {
  cl_state_t state; ///< The current client connection state.

  cl_key_state_t key_state; ///< The key binding and press state.
  cl_mouse_state_t mouse_state; ///< The mouse position state.
  cl_chat_state_t chat_state; ///< The chat mode state.

  char server_name[MAX_OS_PATH]; ///< Name or address of the server to connect to.
  uint32_t connect_time; ///< System time of last connection attempt, for retransmits.

  net_chan_t net_chan; ///< The network channel to the server.

  uint32_t challenge; ///< Challenge value received from the server, used when connecting.
  uint32_t spawn_count; ///< Server spawn count, used to detect map changes.

  cl_loading_t loading; ///< Media loading progress state.

  cl_download_t download; ///< Active download state.

  char demo_filename[MAX_OS_PATH]; ///< The demo filename being recorded or played back.
  file_t *demo_file; ///< The demo file handle.

  GList *servers; ///< List of `cl_server_info_t` discovered from all sources.

  uint32_t broadcast_time; ///< System time when the last LAN broadcast ping was sent.

  struct cg_export_s *cgame; ///< The loaded client game module exports.
} cl_static_t;

#if defined(__CL_LOCAL_H__)
#endif /* __CL_LOCAL_H__ */
