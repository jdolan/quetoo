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

#include "game/game.h"
#include "ai/ai.h"
#include "matrix.h"

#ifdef __SV_LOCAL_H__

/**
 * @brief The practical maximum number of leafs an entity may occupy. If an
 * entity exceeds this (by having a massive bounding box, for example), it may
 * fall out of PVS for some valid client positions.
 */
#define MAX_ENT_LEAFS 512

/**
 * @brief The maximum number of clusters an entity may occupy. If an entity
 * exceeds this (by having a massive bounding box, for example), a full BSP
 * recursion is necessary to determine its visibility (bad).
 */
#define MAX_ENT_CLUSTERS 64

/**
 * @brief The server-specific view of an entity. An sv_entity_t corresponds to
 * precisely one g_entity_t, where most general-purpose entity state resides.
 * This structure is primarily used for entity list management and clipping.
 */
typedef struct {
	int32_t top_node; // used if MAX_ENT_LEAFS or MAX_ENT_CLUSTERS is exceeded

	int32_t clusters[MAX_ENT_CLUSTERS];
	int32_t num_clusters; // if -1, use top_node

	int32_t areas[2];
	struct sv_sector_s *sector;

	matrix4x4_t matrix;
	matrix4x4_t inverse_matrix;
} sv_entity_t;

/**
 * @brief Server states.
 */
typedef enum {
	SV_UNINITIALIZED, // no level loaded
	SV_LOADING, // spawning level edicts
	SV_ACTIVE_GAME, // actively running
	SV_ACTIVE_DEMO
} sv_state_t;

/**
 * @brief The sv_server_t struct is wiped at each level load.
 */
typedef struct {
	sv_state_t state;

	uint32_t time; // always sv.frame_num * 1000 / sv_packetrate->value
	uint32_t frame_num;

	char name[MAX_QPATH]; // map name
	cm_bsp_model_t *cm_models[MAX_MODELS];

	// all known and indexed assets, string constants, etc..
	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

	sv_entity_t entities[MAX_ENTITIES]; // the server-local entity structures
	entity_state_t baselines[MAX_ENTITIES]; // g_entity_t baselines

	// the multicast buffer is used to send a message to a set of clients
	// it is flushed each time Sv_Multicast is called
	mem_buf_t multicast;
	byte multicast_buffer[MAX_MSG_SIZE];

	// demo server information
	file_t *demo_file;
} sv_server_t;

typedef struct {
	int32_t area_bytes;
	byte area_bits[MAX_BSP_AREAS >> 3]; // portal area visibility bits
	player_state_t ps;
	uint16_t num_entities;
	uint32_t entity_state; // index into svs.entity_states array
	uint32_t sent_time; // for ping calculations
} sv_frame_t;

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
	SV_CLIENT_FREE, // can be used for a new connection
	SV_CLIENT_CONNECTED, // client is connecting, but has not yet spawned
	SV_CLIENT_ACTIVE // client is spawned
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
 * @brief Per-client accounting for protocol flow control and low-level
 * connection state management.
 */
typedef struct {
	sv_client_state_t state;

	char user_info[MAX_USER_INFO_STRING]; // name, skin, etc

	int32_t last_frame; // for delta compression
	pm_cmd_t last_cmd; // for filling in big drops

	uint32_t cmd_msec; // for sv_enforce_time
	uint16_t cmd_msec_errors; // maintain how many problems we've seen

	uint32_t frame_latency[SV_CLIENT_LATENCY_COUNT]; // used to calculate ping

	size_t frame_size[QUETOO_TICK_RATE]; // used to rate drop packets
	uint32_t rate;
	uint32_t suppress_count; // number of messages rate suppressed

	g_entity_t *entity; // the g_entity_t for this client
	char name[32]; // extracted from user_info, high bits masked
	int32_t message_level; // for filtering printed messages

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be overflowed without consequence.
	// it is packetized and written to the client, then wiped, each frame.
	sv_client_datagram_t datagram;

	sv_frame_t frames[PACKET_BACKUP]; // updates can be delta'd from here

	sv_client_download_t download; // UDP file downloads

	uint32_t last_message; // quetoo.ticks when packet was last received
	net_chan_t net_chan;
} sv_client_t;

/**
 * @brief Public servers may broadcast their status to as many as 8 master
 * servers.
 */
#define MAX_MASTERS	8

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
 * @brief MAX_CHALLENGES is large to prevent a denial of service attack that
 * could cycle all of them out before legitimate users connected.
 */
#define MAX_CHALLENGES 1024

/**
 * @brief The sv_static_t structure is persistent for the execution of the
 * game. It is only cleared when Sv_Init is called. It is not exposed to the
 * game module.
 */
typedef struct {
	_Bool initialized; // sv_init has completed

	uint32_t spawn_count; // incremented each level start, used to check late spawns

	sv_client_t *clients; // server-side client structures

	// the server maintains an array of entity states it uses to calculate
	// delta compression from frame to frame

	// the size of this array is based on the number of clients we might be
	// asked to support at any point in time during the current game

	uint32_t num_entity_states; // sv_max_clients->integer * UPDATE_BACKUP * MAX_PACKET_ENTITIES
	uint32_t next_entity_state; // next entity_state to use for newly spawned entities
	entity_state_t *entity_states; // entity states array used for delta compression

	net_addr_t masters[MAX_MASTERS];
	uint32_t next_heartbeat;

	sv_challenge_t challenges[MAX_CHALLENGES]; // to prevent invalid IPs from connecting

	/**
	 * @brief The exported game module API.
	 */
	g_export_t *game;

	/**
	 * @brief The exported ai module API.
	 */
	ai_export_t *ai;
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
	( ((byte *)(e) - (byte *) svs.game->entities) / svs.game->entity_size )

#endif /* __SV_LOCAL_H__ */
