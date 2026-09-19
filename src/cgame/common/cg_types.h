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

#include "cgame/cgame.h"
#include "bg_intermission.h"
#include "g_types.h"

#if defined(__CG_LOCAL_H__)

#define CG_CENTER_PRINT_LINES 8

/**
 * @brief The client game reprensetation of teams.
 */
typedef struct {

  /**
   * @brief Team ID.
   */
  GameTeamId id;

  /**
   * @brief Team name.
   */
  char name[MAX_INFO_STRING_KEY];

  /**
   * @brief Shirt color.
   */
  Color color;

  /**
   * @brief Effects color, transmitted as a hue for efficiency.
   */
  float hue;

} ClientGameTeamInfo;

/**
 * @brief The vote in progress, as `CS_VOTE` describes it.
 */
typedef struct {

  /**
   * @brief Whether a vote is under way.
   */
  bool active;

  /**
   * @brief What is being voted on, and its argument, if any.
   */
  char type[MAX_QPATH];
  char arg[MAX_QPATH];

  /**
   * @brief Who called it.
   */
  char initiator[MAX_QPATH];

  /**
   * @brief The tally so far, and how many may cast one.
   */
  int32_t yes, no, eligible;

  /**
   * @brief When it closes, in client time.
   */
  uint32_t deadline;
} ClientGameVoteState;

/**
 * @brief The intermission's map candidates, as `CS_NEXT_MAP` describes them.
 */
typedef struct {

  /**
   * @brief Whether an intermission is under way. The maps are published only for one,
   * so their absence is what says the level is still being played.
   */
  bool active;

  /**
   * @brief Whether ballots are being taken, or the next map is simply being announced.
   */
  bool voting;

  /**
   * @brief The candidates, and the tally each has drawn.
   */
  char maps[MAX_NEXT_MAPS][MAX_QPATH];
  int32_t votes[MAX_NEXT_MAPS];
  int32_t numMaps;

  /**
   * @brief Bumped whenever the candidates change, so that a view redraws the thumbnails
   * only when it must; resolving one enumerates the filesystem.
   */
  uint32_t generation;
} ClientGameNextMapState;

/**
 * @brief The client game representation of clients (players).
 */
typedef struct {

  /**
   * @brief The client info string, e.g. "newbie\enforcer/default."
   */
  char info[MAX_STRING_CHARS];

  /**
   * @brief The player name, e.g. "newbie."
   */
  char name[MAX_INFO_STRING_VALUE];

  /**
   * @brief The model name, e.g. "enforcer."
   */
  char model[MAX_INFO_STRING_VALUE];

  /**
   * @brief The skin name, e.g. "default."
   */
  char skin[MAX_INFO_STRING_VALUE];

  /**
   * @brief Shirt, pants and helmet color.
   */
  Color shirt, pants, helmet;

  /**
   * @brief Effects color, transmitted as a hue for efficiency.
   */
  float hue;

  /**
   * @brief The floor and ceiling of the client's standing box, which their model
   * is scaled and seated to.
   */
  float standingFloor, standingCeiling;

  /**
   * @brief The head model and materials.
   */
  RenderModel *head;
  RenderMaterial *headSkins[MAX_MESH_FACES];

  /**
   * @brief The torso model and materials.
   */
  RenderModel *torso;
  RenderMaterial *torsoSkins[MAX_MESH_FACES];

  /**
   * @brief The legs model and materials.
   */
  RenderModel *legs;
  RenderMaterial *legsSkins[MAX_MESH_FACES];

  /**
   * @brief The skin icon for the scoreboard.
   */
  RenderImage *icon;

  /**
   * @brief The team identifier.
   */
  ClientGameTeamInfo *team;

  /**
   * @brief The cached weapon muzzle position in world space, transformed from
   * the model-space muzzle defined in `link.cfg` / `view.cfg`.
   */
  Vec3 weaponMuzzle;
} ClientGameClientInfo;

#define WEATHER_NONE 0x0
#define WEATHER_RAIN 0x1
#define WEATHER_SNOW 0x2
#define WEATHER_ASH  0x4

