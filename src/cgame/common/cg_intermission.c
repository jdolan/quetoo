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

#include "cg_local.h"
#include "bg_intermission.h"

static struct {
  CGameParseConfigStringHook ParseConfigString;
  CGameStateDidClearHook StateDidClear;
} previous;

/**
 * @brief Bumped whenever the candidates change, never reset, so that a view comparing
 * against it sees a change even across a level.
 */
static uint32_t nextMapGeneration;

/**
 * @brief Bumped whenever the match stats arrive or clear, never reset, for the same reason.
 */
static uint32_t matchStatsGeneration;

/**
 * @brief Reads `CS_NEXT_MAP` into `cgameState.nextMap`.
 */
static bool Cg_ParseConfigString_Intermission(int32_t index) {

  if (index != CS_NEXT_MAP) {
    return previous.ParseConfigString(index);
  }

  CGameNextMapState *nextMap = &cgameState.nextMap;

  char was[MAX_NEXT_MAPS][MAX_QPATH];
  memcpy(was, nextMap->maps, sizeof(was));
  const int32_t numWas = nextMap->numMaps;
  const bool votingWas = nextMap->voting, practiceWas = nextMap->practice;

  const char *s = cgi.ConfigString(index);

  memset(nextMap, 0, sizeof(*nextMap));

  if (!*s) {
    nextMap->generation = numWas || practiceWas ? ++nextMapGeneration : nextMapGeneration;
    return true;
  }

  char buf[MAX_STRING_CHARS];
  Str_Copy(buf, s, sizeof(buf));

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

  const GameNextMapMode mode = (GameNextMapMode) strtol(fields[NEXT_MAP_CS_MODE], NULL, 10);
  const size_t minimum = NEXT_MAP_CS_FIELDS_FOR(mode == NEXT_MAP_PRACTICE ? 0 : 1);

  if (count < minimum || (count - NEXT_MAP_CS_MAPS) % 2) {
    Cg_Warn("Invalid next map: %s\n", s);
    return true;
  }

  nextMap->active = true;
  nextMap->voting = mode == NEXT_MAP_VOTE;
  nextMap->practice = mode == NEXT_MAP_PRACTICE;
  nextMap->numMaps = (int32_t) (count - NEXT_MAP_CS_MAPS) / 2;

  for (int32_t i = 0; i < nextMap->numMaps; i++) {
    Str_Copy(nextMap->maps[i], fields[NEXT_MAP_CS_MAPS + i * 2], MAX_QPATH);
    nextMap->votes[i] = (int32_t) strtol(fields[NEXT_MAP_CS_MAPS + i * 2 + 1], NULL, 10);
  }

  // only the names and the mode cost anything to show, so the tally moving is not a redraw
  if (nextMap->numMaps != numWas || memcmp(was, nextMap->maps, sizeof(was)) ||
      nextMap->voting != votingWas || nextMap->practice != practiceWas) {
    nextMapGeneration++;
  }

  if (nextMap->practice && !practiceWas) {
    Cg_Practice_ResetResult();
  }

  nextMap->generation = nextMapGeneration;

  return true;
}

/**
 * @see cg_intermission.h
 */
void Cg_Intermission_ParseMatchStats(void) {

  CGameMatchStats stats;
  memset(&stats, 0, sizeof(stats));

  stats.damageDealt = (uint32_t) cgi.ReadLong();
  stats.damageTaken = (uint32_t) cgi.ReadLong();

  const int32_t count = cgi.ReadByte();

  for (int32_t i = 0; i < count; i++) {
    const int32_t tag = cgi.ReadByte();
    const uint32_t shots = (uint32_t) cgi.ReadLong();
    const uint32_t hits = (uint32_t) cgi.ReadLong();
    const uint32_t damage = (uint32_t) cgi.ReadLong();
    const uint16_t kills = (uint16_t) cgi.ReadShort();

    if (tag < WEAPON_FIRST || tag >= WEAPON_LAST || stats.numWeapons == WEAPON_TOTAL) {
      Cg_Warn("Invalid weapon stats for item %d\n", tag);
      continue;
    }

    CGameWeaponStats *weapon = &stats.weapons[stats.numWeapons++];

    weapon->tag = tag;
    weapon->shots = shots;
    weapon->hits = hits;
    weapon->damage = damage;
    weapon->kills = kills;
  }

  stats.generation = cgameState.matchStats.generation;

  if (memcmp(&stats, &cgameState.matchStats, sizeof(stats))) {
    cgameState.matchStats = stats;
    cgameState.matchStats.generation = ++matchStatsGeneration;
  }
}

/**
 * @see cg_intermission.h
 */
void Cg_Intermission_Vote(int32_t map) {
  cgi.Cbuf(va("voteMap %d\n", map + 1));
}

/**
 * @brief Acts on the practice choice for the key that the player pressed, from 0, once the
 * match's result is final.
 * @return True if the key is a choice.
 */
static bool Cg_Intermission_Practice(int32_t choice) {

  if (choice < 0 || choice > 2) {
    return false;
  }

  if (Cg_Practice_Result() == -1) {
    return true;
  }

  switch (choice) {
    case 0:
      return Cg_Practice_PlayNext();
    case 1:
      Cg_Practice_Retry();
      return true;
    case 2:
      Cg_Practice_Exit();
      return true;
    default:
      return false;
  }
}

/**
 * @see cg_intermission.h
 */
bool Cg_Intermission_HandleEvent(const SDL_Event *event) {

  if (event->type != SDL_EVENT_KEY_DOWN) {
    return false;
  }

  if (!cgameState.nextMap.active || !(cgameState.nextMap.voting || cgameState.nextMap.practice)) {
    return false;
  }

  // the client hands us every event, so the console and the chat would otherwise
  // cast a ballot for anyone typing a digit
  if (cgi.GetKeyDest() != KEY_GAME) {
    return false;
  }

  if (cgameState.nextMap.practice) {
    return Cg_Intermission_Practice((int32_t) (event->key.key - SDLK_1));
  }

  const int32_t map = (int32_t) (event->key.key - SDLK_1);

  if (map < 0 || map >= cgameState.nextMap.numMaps) {
    return false;
  }

  Cg_Intermission_Vote(map);

  return true;
}

/**
 * @brief An intermission does not outlive its level.
 */
static void Cg_StateDidClear_Intermission(void) {

  memset(&cgameState.nextMap, 0, sizeof(cgameState.nextMap));

  memset(&cgameState.matchStats, 0, sizeof(cgameState.matchStats));
  cgameState.matchStats.generation = ++matchStatsGeneration;

  previous.StateDidClear();
}

/**
 * @brief Installs the intermission over the hooks it needs, once per module image.
 */
void Cg_Intermission_Init(void) {
  static bool installed;

  cgi.AddCmd("voteMap", NULL, CMD_CGAME, "Vote for a map during the intermission: voteMap <number>");

  if (installed) {
    return;
  }

  previous.ParseConfigString = Cg_ParseConfigString;
  Cg_ParseConfigString = Cg_ParseConfigString_Intermission;

  previous.StateDidClear = Cg_StateDidClear;
  Cg_StateDidClear = Cg_StateDidClear_Intermission;

  installed = true;
}
