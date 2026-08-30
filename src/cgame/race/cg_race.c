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

#include "cg_race.h"

/**
 * @file
 * @brief The hooks, the ghost, and the barriers. See cg_race.h.
 *
 * The ghost the server sends wears the viewer's client slot, because a player
 * model needs one; here it is dressed instead as the course record's holder,
 * whose client info arrives on `CS_RACE_GHOST`, and drawn translucent. Another
 * player's ghost is theirs to see and not ours.
 *
 * The barriers are the `func_race_*` brushes, whose conditions arrive on
 * `CS_RACE_BARRIERS` so that prediction clips them as the server will, from
 * the run the stats describe. The client predicts only itself, and with no
 * mover, so `G_Race_ClipEntity`'s "no mover, so solid" rule has no mirror;
 * and it has no clip to one entity, so the one-way wall's "already inside"
 * probe is a trace during which only that wall clips.
 */

// how much of the ghost is there
#define RACE_GHOST_ALPHA .5f

static struct {
  ParseConfigString ParseConfigString;
  MediaDidLoad MediaDidLoad;
  FilterEntity FilterEntity;
  ClientInfo ClientInfo;
  EntityEffects EntityEffects;
  ClipEntity ClipEntity;
} previous;

static cg_client_info_t cg_race_ghost;

typedef struct {
  int32_t entity; // the barrier's entity number
  g_race_barrier_t barrier; // RACE_BARRIER_NONE for an empty slot
  g_race_gate_t gate;
  vec3_t move_dir;
} cg_race_barrier_t;

static cg_race_barrier_t cg_race_barriers[RACE_MAX_BARRIERS];

// the one barrier a probe for "already inside" may clip, while it runs
static const cl_entity_t *cg_race_probing;

uint32_t Cg_Race_Time(const player_state_t *ps) {
  return (uint16_t) ps->stats[STAT_RACE_TIME_LOW] | ((uint32_t) (uint16_t) ps->stats[STAT_RACE_TIME_HIGH] << 16);
}

/**
 * @brief Dresses the ghost as the record holder, or as nobody when there is no
 * record, which `Cg_LoadClient` reads as the default.
 */
static void Cg_Race_LoadGhost(void) {
  Cg_LoadClient(&cg_race_ghost, cgi.ConfigString(CS_RACE_GHOST));
}

static bool Cg_Race_IsGhost(const cl_entity_t *ent) {
  return ent->current.effects & EF_RACE_GHOST;
}

/**
 * @brief Reads one barrier's slot, as `G_func_race_Init` wrote it; an empty
 * or malformed slot is no barrier, and the brush is then plainly solid.
 */
static void Cg_Race_ParseBarrier(int32_t slot) {
  cg_race_barrier_t *b = &cg_race_barriers[slot];
  const char *s = cgi.ConfigString(CS_RACE_BARRIERS + slot);

  *b = (cg_race_barrier_t) { .barrier = RACE_BARRIER_NONE };

  if (!*s) {
    return;
  }

  int32_t entity, barrier, mode, invert, n = 0;

  if (sscanf(s, "%d\\%d\\%n", &entity, &barrier, &n) != 2 || !n || entity < 0 || entity >= MAX_ENTITIES) {
    Cg_Warn("Invalid barrier \"%s\"\n", s);
    return;
  }

  switch (barrier) {
    case RACE_BARRIER_GATE:
      if (sscanf(s + n, "%hu\\%d\\%d", &b->gate.checkpoint, &mode, &invert) != 3 ||
          b->gate.checkpoint < 1 || b->gate.checkpoint > RACE_MAX_CHECKPOINTS ||
          (mode != RACE_GATE_AT_LEAST && mode != RACE_GATE_EXACT)) {
        Cg_Warn("Invalid gate \"%s\"\n", s);
        return;
      }
      b->gate.mode = mode;
      b->gate.invert = invert != 0;
      break;
    case RACE_BARRIER_WALL:
      if (sscanf(s + n, "%f\\%f", &b->move_dir.x, &b->move_dir.y) != 2) {
        Cg_Warn("Invalid one-way wall \"%s\"\n", s);
        return;
      }
      break;
    default:
      Cg_Warn("Invalid barrier \"%s\"\n", s);
      return;
  }

  b->entity = entity;
  b->barrier = barrier;
}

static void Cg_Race_ParseBarriers(void) {

  for (int32_t i = 0; i < RACE_MAX_BARRIERS; i++) {
    Cg_Race_ParseBarrier(i);
  }
}

static const cg_race_barrier_t *Cg_Race_BarrierFor(const cl_entity_t *ent) {

  for (int32_t i = 0; i < RACE_MAX_BARRIERS; i++) {
    if (cg_race_barriers[i].barrier != RACE_BARRIER_NONE && cg_race_barriers[i].entity == ent->current.number) {
      return &cg_race_barriers[i];
    }
  }

  return NULL;
}

