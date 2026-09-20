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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "common/common.h"

#include "game/game.h"

/**
 * @brief Server states.
 */
typedef enum {
  SV_UNINITIALIZED,
  SV_INITIALIZED,
  SV_LOADING,
  SV_ACTIVE_GAME,
  SV_ACTIVE_DEMO
} ServerState;

#if defined(__SV_LOCAL_H__)

/**
 * @brief The server entity type.
 */
typedef struct {

  /**
   * @brief The corresponding game entity; set once per map at spawn time.
   */
  GameEntity *gent;

  /**
   * @brief Baseline entity state for delta compression.
   */
  EntityState baseline;

  /**
   * @brief World sector for entity list management.
   */
  struct ServerSector *sector;

  /**
   * @brief World-space transform for collision tests.
   */
  Mat4 matrix;

  /**
   * @brief Inverse of matrix, used to bring traces into model space.
   */
  Mat4 inverseMatrix;
} ServerEntity;

/**
 * @brief The `Server` struct is wiped at each level load.
 */
typedef struct {

  /**
   * @brief Simulation time in ms; always `frameNum` * 1000 / `QUETOO_TICK_RATE`.
   */
  uint32_t time;

  /**
   * @brief Current simulation frame number.
   */
  uint32_t frameNum;

  /**
   * @brief Map name, e.g. "maps/edge".
   */
  char name[MAX_QPATH];

  /**
   * @brief Collision models; [0] is worldspawn, rest are inline models.
   */
  CmBspModel *cmModels[MAX_MODELS];

  /**
   * @brief Config strings enumerating all loaded assets (models, sounds, skins, etc.).
   */
  char configStrings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

  /**
   * @brief Server-side entity array.
   */
  ServerEntity entities[MAX_ENTITIES];

  /**
   * @brief Multicast buffer, accumulated and delivered each server frame.
   */
  MemBuf multicast;

  /**
   * @brief Backing storage for the multicast buffer.
   */
  byte multicastBuffer[MAX_MSG_SIZE];

  /**
   * @brief Open demo file for demo playback, or `NULL` during live gameplay.
   */
  File *demoFile;

  /**
   * @brief The fixed-size header read from `demoFile`, for demo playback.
   */
  DemoHeader demoHeader;

  /**
   * @brief The keyframe table read from `demoFile`, for demo playback seeking.
   */
  DemoKeyframe *demoKeyframes;

  /**
   * @brief The number of entries in `demoKeyframes`.
   */
  int32_t numDemoKeyframes;

  /**
   * @brief The frame number of the most recently read demo message, for demo playback.
   */
  int32_t demoFrameNum;

  /**
   * @brief True if demo playback is currently paused.
   */
  bool demoPaused;

  /**
   * @brief Set by `Sv_SeekDemo` to release exactly one frame even while paused. Without it a
   * seek issued from the paused transport controls would move the file position but transmit
   * nothing, leaving the viewer on the old frame until playback resumed somewhere unexpected.
   */
  bool demoStep;

  /**
   * @brief Set once playback has read the recording's terminator, and cleared by a seek. The
   * keyframe index is appended after the stream, so reading on past the end walks into it and
   * reads an index entry as a chunk size.
   */
  bool demoEnded;

  /**
   * @brief Where the recorded stream begins, which is the size of the header the demo was
   * written with. Held rather than recomputed, because a version 2 header is shorter.
   */
  int64_t demoStreamOffset;
} Server;

/**
 * @brief The server's client frame type. For each server frame, a unique client
 * frame is authored, containing only the relevant updates for that client. This
 * structure is sent as the header of `SV_CMD_FRAME`.
 */
typedef struct {

  /**
   * @brief Player state snapshot for this frame.
   */
  PlayerState ps;

  /**
   * @brief Number of delta-compressed entities in this frame.
   */
  int16_t numEntities;

  /**
   * @brief Index into the entity state circular buffer.
   */
  uint32_t entityState;

  /**
   * @brief Server time when this frame was dispatched, used to calculate ping.
   */
  uint32_t sentTime;
} ServerClientFrame;

/**
 * @brief Clients are dropped after 20 seconds without receiving a packet.
 */
#define SV_TIMEOUT 20

/**
 * @brief Frame latency accounting, used to estimate ping.
 */
#define SV_CLIENT_LATENCY_COUNT 16

/**
 * @brief How often the reported ping is recalculated, in milliseconds.
 * @remarks This must track the scoreboard's own refresh in `G_ClientScores`, since both it and
 * the HUD read whatever this last settled on. Recalculating every frame only flickers: the
 * figure is a sixteen sample mean, so it is never that fresh to begin with.
 */
#define SV_CLIENT_PING_INTERVAL 500

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
} ServerClientState;

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
} ServerClientMessage;

/**
 * @brief A datagram structure that maintains individual message offsets so
 * that it may be safely fragmented for delivery.
 */
typedef struct {

  /**
   * @brief Managed-size buffer wrapping data[].
   */
  MemBuf buffer;

  /**
   * @brief Raw message storage for this frame's datagram.
   */
  byte data[MAX_DATAGRAM_SIZE];

  /**
   * @brief List of `ServerClientMessage` bounds for safe fragmentation.
   */
  List *messages;
} ServerClientDatagram;

/**
 * @brief Tracks a client's HTTP file download connection.
 */
typedef struct {
  int32_t socket;
  char request[1024];
  int32_t requestLen;
  byte *data;
  int32_t size;
  int32_t count;
} ServerHttpClient;

/**
 * @brief The server client type.
 */
