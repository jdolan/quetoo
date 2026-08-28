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
 * @brief The hooks, and the ghost. See cg_race.h.
 *
 * The ghost the server sends wears the viewer's client slot, because a player
 * model needs one; here it is dressed instead as the course record's holder,
 * whose client info arrives on `CS_RACE_GHOST`, and drawn translucent. Another
 * player's ghost is theirs to see and not ours.
 */

// how much of the ghost is there
#define RACE_GHOST_ALPHA .5f

static struct {
  DrawHudElements DrawHudElements;
  ParseConfigString ParseConfigString;
  MediaDidLoad MediaDidLoad;
  FilterEntity FilterEntity;
  ClientInfo ClientInfo;
  EntityEffects EntityEffects;
} previous;

static cg_client_info_t cg_race_ghost;

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

static void Cg_DrawHudElements_Race(const player_state_t *ps, cg_hud_layout_t *layout) {

  previous.DrawHudElements(ps, layout);

  Cg_Race_DrawHud(ps);
}

static bool Cg_ParseConfigString_Race(int32_t index) {

  if (index == CS_RACE_GHOST) {
    Cg_Race_LoadGhost();
    return true;
  }

  return previous.ParseConfigString(index);
}

/**
 * @brief The client infos are media, and are reloaded with it.
 */
static void Cg_MediaDidLoad_Race(void) {

  previous.MediaDidLoad();

  Cg_Race_LoadGhost();
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

  previous.DrawHudElements = Cg_DrawHudElements;
  Cg_DrawHudElements = Cg_DrawHudElements_Race;

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
}
