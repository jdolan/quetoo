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

static struct {
  FilterMap FilterMap;
} previous;

/**
 * @brief The games this module plays as variants of deathmatch, which the maps name
 * rather than `lithium`.
 */
static const char *cg_lithium_games[] = { "dm", "tdm", "duel", "instagib" };

/**
 * @brief Lists a map made for any of the deathmatch variants.
 */
static bool Cg_FilterMap_Lithium(const char *mapname, const char *games) {

  for (size_t i = 0; i < lengthof(cg_lithium_games); i++) {
    if (Cg_HasGame(games, cg_lithium_games[i])) {
      return true;
    }
  }

  return previous.FilterMap(mapname, games);
}

/**
 * @brief Lithium draws the grappling hook and the techs, both of which are
 * features of the common sources that `Cg_Init` installs from the defines in this
 * module's Makefile.am. Its one hook of its own names the deathmatch variants in
 * the create-server map browser, since no map is made for `lithium`.
 */
void Cg_Module_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  previous.FilterMap = Cg_FilterMap;
  Cg_FilterMap = Cg_FilterMap_Lithium;

  installed = true;
}

/**
 * @brief Nothing of its own to release; the features own what they load.
 */
void Cg_Module_Shutdown(void) {
}
