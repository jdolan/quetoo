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

#include <Objectively/PointerArray.h>

#include "net/net_types.h"
#include "renderer/r_types.h"
#include "sound/s_types.h"
#include "ui/ui_types.h"

typedef struct {

  /**
   * @brief The movement command.
   */
  PlayerMoveCmd cmd;

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
    Vec3 origin;

    /**
     * @brief The prediction error for this command.
     */
    Vec3 error;
  } prediction;
} ClientCmd;

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
  PlayerState ps;

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
} ClientFrame;

typedef struct {

  /**
   * @brief The animation definition.
   */
  EntityAnimation animation;

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
   * @brief The interpolation fraction between `old_frame` and frame.
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
} ClientEntityAnimation;

typedef enum {
  TRAIL_PRIMARY,
  TRAIL_SECONDARY,
  TRAIL_TERTIARY,
  TRAIL_BUBBLE,

  TRAIL_ID_COUNT
} ClientTrailId;

typedef struct {

  /**
   * @brief Delta base state; used when no previous frame is available.
   */
  EntityState baseline;

  /**
   * @brief The current entity state.
   */
  EntityState current;

  /**
   * @brief The previous entity state; always valid, may be a copy of current.
   */
  EntityState prev;

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
  Vec3 trail_origins[TRAIL_ID_COUNT];

  /**
   * @brief Torso animation state.
   */
  ClientEntityAnimation animation1;

  /**
   * @brief Legs animation state.
   */
  ClientEntityAnimation animation2;

  /**
   * @brief Interpolated origin.
   */
  Vec3 origin;

  /**
   * @brief The previous interpolated origin.
   */
  Vec3 previous_origin;

  /**
   * @brief Interpolated termination (for beams).
   */
  Vec3 termination;

  /**
   * @brief Interpolated angles.
   */
  Vec3 angles;

  /**
   * @brief Bounding box in model space.
   */
  Box3 bounds;

  /**
   * @brief Absolute bounding box in world space.
   */
  Box3 abs_bounds;

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
  Mat4 matrix;

  /**
   * @brief Inverse transform matrix, used for point contents tests.
   */
  Mat4 inverse_matrix;
} ClientEntity;

/**
 * @brief A circular buffer of recently sent `user_cmd_t` is maintained so that
 * we can always re-send the last 2 commands to counter packet loss, and so
 * that client-side prediction can verify its accuracy.
 * @remarks The buffer must span the round trip time, since a command is replayed until the
 * server acknowledges it. Commands are sent once per rendered frame, throttled to 4ms apart,
 * so 64 covered only 256ms at that ceiling, beyond which prediction froze the player in place.
 * 256 carries 1024ms at the same rate.
 */
#define CMD_BACKUP 256
#define CMD_MASK (CMD_BACKUP - 1)

/**
 * @brief Client side prediction output, produced by running sent but
 * unacknowledged `user_cmd_t`'s through the player movement code locally.
 */
typedef struct {

  struct {

    /**
     * @brief The predicted view origin.
     */
    Vec3 origin;

    /**
     * @brief The predicted view offset (ducking).
     */
    Vec3 offset;

    /**
     * @brief The predicted view angles (local movement + delta angles).
     */
    Vec3 angles;

    /**
     * @brief The predicted step offset.
     */
    float step_offset;
  } view;

  /**
   * @brief The ground trace for the predicted position.
   */
  CmTrace ground;

  /**
   * @brief The prediction error, interpolated over the current server frame.
   */
  Vec3 error;
} ClientPredictedState;

/**
 * @brief We accumulate a large buffer of entity states for each entity in order to calculate delta compression.
 */