typedef struct {

  /**
   * @brief The corresponding game client; set once at game initialization.
   */
  GameClient *gclient;

  /**
   * @brief Voice chat budget, in bytes, refilled over time and spent on transmission.
   */
  int32_t voiceBytes;
  uint32_t voiceTime;

  /**
   * @brief Mask of clients this one has muted; their voice is never relayed here.
   */
  uint64_t voiceMutes;

  /**
   * @brief Connection state of this client slot.
   */
  ServerClientState state;

  /**
   * @brief Raw user-info key-value string.
   */
  char userInfo[MAX_INFO_STRING_STRING];

  /**
   * @brief Player name extracted from `userInfo`, stripped of color codes.
   */
  char name[32];

  /**
   * @brief Minimum print level for chat messages delivered to this client.
   */
  int32_t messageLevel;

  /**
   * @brief Last acknowledged frame number for delta compression; -1 sends baselines.
   */
  int32_t lastFrame;

  /**
   * @brief Accumulated movement command duration; exceeding server elapsed time indicates cheating.
   */
  uint32_t cmdMsec;

  /**
   * @brief Consecutive anti-cheat violation count for `cmdMsec` drift.
   */
  uint16_t cmdMsecErrors;

  /**
   * @brief Ring buffer of recent per-frame delivery timestamps for ping estimation.
   * @remarks Written in sequence rather than indexed by frame number. Indexing by frame let a
   * client that acknowledged on a fixed stride hold a subset of the slots indefinitely, so
   * samples of any age were averaged in forever.
   */
  uint32_t frameLatency[SV_CLIENT_LATENCY_COUNT];

  /**
   * @brief The next slot of `frameLatency` to write.
   */
  uint32_t frameLatencyIndex;

  /**
   * @brief How many slots of `frameLatency` have been written, saturating at the ring size.
   * @remarks A latency of zero is a legitimate sample on a loopback or local network, so the
   * count says which slots are populated rather than testing the samples themselves.
   */
  uint32_t frameLatencyCount;

  /**
   * @brief Estimated round-trip latency in milliseconds.
   */
  int32_t ping;

  /**
   * @brief Per-frame datagram; accumulated, packetized and delivered each server frame.
   */
  ServerClientDatagram datagram;

  /**
   * @brief Circular buffer of sent frames; referenced by client for delta compression.
   */
  ServerClientFrame frames[PACKET_BACKUP];

  /**
   * @brief HTTP file download connection for this client.
   */
  ServerHttpClient http;

  /**
   * @brief UDP network channel to this client.
   */
  NetChan netChan;

  /**
   * @brief Server time of last received packet, used to detect timeouts.
   */
  uint32_t lastMessage;
} ServerClient;

/**
 * @brief Challenges are a request for a connection. The client must receive
 * and then re-use a valid challenge in order to receive a client slot. This
 * provides basic protection against simple UDP DoS attacks.
 */
typedef struct {
  NetAddr addr;
  uint32_t challenge;
  uint32_t time;
} ServerChallenge;

/**
 * @brief The master server we advertise to, and the challenge it most recently
 * issued. The challenge must be echoed in our heartbeats before the master will
 * list us, which proves that we receive traffic at the address we send from.
 */
typedef struct {
  NetAddr addr;
  uint32_t challenge;
  uint32_t challengeTime;
} ServerMaster;

/**
 * @brief `MAX_CHALLENGES` is large to prevent a denial of service attack that
 * could cycle all of them out before legitimate users connected.
 */
#define MAX_CHALLENGES 1024

typedef struct {
  /**
   * @brief The filename the map list was parsed from.
   */
  char filename[MAX_QPATH];

  /**
   * @brief Cached map list entries (`CmEntity *`) parsed from `sv_mapList`.
   */
  List *list;

  /**
   * @brief The length of `map_list`.
   */
  int32_t length;

  /**
   * @brief The current map list index.
   */
  int32_t index;

  /**
   * @brief The index the running level was served from, or `-1` if it did not come
   * from the rotation. This is not `index`, which is only where the rotation has
   * reached: a list may name the same map more than once, and the two occurrences
   * are different positions to resume from.
   */
  int32_t current;

  /**
   * @brief The modification time of the file when it was last loaded.
   */
  int64_t modtime;

  /**
   * @brief The index `Sv_SetNextMap` chose, which the next `Sv_NextMap` returns in
   * place of the rotation's pick, or `-1`. Consumed as soon as it is read.
   */
  int32_t next;
} ServerMapList;

/**
 * @brief The `ServerStatic` structure is persistent for the execution of the
 * game. It is only cleared when `Sv_Init` is called. It is not exposed to the
 * game module.
 */
typedef struct {

  /**
   * @brief Current server lifecycle state.
   */
  ServerState state;

  /**
   * @brief Dynamically allocated array of connected client slots.
   */
  ServerClient *clients;

  /**
   * @brief Circular buffer of entity states for delta compression across all clients.
   */
  EntityState *entityStates;

  /**
   * @brief Length of `entityStates`; always `PACKET_BACKUP` * `MAX_ENTITIES`.
   */
  uint32_t numEntityStates;

  /**
   * @brief Next free index in `entityStates` for newly spawned entities.
   */
  uint32_t nextEntityState;

  /**
   * @brief The configured master server, and its outstanding challenge.
   */
  ServerMaster master;

  /**
   * @brief Server time after which the next heartbeat is sent to master servers.
   */
  uint32_t nextHeartbeat;

  /**
   * @brief Pending connection challenges for DoS mitigation.
   */
  ServerChallenge challenges[MAX_CHALLENGES];

  /**
   * @brief Incremented at each map load to validate late-arriving connection handshakes.
   */
  uint32_t spawnCount;

  /**
   * @brief The map list.
   */
  ServerMapList maps;

  /**
   * @brief Exported API from the loaded game module.
   */
  GameExport *game;
} ServerStatic;

#endif
