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
  entity_state_t baseline;    ///< Baseline entity state for delta compression.
  struct sv_sector_s *sector; ///< World sector for entity list management.
  mat4_t matrix;              ///< World-space transform for collision tests.
  mat4_t inverse_matrix;      ///< Inverse of matrix, used to bring traces into model space.
} sv_entity_t;

/**
 * @brief The `sv_server_t` struct is wiped at each level load.
 */
typedef struct {
  uint32_t time;                                             ///< Simulation time in ms; always frame_num * 1000 / QUETOO_TICK_RATE.
  uint32_t frame_num;                                        ///< Current simulation frame number.
  char name[MAX_QPATH];                                      ///< Map name, e.g. "maps/edge".
  cm_bsp_model_t *cm_models[MAX_MODELS];                     ///< Collision models; [0] is worldspawn, rest are inline models.
  char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS]; ///< Config strings enumerating all loaded assets (models, sounds, skins, etc.).
  sv_entity_t entities[MAX_ENTITIES];                        ///< Server-side entity array.
  mem_buf_t multicast;                                       ///< Multicast buffer, accumulated and delivered each server frame.
  byte multicast_buffer[MAX_MSG_SIZE];                       ///< Backing storage for the multicast buffer.
  file_t *demo_file;                                         ///< Open demo file for demo playback, or NULL during live gameplay.
} sv_server_t;

/**
 * @brief The server's client frame type. For each server frame, a unique client
 * frame is authored, containing only the relevant updates for that client. This
 * structure is sent as the header of `SV_CMD_FRAME`.
 */
typedef struct {
  player_state_t ps;     ///< Player state snapshot for this frame.
  int16_t num_entities;  ///< Number of delta-compressed entities in this frame.
  uint32_t entity_state; ///< Index into the entity state circular buffer.
  uint32_t sent_time;    ///< Server time when this frame was dispatched, used to calculate ping.
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
  SV_CLIENT_FREE,      ///< Client slot is available.
  SV_CLIENT_CONNECTED, ///< Client has connected but not yet spawned in-game.
  SV_CLIENT_ACTIVE     ///< Client is fully in-game and receiving frames.
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
  size_t offset; ///< Byte offset of this message in the datagram buffer.
  size_t len;    ///< Byte length of this message.
} sv_client_message_t;

/**
 * @brief A datagram structure that maintains individual message offsets so
 * that it may be safely fragmented for delivery.
 */
typedef struct {
  mem_buf_t buffer;             ///< Managed-size buffer wrapping data[].
  byte data[MAX_DATAGRAM_SIZE]; ///< Raw message storage for this frame's datagram.
  GList *messages;              ///< List of sv_client_message_t bounds for safe fragmentation.
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
  sv_client_state_t state;                         ///< Connection state of this client slot.
  char user_info[MAX_INFO_STRING_STRING];          ///< Raw user-info key-value string.
  char name[32];                                   ///< Player name extracted from user_info, stripped of color codes.
  int32_t message_level;                           ///< Minimum print level for chat messages delivered to this client.
  int32_t last_frame;                              ///< Last acknowledged frame number for delta compression; -1 sends baselines.
  uint32_t cmd_msec;                               ///< Accumulated movement command duration; exceeding server elapsed time indicates cheating.
  uint16_t cmd_msec_errors;                        ///< Consecutive anti-cheat violation count for cmd_msec drift.
  uint32_t frame_latency[SV_CLIENT_LATENCY_COUNT]; ///< Ring buffer of recent per-frame delivery timestamps for ping estimation.
  int32_t ping;                                    ///< Estimated round-trip latency in milliseconds.
  sv_client_datagram_t datagram;                   ///< Per-frame datagram; accumulated, packetized and delivered each server frame.
  sv_client_frame_t frames[PACKET_BACKUP];         ///< Circular buffer of sent frames; referenced by client for delta compression.
  sv_http_client_t http;                           ///< HTTP file download connection for this client.
  net_chan_t net_chan;                             ///< UDP network channel to this client.
  uint32_t last_message;                           ///< Server time of last received packet, used to detect timeouts.
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
  SV_UNINITIALIZED, ///< Server has not been initialized.
  SV_INITIALIZED,   ///< Server is initialized but no map is loaded.
  SV_LOADING,       ///< Map is currently loading.
  SV_ACTIVE_GAME,   ///< Server is running a live game.
  SV_ACTIVE_DEMO    ///< Server is playing back a demo.
} sv_state_t;

/**
 * @brief The `sv_static_t` structure is persistent for the execution of the
 * game. It is only cleared when `Sv_Init` is called. It is not exposed to the
 * game module.
 */
typedef struct {
  sv_state_t state;                          ///< Current server lifecycle state.
  sv_client_t *clients;                      ///< Dynamically allocated array of connected client slots.
  entity_state_t *entity_states;             ///< Circular buffer of entity states for delta compression across all clients.
  uint32_t num_entity_states;                ///< Length of entity_states; always PACKET_BACKUP * MAX_ENTITIES.
  uint32_t next_entity_state;                ///< Next free index in entity_states for newly spawned entities.
  net_addr_t masters[MAX_MASTERS];           ///< Configured master server addresses for heartbeat broadcasts.
  uint32_t next_heartbeat;                   ///< Server time after which the next heartbeat is sent to master servers.
  sv_challenge_t challenges[MAX_CHALLENGES]; ///< Pending connection challenges for DoS mitigation.
  uint32_t spawn_count;                      ///< Incremented at each map load to validate late-arriving connection handshakes.
  g_export_t *game;                          ///< Exported API from the loaded game module.
} sv_static_t;

#endif /* __SV_LOCAL_H__ */
