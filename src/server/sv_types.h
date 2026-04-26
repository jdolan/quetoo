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

#include "common/common.h"

#include "game/game.h"

#if defined(__SV_LOCAL_H__)

/**
 * @brief The server entity type.
 */
typedef struct {
  /**
   * @brief Baseline entity state for delta compression.
   */
  entity_state_t baseline;
  /**
   * @brief World sector for entity list management.
   */
  struct sv_sector_s *sector;
  /**
   * @brief World-space transform for collision tests.
   */
  mat4_t matrix;
  /**
   * @brief Inverse of matrix, used to bring traces into model space.
   */
  mat4_t inverse_matrix;
} sv_entity_t;

/**
 * @brief The `sv_server_t` struct is wiped at each level load.
 */
typedef struct {
  /**
   * @brief Simulation time in ms; always frame_num * 1000 / QUETOO_TICK_RATE.
   */
  uint32_t time;
  /**
   * @brief Current simulation frame number.
   */
  uint32_t frame_num;
  /**
   * @brief Map name, e.g. "maps/edge".
   */
  char name[MAX_QPATH];
  /**
   * @brief Collision models; [0] is worldspawn, rest are inline models.
   */
  cm_bsp_model_t *cm_models[MAX_MODELS];
  /**
   * @brief Config strings enumerating all loaded assets (models, sounds, skins, etc.).
   */
  char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];
  /**
   * @brief Server-side entity array.
   */
  sv_entity_t entities[MAX_ENTITIES];
  /**
   * @brief Multicast buffer, accumulated and delivered each server frame.
   */
  mem_buf_t multicast;
  /**
   * @brief Backing storage for the multicast buffer.
   */
  byte multicast_buffer[MAX_MSG_SIZE];
  /**
   * @brief Open demo file for demo playback, or NULL during live gameplay.
   */
  file_t *demo_file;
} sv_server_t;

/**
 * @brief The server's client frame type. For each server frame, a unique client
 * frame is authored, containing only the relevant updates for that client. This
 * structure is sent as the header of `SV_CMD_FRAME`.
 */
typedef struct {
  /**
   * @brief Player state snapshot for this frame.
   */
  player_state_t ps;
  /**
   * @brief Number of delta-compressed entities in this frame.
   */
  int16_t num_entities;
  /**
   * @brief Index into the entity state circular buffer.
   */
  uint32_t entity_state;
  /**
   * @brief Server time when this frame was dispatched, used to calculate ping.
   */
  uint32_t sent_time;
} sv_client_frame_t;

/**
 * @brief Clients are dropped after 20 seconds without receiving a packet.
 */
#define SV_TIMEOUT 20

/**
 * @brief Frame latency accounting, used to estimate ping.
 */
#define SV_CLIENT_LATENCY_COUNT 16

/**
 * @brief User movement command duration is inspected regularly to ensure that
 * they are not cheating. If their movement is too far out of sync with the
 * server's clock, we take notice and eventually kick them.
 */
#define CMD_MSEC_CHECK_INTERVAL 1000
#define CMD_MSEC_ALLOWABLE_DRIFT (CMD_MSEC_CHECK_INTERVAL * 1.5)
#define CMD_MSEC_MAX_DRIFT_ERRORS 10

/**
 * @brief Client states.
 */
typedef enum {
  SV_CLIENT_FREE,
  SV_CLIENT_CONNECTED,
  SV_CLIENT_ACTIVE
} sv_client_state_t;

/**
 * @brief The maximum size of a client's datagram buffer.
 */
#define MAX_DATAGRAM_SIZE (MAX_MSG_SIZE * 4)

/**
 * @brief Represents the bounds of an individual client message within the
 * buffered datagram for a given frame. Datagrams are packetized along message
 * bounds and transmitted as fragments when necessary.
 */
typedef struct {
  /**
   * @brief Byte offset of this message in the datagram buffer.
   */
  size_t offset;
  /**
   * @brief Byte length of this message.
   */
  size_t len;
} sv_client_message_t;

/**
 * @brief A datagram structure that maintains individual message offsets so
 * that it may be safely fragmented for delivery.
 */
typedef struct {
  /**
   * @brief Managed-size buffer wrapping data[].
   */
  mem_buf_t buffer;
  /**
   * @brief Raw message storage for this frame's datagram.
   */
  byte data[MAX_DATAGRAM_SIZE];
  /**
   * @brief List of sv_client_message_t bounds for safe fragmentation.
   */
  GList *messages;
} sv_client_datagram_t;

