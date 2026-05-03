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

  /**
   * @brief The movement command.
   */
  pm_cmd_t cmd;

  /**
   * @brief Simulation time when the command was sent.
   */
  uint32_t time;

  /**
   * @brief System time when the command was sent.
   */
  uint32_t timestamp;
  struct {

    /**
     * @brief The simulation time when prediction was run.
     */
    uint32_t time;

    /**
     * @brief The predicted origin for this command.
     */
    vec3_t origin;

    /**
     * @brief The prediction error for this command.
     */
    vec3_t error;
  } prediction;
} cl_cmd_t;

typedef struct {

  /**
   * @brief Sequential frame identifier, used for delta.
   */
  int32_t frame_num;

  /**
   * @brief The delta frame number; negative values indicate no delta.
   */
  int32_t delta_frame_num;

  /**
   * @brief The player state.
   */
  player_state_t ps;

  /**
   * @brief The number of entities in the frame.
   */
  int32_t num_entities;

  /**
   * @brief Non-masked index into `cl.entity_states`.
   */
  uint32_t entity_state;

  /**
   * @brief False if delta parsing failed.
   */
  bool valid;

  /**
   * @brief True if this frame has been interpolated one or more times.
   */
  bool interpolated;

  /**
   * @brief Simulation time for which the frame is valid.
   */
  uint32_t time;
} cl_frame_t;