#define ENTITY_STATE_BACKUP (PACKET_BACKUP * MAX_ENTITIES)
#define ENTITY_STATE_MASK (ENTITY_STATE_BACKUP - 1)

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
   * @brief Packets sent since the diagnostics last read and cleared it.
   */
  uint32_t packets;

  /**
   * @brief Packets dropped by the server, cumulative for this connection.
   */
  uint32_t dropped;

  /**
   * @brief Circular buffer of recently sent commands, enabling re-send for loss recovery and client-side prediction.
   */
  ClientCmd cmds[CMD_BACKUP];

  /**
   * @brief The predicted state (view origin, offset, angles, etc.) of the client.
   */
  ClientPredictedState predicted_state;

  /**
   * @brief The most recently interpolated server frame.
   */
  ClientFrame frame;

  /**
   * @brief Circular buffer of received server frames, used for delta-compression.
   */
  ClientFrame frames[PACKET_BACKUP];

  /**
   * @brief The delta frame for the currently received frame, or `NULL`. Pointer into `frames`.
   */
  const ClientFrame *delta_frame;

  /**
   * @brief The previously received sequential frame, or `NULL`. Pointer into `frames`.
   */
  const ClientFrame *previous_frame;

  /**
   * @brief All known server-side entities, parsed from received frames.
   */
  ClientEntity entities[MAX_ENTITIES];

  /**
   * @brief The server entity representing the local client (player). Pointer into `entities`; may point to a chasecam target.
   */
  ClientEntity *entity;

  /**
   * @brief Large shared buffer of entity states used for delta-compression across parsed frames.
   */
  EntityState entity_states[ENTITY_STATE_BACKUP];

  /**
   * @brief The entity state index for parsing server frames.
   */
  uint32_t entity_state;

  /**
   * @brief Clamped simulation time, always between the previous and most recent server frame times.
   */
  uint32_t time;

  /**
   * @brief Unclamped time in milliseconds since launch. Affected by `time_scale`; useful for effect durations.
   */
  uint32_t unclamped_time;

  /**
   * @brief The time each client was last heard speaking, for the voice indicator.
   */
  uint32_t voice_time[MAX_CLIENTS];

  /**
   * @brief Unclamped time in milliseconds since the player connected. Not affected by `time_scale`.
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
  Vec3 angles;

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
  CmBspModel *cm_models[MAX_MODELS];

  /**
   * @brief Renderer models resolved from `config_strings`.
   */
  RenderModel *models[MAX_MODELS];

  /**
   * @brief Sound samples resolved from `config_strings`.
   */
  SoundSample *sounds[MAX_SOUNDS];

  /**
   * @brief Music tracks resolved from `config_strings`.
   */
  SoundMusic *musics[MAX_MUSICS];

  /**
   * @brief Index into `config_strings` used to verify file presence or initiate downloads.
   */
  int32_t precache_check;
} Client;

typedef enum {
  CL_UNINITIALIZED,
  CL_DISCONNECTED,
  CL_CONNECTING,
  CL_CONNECTED,
  CL_LOADING,
  CL_ACTIVE
} ClientState;

typedef enum {
  KEY_UI = 1,
  KEY_CONSOLE,
  KEY_GAME,
  KEY_CHAT
} ClientKeyDest;

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
  ClientKeyDest dest;

  /**
   * @brief Key binding strings, indexed by `SDL_Scancode`.
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
} ClientKeyState;

typedef struct {

  /**
   * @brief Current relative mouse delta, in sensitivity-scaled units.
   */
  float x, y;

  /**
   * @brief Previous relative mouse delta, for interpolation.
   */
  float old_x, old_y;
} ClientMouseState;

typedef struct {

  /**
   * @brief The download file handle.
   */
  File *file;

  /**
   * @brief Temporary file path used during download.
   */
  char tempname[MAX_OS_PATH];

  /**
   * @brief Final destination file path.
   */
  char name[MAX_OS_PATH];
} ClientDownload;

/**
 * @brief The network server sources.
 */
typedef enum {
  SERVER_SOURCE_INTERNET,
  SERVER_SOURCE_USER,
  SERVER_SOURCE_BCAST
} ClientServerSource;

/**
 * @brief The server information type, hydrated by querying server status via the browser.
 */
typedef struct {

  /**
   * @brief The server network address.
   */
  NetAddr addr;

  /**
   * @brief How this server was discovered.
   */
  ClientServerSource source;

  /**
   * @brief The server hostname.
   */
  char hostname[48];

  /**
   * @brief The server's identity for the lifetime of its process, or empty if it did not
   * report one. Distinct servers never share it, so one reached by two addresses is one entry.
   */
  char guid[37];

  /**
   * @brief The server name (map/game title).
   */
  char name[32];

  /**
   * @brief The gameplay mode name.
   */
  char gameplay[32];

  /**
   * @brief The movement name.
   */
  char movement[32];

  /**
   * @brief Error string if the server could not be queried.
   */
  char error[128];

  /**
   * @brief The current number of connected clients.
   */
  int32_t clients;

  /**
   * @brief The number of connected clients that are bots.
   */
  int32_t bots;

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

  /**
   * @brief Exponentially smoothed ping, retained across refreshes to damp the
   * per-request variance from one-shot status replies.
   */
  int32_t ping_smoothed;
} ClientServerInfo;

/**
 * @brief The client's view of the currently connected server.
 */
typedef struct {
  /**
   * @brief Name or address of the server to connect to.
   */
  char address[MAX_OS_PATH];

  /**
   * @brief The address `address` last resolved to, so that its status can be looked up without
   * resolving it again.
   */
  NetAddr addr;

  /**
   * @brief System time of last connection attempt, for retransmits.
   */
  uint32_t connect_time;

  /**
   * @brief Challenge value received from the server, used when connecting.
   */
  uint32_t challenge;

  /**
   * @brief Server spawn count, used to detect map changes.
   */
  uint32_t spawn_count;
} ClientServer;

/**
 * @brief Demo recording and playback state.
 */