/**
 * @brief Tracks a client's HTTP file download connection.
 */
typedef struct {
  int32_t socket;
  char request[1024];
  int32_t request_len;
  byte *data;
  int32_t size;
  int32_t count;
} sv_http_client_t;

/**
 * @brief The server client type.
 */
typedef struct {
  /**
   * @brief Connection state of this client slot.
   */
  sv_client_state_t state;
  /**
   * @brief Raw user-info key-value string.
   */
  char user_info[MAX_INFO_STRING_STRING];
  /**
   * @brief Player name extracted from user_info, stripped of color codes.
   */
  char name[32];
  /**
   * @brief Minimum print level for chat messages delivered to this client.
   */
  int32_t message_level;
  /**
   * @brief Last acknowledged frame number for delta compression; -1 sends baselines.
   */
  int32_t last_frame;
  /**
   * @brief Accumulated movement command duration; exceeding server elapsed time indicates cheating.
   */
  uint32_t cmd_msec;
  /**
   * @brief Consecutive anti-cheat violation count for cmd_msec drift.
   */
  uint16_t cmd_msec_errors;
  /**
   * @brief Ring buffer of recent per-frame delivery timestamps for ping estimation.
   */
  uint32_t frame_latency[SV_CLIENT_LATENCY_COUNT];
  /**
   * @brief Estimated round-trip latency in milliseconds.
   */
  int32_t ping;
  /**
   * @brief Per-frame datagram; accumulated, packetized and delivered each server frame.
   */
  sv_client_datagram_t datagram;
  /**
   * @brief Circular buffer of sent frames; referenced by client for delta compression.
   */
  sv_client_frame_t frames[PACKET_BACKUP];
  /**
   * @brief HTTP file download connection for this client.
   */
  sv_http_client_t http;
  /**
   * @brief UDP network channel to this client.
   */
  net_chan_t net_chan;
  /**
   * @brief Server time of last received packet, used to detect timeouts.
   */
  uint32_t last_message;
} sv_client_t;

/**
 * @brief Public servers may broadcast their status to as many as 8 master
 * servers.
 */
#define MAX_MASTERS  8

/**
 * @brief Challenges are a request for a connection. The client must receive
 * and then re-use a valid challenge in order to receive a client slot. This
 * provides basic protection against simple UDP DoS attacks.
 */
typedef struct {
  net_addr_t addr;
  uint32_t challenge;
  uint32_t time;
} sv_challenge_t;

/**
 * @brief `MAX_CHALLENGES` is large to prevent a denial of service attack that
 * could cycle all of them out before legitimate users connected.
 */
#define MAX_CHALLENGES 1024

/**
 * @brief Server states.
 */
typedef enum {
  SV_UNINITIALIZED,
  SV_INITIALIZED,
  SV_LOADING,
  SV_ACTIVE_GAME,
  SV_ACTIVE_DEMO
} sv_state_t;

/**
 * @brief The `sv_static_t` structure is persistent for the execution of the
 * game. It is only cleared when `Sv_Init` is called. It is not exposed to the
 * game module.
 */
typedef struct {
  /**
   * @brief Current server lifecycle state.
   */
  sv_state_t state;
  /**
   * @brief Dynamically allocated array of connected client slots.
   */
  sv_client_t *clients;
  /**
   * @brief Circular buffer of entity states for delta compression across all clients.
   */
  entity_state_t *entity_states;
  /**
   * @brief Length of entity_states; always PACKET_BACKUP * MAX_ENTITIES.
   */
  uint32_t num_entity_states;
  /**
   * @brief Next free index in entity_states for newly spawned entities.
   */
  uint32_t next_entity_state;
  /**
   * @brief Configured master server addresses for heartbeat broadcasts.
   */
  net_addr_t masters[MAX_MASTERS];
  /**
   * @brief Server time after which the next heartbeat is sent to master servers.
   */
  uint32_t next_heartbeat;
  /**
   * @brief Pending connection challenges for DoS mitigation.
   */
  sv_challenge_t challenges[MAX_CHALLENGES];
  /**
   * @brief Incremented at each map load to validate late-arriving connection handshakes.
   */
  uint32_t spawn_count;
  /**
   * @brief Exported API from the loaded game module.
   */
  g_export_t *game;
} sv_static_t;

#endif /* __SV_LOCAL_H__ */