static bool Cg_ParseConfigString_Race(int32_t index) {

  if (index == CS_RACE_GHOST) {
    Cg_Race_LoadGhost();
    return true;
  }

  if (index >= CS_RACE_BARRIERS && index < CS_RACE_BARRIERS + RACE_MAX_BARRIERS) {
    Cg_Race_ParseBarrier(index - CS_RACE_BARRIERS);
    return true;
  }

  return previous.ParseConfigString(index);
}

/**
 * @brief The client infos are media, and are reloaded with it; the barriers
 * are re-read so that a new level's slots replace the last one's.
 */
static void Cg_MediaDidLoad_Race(void) {

  previous.MediaDidLoad();

  Cg_Race_LoadGhost();
  Cg_Race_ParseBarriers();
}

static bool Cg_Race_ClipPrevious(const cl_entity_t *mover, const cl_entity_t *ent, const vec3_t start, const vec3_t end, const box3_t bounds) {
  return previous.ClipEntity ? previous.ClipEntity(mover, ent, start, end, bounds) : true;
}

/**
 * @brief `G_Race_ClipEntity`, as the client can apply it to itself.
 */
static bool Cg_ClipEntity_Race(const cl_entity_t *mover, const cl_entity_t *ent, const vec3_t start, const vec3_t end, const box3_t bounds) {

  if (cg_race_probing) {
    return ent == cg_race_probing;
  }

  const cg_race_barrier_t *b = Cg_Race_BarrierFor(ent);

  if (!b) {
    return previous.ClipEntity(mover, ent, start, end, bounds);
  }

  const player_state_t *ps = &cgi.client->frame.ps;

  if (b->barrier == RACE_BARRIER_GATE) {

    if (ps->stats[STAT_RACE_RUN] != RACE_RUN_ACTIVE) {
      return false;
    }

    const bool open = b->gate.mode == RACE_GATE_EXACT
                      ? ps->stats[STAT_RACE_CHECKPOINTS] == b->gate.checkpoint
                      : ps->stats[STAT_RACE_CHECKPOINTS] >= b->gate.checkpoint;

    if (open != b->gate.invert) {
      return false;
    }

    return Cg_Race_ClipPrevious(mover, ent, start, end, bounds);
  }

  cg_race_probing = ent;
  const cm_trace_t tr = cgi.Trace(start, start, bounds, NULL, CONTENTS_MASK_CLIP_PLAYER);
  cg_race_probing = NULL;

  if (tr.start_solid && tr.ent == ent) {
    return false;
  }

  const vec3_t travel = Vec3_Subtract(end, start);

  if (travel.x * b->move_dir.x + travel.y * b->move_dir.y > 0.f) {
    return false;
  }

  return Cg_Race_ClipPrevious(mover, ent, start, end, bounds);
}

static bool Cg_FilterEntity_Race(const cl_entity_t *ent) {

  if (Cg_Race_IsGhost(ent) && ent->current.client != cgi.client->frame.ps.client) {
    return false;
  }

  return previous.FilterEntity(ent);
}

static cg_client_info_t *Cg_ClientInfo_Race(const cl_entity_t *ent) {

  if (Cg_Race_IsGhost(ent)) {
    return &cg_race_ghost;
  }

  return previous.ClientInfo(ent);
}

static void Cg_EntityEffects_Race(cl_entity_t *ent, r_entity_t *e) {

  previous.EntityEffects(ent, e);

  if (Cg_Race_IsGhost(ent)) {
    e->effects |= EF_BLEND | EF_NO_SHADOW;
    e->color = Vec4_Scale(e->color, RACE_GHOST_ALPHA);
  }
}

void Cg_Race_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  installed = true;

  Cg_DrawHudElements = Cg_Race_DrawHud;
  Cg_DrawScores = Cg_Race_DrawScores;

  previous.ParseConfigString = Cg_ParseConfigString;
  Cg_ParseConfigString = Cg_ParseConfigString_Race;

  previous.MediaDidLoad = Cg_MediaDidLoad;
  Cg_MediaDidLoad = Cg_MediaDidLoad_Race;

  previous.FilterEntity = Cg_FilterEntity;
  Cg_FilterEntity = Cg_FilterEntity_Race;

  previous.ClientInfo = Cg_ClientInfo;
  Cg_ClientInfo = Cg_ClientInfo_Race;

  previous.EntityEffects = Cg_EntityEffects;
  Cg_EntityEffects = Cg_EntityEffects_Race;

  previous.ClipEntity = Cg_ClipEntity;
  Cg_ClipEntity = Cg_ClipEntity_Race;
}