/**
 * @brief How the camera frames whatever it is watching, in demo playback and while spectating a
 * live game alike. Whether it is watching anything at all is a separate question - free flight
 * is the absence of a subject, not a way of framing one - which the server answers live, and
 * `ClientGameSpectateState::detached` answers during playback.
 */
typedef enum {
  /**
   * @brief Through the subject's own eyes.
   */
  CAMERA_FIRST_PERSON,

  /**
   * @brief Behind the subject, at the `cg_third_person_*` offset, riding their facing.
   */
  CAMERA_THIRD_PERSON,

  /**
   * @brief Anchored on the subject, but aimed by the viewer: the mouse swings the camera around
   * them and `+forward`/`+back` changes its distance.
   */
  CAMERA_FOLLOW,

  CAMERA_MODE_TOTAL
} ClientGameCameraMode;

/**
 * @brief Follow camera state: mouse-driven yaw/pitch and `+forward`/`+back`-driven distance,
 * held in world space so the camera keeps its place while the subject turns.
 */
typedef struct {
  float yaw, pitch, distance;

  /**
   * @brief Whether the camera was following last frame, so that entering the mode seeds the
   * accumulator. This lives here rather than in a static so that it is cleared with the rest of
   * the follow state, which a reconnect would otherwise leave disagreeing.
   */
  bool following;
} ClientGameFollowState;

/**
 * @brief Free-flight camera state for demo playback: a locally-owned `PM_SPECTATOR` movement
 * state driven directly by `Pm_Move`, independent of the recorded `PlayerState`.
 */
typedef struct {
  PlayerMoveState state;
  bool initialized;

  /**
   * @brief Whether the demo camera has left the recorded player behind. Live, the equivalent
   * question is whether the server has given us a chase target, which `STAT_CHASE` answers.
   */
  bool detached;
} ClientGameSpectateState;

/**
 * @brief Client game state. Most of this is parsed from ConfigStrings when they change.
 */
typedef struct {

  /**
   * @brief The clients (players).
   */
  ClientGameClientInfo clients[MAX_CLIENTS];

  /**
   * @brief The client info each standing corpse died wearing, by CS_CORPSES slot.
   */
  ClientGameClientInfo corpses[MAX_CORPSES];

  /**
   * @brief The forced skin (foreskin?) client info.
   */
  ClientGameClientInfo forceSkin;

  /**
   * @brief The teams.
   */
  ClientGameTeamInfo teams[MAX_TEAMS];

  /**
   * @brief The gameplay mode.
   */
  GameplayId gameplay;

  /**
   * @brief Active item set.
   */
  GameItems items;

  /**
   * @brief Non-zero if teams play is enabled.
   */
  int32_t numTeams;

  #if defined(G_HOOK)
/**
   * @brief Grapple hook speed, for client side prediction.
   */
  float hookPullSpeed;
#endif

  
  /**
   * @brief The current number of clients connected to the server.
   */
  int32_t numClients;

  /**
   * @brief Bot navitation node editor.
   */
  int32_t navEdit;

  /**
   * @brief Center print message state from `SV_CMD_CENTER_PRINT`.
   */
  struct {
    char lines[CG_CENTER_PRINT_LINES][MAX_STRING_CHARS];
    int32_t numLines;
    uint32_t time;
  } centerPrint;

  /**
   * @brief Pending view angle snap from a reliable `SV_CMD_SNAP_ANGLES` message.
   */
  bool snapAngles;

  /**
   * @brief The view angles to snap to.
   */
  Vec3 snapViewAngles;

  /**
   * @brief The vote in progress, from `CS_VOTE`.
   */
  ClientGameVoteState vote;

  /**
   * @brief The intermission's map candidates, from `CS_NEXT_MAP`.
   */
  ClientGameNextMapState nextMap;

  /**
   * @brief The camera mode, cycled by `camera`.
   */
  ClientGameCameraMode cameraMode;

  /**
   * @brief Whether the transport and camera controls have been printed for this connection.
   */
  bool printedControls;

  /**
   * @brief Follow camera state, shared by live spectating and demo playback.
   */
  ClientGameFollowState follow;

  /**
   * @brief Free-flight camera state, used during demo playback only.
   */
  ClientGameSpectateState spectate;
} ClientGameState;

extern ClientGameState cg_state;

#endif