typedef struct {

  /**
   * @brief The animation definition.
   */
  entity_animation_t animation;

  /**
   * @brief The time when this animation started.
   */
  uint32_t time;

  /**
   * @brief The current frame index.
   */
  int32_t frame;

  /**
   * @brief The previous frame index.
   */
  int32_t old_frame;

  /**
   * @brief The interpolation fraction between old_frame and frame.
   */
  float lerp;

  /**
   * @brief The fraction of the animation that has elapsed.
   */
  float fraction;

  /**
   * @brief True if the animation is playing in reverse.
   */
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

  /**
   * @brief Delta base state; used when no previous frame is available.
   */
  entity_state_t baseline;

  /**
   * @brief The current entity state.
   */
  entity_state_t current;

  /**
   * @brief The previous entity state; always valid, may be a copy of current.
   */
  entity_state_t prev;

  /**
   * @brief The last frame in which this entity was seen.
   */
  int32_t frame_num;

  /**
   * @brief Timestamp for intermittent effects.
   */
  uint32_t timestamp;

  /**
   * @brief Trail emission origins, one per trail ID.
   */
  vec3_t trail_origins[TRAIL_ID_COUNT];

  /**
   * @brief Torso animation state.
   */
  cl_entity_animation_t animation1;

  /**
   * @brief Legs animation state.
   */
  cl_entity_animation_t animation2;

  /**
   * @brief Interpolated origin.
   */
  vec3_t origin;

  /**
   * @brief The previous interpolated origin.
   */
  vec3_t previous_origin;

  /**
   * @brief Interpolated termination (for beams).
   */
  vec3_t termination;

  /**
   * @brief Interpolated angles.
   */
  vec3_t angles;

  /**
   * @brief Bounding box in model space.
   */
  box3_t bounds;

  /**
   * @brief Absolute bounding box in world space.
   */
  box3_t abs_bounds;

  /**
   * @brief Ideal leg yaw (player models only).
   */
  float legs_yaw;

  /**
   * @brief Current interpolated leg yaw (player models only).
   */
  float legs_current_yaw;

  /**
   * @brief Interpolated vertical step offset for stair smoothing.
   */
  float step_offset;

  /**
   * @brief Snapped transform matrix, used for traces.
   */
  mat4_t matrix;

  /**
   * @brief Inverse transform matrix, used for point contents tests.
   */
  mat4_t inverse_matrix;
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

    /**
     * @brief The predicted view origin.
     */
    vec3_t origin;

    /**
     * @brief The predicted view offset (ducking).
     */
    vec3_t offset;

    /**
     * @brief The predicted view angles (local movement + delta angles).
     */
    vec3_t angles;

    /**
     * @brief The predicted step offset.
     */
    float step_offset;
  } view;

  /**
   * @brief The ground trace for the predicted position.
   */
  cm_trace_t ground;

  /**
   * @brief The prediction error, interpolated over the current server frame.
   */
  vec3_t error;
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

  /**
   * @brief Total frames rendered during a timedemo run.
   */
  uint32_t time_demo_frames;

  /**
   * @brief System time at which the current timedemo run began.
   */
  uint32_t time_demo_start;

  /**
   * @brief Circular sample buffer of frames-per-second counts.
   */
  uint16_t frame_counter[STAT_COUNTER_SAMPLE_COUNT];

  /**
   * @brief Circular sample buffer of packets-per-second counts.
   */
  uint16_t packet_counter[STAT_COUNTER_SAMPLE_COUNT];

  /**
   * @brief Current write index and valid sample count for the stat counters.
   */
  uint8_t sample_index, sample_count;

  /**
   * @brief Circular sample buffer of frame durations in milliseconds.
   */
  uint8_t frametime_counter[FRAMETIME_COUNTER_SAMPLE_COUNT];

  /**
   * @brief Current write index and valid sample count for frametime counters.
   */
  uint8_t frametime_index, frametime_count;

  /**
   * @brief Circular buffer of recently sent commands, enabling re-send for loss recovery and client-side prediction.
   */
  cl_cmd_t cmds[CMD_BACKUP];

  /**
   * @brief The predicted state (view origin, offset, angles, etc.) of the client.
   */
  cl_predicted_state_t predicted_state;

  /**
   * @brief The most recently interpolated server frame.
   */
  cl_frame_t frame;

  /**
   * @brief Circular buffer of received server frames, used for delta-compression.
   */
  cl_frame_t frames[PACKET_BACKUP];

  /**
   * @brief The delta frame for the currently received frame, or `NULL`. Pointer into `frames`.
   */
  const cl_frame_t *delta_frame;

  /**
   * @brief The previously received sequential frame, or `NULL`. Pointer into `frames`.
   */
  const cl_frame_t *previous_frame;

  /**
   * @brief All known server-side entities, parsed from received frames.
   */
  cl_entity_t entities[MAX_ENTITIES];

  /**
   * @brief The server entity representing the local client (player). Pointer into `entities`; may point to a chasecam target.
   */
  cl_entity_t *entity;

  /**
   * @brief Large shared buffer of entity states used for delta-compression across parsed frames.
   */
  entity_state_t entity_states[ENTITY_STATE_BACKUP];

  /**
   * @brief The entity state index for parsing server frames.
   */
  uint32_t entity_state;

  /**
   * @brief Clamped simulation time, always between the previous and most recent server frame times.
   */
  uint32_t time;

  /**
   * @brief Unclamped time in milliseconds since launch. Affected by time_scale; useful for effect durations.
   */
  uint32_t unclamped_time;

  /**
   * @brief Unclamped time in milliseconds since the player connected. Not affected by time_scale.
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
   * @brief The client view angles derived from input, sent to the server. Cleared on level entry.
   */
  vec3_t angles;

  /**
   * @brief True if the client is viewing a demo.
   */
  bool demo_server;

  /**
   * @brief True if the client is in third-person view (disables client-side prediction).
   */
  bool third_person;

  /**
   * @brief The parsed configuration strings.
   */
  char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

  /**
   * @brief Collision BSP inline models loaded for client-side prediction.
   */
  cm_bsp_model_t *cm_models[MAX_MODELS];

  /**
   * @brief Renderer models resolved from `config_strings`.
   */
  r_model_t *models[MAX_MODELS];

  /**
   * @brief Renderer images resolved from `config_strings`.
   */
  r_image_t *images[MAX_IMAGES];

  /**
   * @brief Sound samples resolved from `config_strings`.
   */
  s_sample_t *sounds[MAX_SOUNDS];

  /**
   * @brief Music tracks resolved from `config_strings`.
   */
  s_music_t *musics[MAX_MUSICS];

  /**
   * @brief Index into `config_strings` used to verify file presence or initiate downloads.
   */
  int32_t precache_check;
} cl_client_t;