typedef struct {
  /**
   * @brief The demo filename being recorded or played back.
   */
  char filename[MAX_OS_PATH];

  /**
   * @brief The demo file handle.
   */
  File *file;

  /**
   * @brief A copy of the header written to `file`, kept in memory because `file` is opened
   * write-only: Cl_Stop_f patches this copy and rewrites it, rather than reading it back.
   */
  DemoHeader header;

  /**
   * @brief The frame number most recently written to `file`, or `-1`. Guards against writing a
   * duplicate record when a received packet carried no new `SV_CMD_FRAME`.
   */
  int32_t last_frame_num;

  /**
   * @brief The (absolute, server-since-map-load) frame number of the first frame written this
   * recording, or `-1` before it's known. Every frame_num persisted to the file - the per-message
   * prefix, the synthesized SV_CMD_FRAME's own field, and the keyframe index - is written
   * relative to this, so the file's numbering always starts at 0 regardless of when in the map's
   * lifetime `record` was issued. Without this, duration and Sv_SeekDemo's millis-to-frame_num
   * conversion would be measured against the wrong origin whenever recording didn't start at
   * frame 0 (i.e. always, in practice).
   */
  int32_t start_frame_num;

  /**
   * @brief Raw bytes of any non-`SV_CMD_FRAME` commands (chat, centerprint, temp entities,
   * sounds, etc.) captured verbatim by `Cl_ParseServerMessage` so they ride along with the next
   * recorded frame, persisting across packets until a frame flushes them (see `event_size`'s
   * comment in `Cl_ParseServerMessage`). Sized to `MAX_MSG_SIZE * 4`, matching the server's own
   * `MAX_DATAGRAM_SIZE`: a single busy tick's queued messages can be fragmented across that many
   * packets before any of them carries a new frame, and this must not lose data to its own
   * capacity before Cl_WriteDemoMessage gets a chance to decide what actually fits in one chunk.
   */
  byte event_buffer[MAX_MSG_SIZE * 4];

  /**
   * @brief The number of valid bytes in `event_buffer`.
   */
  size_t event_size;

  /**
   * @brief The total duration of the demo currently being played back, in milliseconds, or `0`
   * if not viewing a demo. Received once via `SV_CMD_DEMO_INFO` when connecting to a demo relay.
   * Lives here (on `cls`, not `cl`) rather than alongside `cl.demo_server` because it arrives in
   * the same packet as, and just ahead of, the relayed `SV_CMD_SERVER_DATA` that triggers
   * Cl_ClearState's memset of `cl` - storing it there would have it wiped out immediately after
   * being set.
   */
  int32_t duration;

  /**
   * @brief True if demo playback is believed to be paused, toggled locally by the `demo_pause`
   * command (see `Cl_DemoPause_f`). The server is the actual authority on pause state, but
   * tracking it client-side avoids a round trip just to gate the paused-playback controls UI,
   * the mouse grab, and UI event dispatch.
   */
  bool paused;

  /**
   * @brief Per-frame index accumulated in memory while recording, flushed at Cl_Stop_f.
   */
  DemoKeyframe *keyframes;

  /**
   * @brief The number of valid entries in `keyframes`.
   */
  size_t num_keyframes;

  /**
   * @brief The allocated capacity of `keyframes`.
   */
  size_t max_keyframes;
} ClientDemo;

/**
 * @brief Loading state tracking.
 */
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
} ClientLoading;

/**
 * Custom Notification names.
 */
typedef enum {
  NOTIFICATION_NONE,
  NOTIFICATION_SERVER_PARSED,
  NOTIFICATION_ENTITY_PARSED,
  NOTIFICATION_ENTITY_SELECTED,
  NOTIFICATION_LEADERBOARD_FETCHED,
  NOTIFICATION_STATS_FETCHED,
  NOTIFICATION_EDITOR_SELECTION_CYCLE,
} ClientNotification;

/**
 * @brief The `ClientStatic` structure is persistent for the execution of the
 * game. It is only cleared when `Cl_Init` is called. It is not exposed to the
 * client game module.
 */
typedef struct {

  /**
   * @brief The current client connection state.
   */
  ClientState state;

  /**
   * @brief The key binding and press state.
   */
  ClientKeyState key_state;

  /**
   * @brief The mouse position state.
   */
  ClientMouseState mouse_state;

  /**
   * @brief List of `ClientServerInfo` discovered from all sources.
   */
  PointerArray *servers;

  /**
   * @brief System time when the last LAN broadcast ping was sent.
   */
  uint32_t broadcast_time;

  /**
   * @brief The current server.
   */
  ClientServer server;

  /**
   * @brief The network channel to the current server.
   */
  NetChan net_chan;

  /**
   * @brief Media loading progress state.
   */
  ClientLoading loading;

  /**
   * @brief Active download state.
   */
  ClientDownload download;

  /**
   * @brief The demo, for playback and recording.
   */
  ClientDemo demo;

  /**
   * @brief The loaded client game module exports.
   */
  struct ClientGameExport *cgame;
} ClientStatic;

#if defined(__CL_LOCAL_H__)
#endif
