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

#include "cg_local.h"
#include "bg_intermission.h"

static struct {
  ParseConfigString ParseConfigString;
  StateDidClear StateDidClear;
} previous;

/**
 * @brief Bumped whenever the candidates change, never reset, so that a view comparing
 * against it sees a change even across a level.
 */
static uint32_t cg_next_map_generation;

/**
 * @brief Reads `CS_NEXT_MAP` into `cg_state.next_map`.
 */
static bool Cg_ParseConfigString_Intermission(int32_t index) {

  if (index != CS_NEXT_MAP) {
    return previous.ParseConfigString(index);
  }

  cg_next_map_state_t *next_map = &cg_state.next_map;

  char was[MAX_NEXT_MAPS][MAX_QPATH];
  memcpy(was, next_map->maps, sizeof(was));
  const int32_t num_was = next_map->num_maps;

  const char *s = cgi.ConfigString(index);

  memset(next_map, 0, sizeof(*next_map));

  if (!*s) {
    next_map->generation = num_was ? ++cg_next_map_generation : cg_next_map_generation;
    return true;
  }

  char buf[MAX_STRING_CHARS];
  q_strlcpy(buf, s, sizeof(buf));

  // split positionally, since the count of candidates is what the string carries
  char *fields[NEXT_MAP_CS_FIELDS_FOR(MAX_NEXT_MAPS)] = { NULL };
  size_t count = 0;

  for (char *c = buf; count < lengthof(fields); ) {
    fields[count++] = c;

    char *sep = strchr(c, '\\');
    if (!sep) {
      break;
    }
    *sep = '\0';
    c = sep + 1;
  }

  if (count < NEXT_MAP_CS_FIELDS_FOR(1) || (count - NEXT_MAP_CS_MAPS) % 2) {
    Cg_Warn("Invalid next map: %s\n", s);
    return true;
  }

  next_map->active = true;
  next_map->voting = *fields[NEXT_MAP_CS_VOTING] == '1';
  next_map->num_maps = (int32_t) (count - NEXT_MAP_CS_MAPS) / 2;

  for (int32_t i = 0; i < next_map->num_maps; i++) {
    q_strlcpy(next_map->maps[i], fields[NEXT_MAP_CS_MAPS + i * 2], MAX_QPATH);
    next_map->votes[i] = (int32_t) strtol(fields[NEXT_MAP_CS_MAPS + i * 2 + 1], NULL, 10);
  }

  // only the names cost anything to show, so the tally moving is not a redraw
  if (next_map->num_maps != num_was || memcmp(was, next_map->maps, sizeof(was))) {
    cg_next_map_generation++;
  }

  next_map->generation = cg_next_map_generation;

  return true;
}

/**
 * @see cg_intermission.h
 */
void Cg_Intermission_Vote(int32_t map) {
  cgi.Cbuf(va("vote_map %d\n", map + 1));
}

/**
 * @see cg_intermission.h
 */
bool Cg_Intermission_HandleEvent(const SDL_Event *event) {

  if (event->type != SDL_EVENT_KEY_DOWN) {
    return false;
  }

  if (!cg_state.next_map.active || !cg_state.next_map.voting) {
    return false;
  }

  // the client hands us every event, so the console and the chat would otherwise
  // cast a ballot for anyone typing a digit
  if (cgi.GetKeyDest() != KEY_GAME) {
    return false;
  }

  const int32_t map = (int32_t) (event->key.key - SDLK_1);

  if (map < 0 || map >= cg_state.next_map.num_maps) {
    return false;
  }

  Cg_Intermission_Vote(map);

  return true;
}

/**
 * @brief An intermission does not outlive its level.
 */
static void Cg_StateDidClear_Intermission(void) {

  memset(&cg_state.next_map, 0, sizeof(cg_state.next_map));

  previous.StateDidClear();
}

/**
 * @brief Installs the intermission over the hooks it needs, once per module image.
 */
void Cg_Intermission_Init(void) {
  static bool installed;

  cgi.AddCmd("vote_map", NULL, CMD_CGAME, "Vote for a map during the intermission: vote_map <number>");

  if (installed) {
    return;
  }

  previous.ParseConfigString = Cg_ParseConfigString;
  Cg_ParseConfigString = Cg_ParseConfigString_Intermission;

  previous.StateDidClear = Cg_StateDidClear;
  Cg_StateDidClear = Cg_StateDidClear_Intermission;

  installed = true;
}