typedef enum {
  CL_UNINITIALIZED,
  CL_DISCONNECTED,
  CL_CONNECTING,
  CL_CONNECTED,
  CL_LOADING,
  CL_ACTIVE
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

  /**
   * @brief The current key destination (UI, console, game, chat).
   */
  cl_key_dest_t dest;

  /**
   * @brief Key binding strings, indexed by SDL_Scancode.
   */
  char *binds[SDL_SCANCODE_COUNT];

  /**
   * @brief True if the key is currently held down.
   */
  bool down[SDL_SCANCODE_COUNT];

  /**
   * @brief True if the key was pressed this frame.
   */
  bool latched[SDL_SCANCODE_COUNT];
} cl_key_state_t;

typedef struct {

  /**
   * @brief True if the current chat message is for team only.
   */
  bool team_chat;
} cl_chat_state_t;

typedef struct {

  /**
   * @brief Current mouse position in window coordinates.
   */
  int32_t x, y;

  /**
   * @brief Previous mouse position in window coordinates.
   */
  int32_t old_x, old_y;
} cl_mouse_state_t;

typedef struct {

  /**
   * @brief The download file handle.
   */
  file_t *file;

  /**
   * @brief Temporary file path used during download.
   */
  char tempname[MAX_OS_PATH];

  /**
   * @brief Final destination file path.
   */
  char name[MAX_OS_PATH];
} cl_download_t;

// server information, for finding network games
typedef enum {
  SERVER_SOURCE_INTERNET,
  SERVER_SOURCE_USER,
  SERVER_SOURCE_BCAST
} cl_server_source_t;

typedef struct {

  /**
   * @brief The server network address.
   */
  net_addr_t addr;

  /**
   * @brief How this server was discovered.
   */
  cl_server_source_t source;

  /**
   * @brief The server hostname.
   */
  char hostname[48];

  /**
   * @brief The server name (map/game title).
   */
  char name[32];

  /**
   * @brief The gameplay mode name.
   */
  char gameplay[32];

  /**
   * @brief Error string if the server could not be queried.
   */
  char error[128];

  /**
   * @brief The current number of connected clients.
   */
  int32_t clients;

  /**
   * @brief The maximum number of clients.
   */
  int32_t max_clients;

  /**
   * @brief System time when the server was last pinged.
   */
  uint32_t ping_time;

  /**
   * @brief Measured round-trip latency to the server in milliseconds.
   */
  int32_t ping;
} cl_server_info_t;

typedef struct {

  /**
   * @brief Load progress from 0 to 100.
   */
  int32_t percent;

  /**
   * @brief Human-readable status string describing what is loading.
   */
  const char *status;

  /**
   * @brief Path to the mapshot image for the current map.
   */
  char mapshot[MAX_QPATH];
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

  /**
   * @brief The current client connection state.
   */
  cl_state_t state;

  /**
   * @brief The key binding and press state.
   */
  cl_key_state_t key_state;

  /**
   * @brief The mouse position state.
   */
  cl_mouse_state_t mouse_state;

  /**
   * @brief The chat mode state.
   */
  cl_chat_state_t chat_state;

  /**
   * @brief Name or address of the server to connect to.
   */
  char server_name[MAX_OS_PATH];

  /**
   * @brief System time of last connection attempt, for retransmits.
   */
  uint32_t connect_time;

  /**
   * @brief The network channel to the server.
   */
  net_chan_t net_chan;

  /**
   * @brief Challenge value received from the server, used when connecting.
   */
  uint32_t challenge;

  /**
   * @brief Server spawn count, used to detect map changes.
   */
  uint32_t spawn_count;

  /**
   * @brief Media loading progress state.
   */
  cl_loading_t loading;

  /**
   * @brief Active download state.
   */
  cl_download_t download;

  /**
   * @brief The demo filename being recorded or played back.
   */
  char demo_filename[MAX_OS_PATH];

  /**
   * @brief The demo file handle.
   */
  file_t *demo_file;

  /**
   * @brief List of `cl_server_info_t` discovered from all sources.
   */
  GList *servers;

  /**
   * @brief System time when the last LAN broadcast ping was sent.
   */
  uint32_t broadcast_time;

  /**
   * @brief The loaded client game module exports.
   */
  struct cg_export_s *cgame;
} cl_static_t;

#if defined(__CL_LOCAL_H__)
#endif /* __CL_LOCAL_H__ */
