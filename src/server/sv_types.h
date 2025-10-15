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

#ifdef __SV_LOCAL_H__

/**
 * @brief The server entity type.
 */
typedef struct {
  /**
   * @brief The baseline entity state, used for delta compression.
   */
  entity_state_t baseline;

  /**
   * @brief The sector, for entity list management.
   */
  struct sv_sector_s *sector;

  /**
   * @brief The clipping matrix, used for collision tests.
   */
  mat4_t matrix;
  mat4_t inverse_matrix;
} sv_entity_t;

/**
 * @brief The `sv_server_t` struct is wiped at each level load.
 */
typedef struct {
  /**
   * @brief The simulation time.
   * @details This is always `sv.frame_num * 1000 / QUETOO_TICK_RATE`.
   */
  uint32_t time;

  /**
   * @brief The current frame number.
   */
  uint32_t frame_num;

  /**
   * @brief The map name.
   */
  char name[MAX_QPATH];

  /**
   * @brief The collision models (worldspawn, plus all inline models).
   */
  cm_bsp_model_t *cm_models[MAX_MODELS];

  /**
   * @brief The configuration strings. These enumerate all loaded assets, such as models,
   * sounds, client skins, etc.
   */
  char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

  /**
   * @brief The entities.
   */
  sv_entity_t entities[MAX_ENTITIES];

  /**
   * @brief The multicast buffer used to send a message to a set of clients.
   */
  mem_buf_t multicast;
  byte multicast_buffer[MAX_MSG_SIZE];

  /**
   * @brief The demo file for demo playback.
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
   * @brief The player state.
   */
  player_state_t ps;

  /**
   * @brief The number of delta-compressed entities in this frame.
   */
  uint16_t num_entities;

  /**
   * @brief The index into the entity state circular buffer.
   */
  uint32_t entity_state;

  /**
   * @brief The delivery timestamp, used to calculate ping.
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
  size_t offset;
  size_t len;
} sv_client_message_t;

/**
 * @brief A datagram structure that maintains individual message offsets so
 * that it may be safely fragmented for delivery.
 */
typedef struct {
  mem_buf_t buffer; // the managed size buffer
  byte data[MAX_DATAGRAM_SIZE]; // the raw message buffer
  GList *messages; // message segmentation
} sv_client_datagram_t;

/**
 * @brief Each client my download a single file at a time via the game's UDP
 * protocol. This only serves as a fallback for when HTTP downloading is not
 * configured or unavailable.
 */
typedef struct {
  byte *buffer;
  int32_t size;
  int32_t count;
} sv_client_download_t;

/**
 * @brief The server client type.
 */
typedef struct {
  /**
   * @brief The client state.
   */
  sv_client_state_t state;

  /**
   * @brief The user-info string.
   */
  char user_info[MAX_USER_INFO_STRING];

  /**
   * @brief The player name, extracted from user-info and stripped of colors.
   */
  char name[32];

  /**
   * @brief The message receipt level, used to filter chat messages.
   */
  int32_t message_level;

  /**
   * @brief The last frame the client has acknowledged.
   * @details This is used for delta compression. -1 will receive baselines.
   */
  int32_t last_frame;

  /**
   * @brief The sum of movement command durations for this client. If this
   * exceeds the server elapsed duration, the client is trying to cheat.
   */
  uint32_t cmd_msec;
  uint16_t cmd_msec_errors;

  /**
   * @brief Ping calculation.
   */
  uint32_t frame_latency[SV_CLIENT_LATENCY_COUNT];

  /**
   * @brief The entity bound to this client.
   */
  g_entity_t *entity;

  /**
   * @brief The client's datagram.
   * @details This is accumulated, packetized and delivered each server frame.
   */
  sv_client_datagram_t datagram;

  /**
   * @brief The circular buffer of recently sent client frames. The client
   * may reference any of these frames for valid delta compression. If the
   * client references a frame _not_ in this array, they'll receive baselines.
   */
  sv_client_frame_t frames[PACKET_BACKUP];

  /**
   * @brief UDP file downloads.
   */
  sv_client_download_t download;

  /**
   * @brief The network channel.
   */
  net_chan_t net_chan;

  /**
   * @brief Server time when the last message was received, used to detect timeouts.
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
   * @brief The server state.
   */
  sv_state_t state;

  /**
   * @brief The clients.
   */
  sv_client_t *clients;

  /**
   * @brief The entity state circular array.
   * @details The server maintains this to calculate delta compression between client frames.
   * The array is sized to support the maximum number of clients and entities.
   */
  entity_state_t *entity_states;

  /**
   * @brief The length of the entity states array.
   * @details Equal to `sv_max_clients->integer * UPDATE_BACKUP * MAX_PACKET_ENTITIES`.
   */
  uint32_t num_entity_states;

  /**
   * @brief The next free entity state for newly spawned entities.
   */
  uint32_t next_entity_state;

  /**
   * @brief The master servers.
   * @details Master servers maintain lists of running game servers.
   */
  net_addr_t masters[MAX_MASTERS];

  /**
   * @brief The time at which a heartbeat will be sent to all configured master servers.
   */
  uint32_t next_heartbeat;

  /**
   * @brief The challenges array.
   */
  sv_challenge_t challenges[MAX_CHALLENGES];

  /**
   * @brief The spawn count, incremented at each map start, is used to ensure that
   * long-running connection handshakes (for example, when a client needs to download
   * the map) are still current when the client attempts to spawn.
   */
  uint32_t spawn_count;

  /**
   * @brief The exported game module API.
   */
  g_export_t *game;
} sv_static_t;

/**
 * @brief Yields a pointer to the edict by the given number by negotiating the
 * edicts array based on the reported size of g_entity_t.
 */
#define ENTITY_FOR_NUM(n) \
  ( (g_entity_t *) ((byte *) svs.game->entities + svs.game->entity_size * (n)) )

/**
 * @brief Yields the entity number (index) for the specified g_entity_t * by
 * negotiating the edicts array based on the reported size of g_entity_t.
 */
#define NUM_FOR_ENTITY(e) \
  ( ((intptr_t) (e) - (intptr_t) svs.game->entities) / svs.game->entity_size )

#endif /* __SV_LOCAL_H__ */
